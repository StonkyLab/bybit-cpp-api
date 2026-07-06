/**
Bybit Futures WebSocket Session

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.
*/

#include "stonky/bybit/bybit_ws_session.h"
#include <boost/asio/ssl/host_name_verification.hpp>
#include "stonky/utils/log_utils.h"
#include "stonky/utils/json_utils.h"
#include "stonky/utils/utils.h"
#include <nlohmann/json.hpp>
#include <boost/asio/buffers_iterator.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <atomic>
#include <list>
#include <map>
#include <fmt/ranges.h>

namespace stonky::bybit {
static constexpr int PING_INTERVAL_IN_S = 20;
/// Reconnect when no pong (JSON or protocol frame) arrived for this long.
static constexpr int PONG_TIMEOUT_IN_S = 3 * PING_INTERVAL_IN_S;
static constexpr int RECONNECT_DELAY_MAX_IN_S = 30;
/// Auth "expires" headroom. Bybit only requires expires > server now; a few
/// seconds absorbs clock skew and the connect round-trip.
static constexpr int AUTH_EXPIRES_OFFSET_IN_MS = 5000;
/// Reconnect when the auth ack did not arrive within this window (the docs
/// recommend treating a missing ack as a failure; pings alone keep an
/// unauthenticated connection "alive" but useless).
static constexpr int AUTH_ACK_TIMEOUT_IN_S = 15;

using WsStream = boost::beast::websocket::stream<boost::beast::ssl_stream<boost::beast::tcp_stream>>;

struct WebSocketSession::P {
    boost::asio::io_context &ioc;
    boost::asio::ssl::context &ctx;
    boost::asio::strand<boost::asio::io_context::executor_type> strand;
    boost::asio::ip::tcp::resolver resolver;
    /// Recreated on every (re)connect attempt — a Beast websocket stream is not
    /// reusable after a failed/closed connection. shared_ptr because completion
    /// handlers of generation N capture it; the object must outlive its pending
    /// ops even after generation N+1 replaced this member.
    std::shared_ptr<WsStream> ws;
    boost::beast::multi_buffer buffer;

    std::string host;
    std::string port;
    std::string path{"/v5/public/linear"};
    std::string hostHeader; /// host:port for the WS upgrade, rebuilt per connect

    std::string apiKey;
    std::string apiSecret;
    bool authRequired = false;

    onLogMessage logMessageCB;
    onDataEvent dataEventCB;

    /// Outbound write pump. One async_write in flight at most; everything
    /// (auth, subscribe, JSON ping) goes through the queue, so writes never
    /// depend on inbound traffic and never race each other.
    std::list<std::string> outboundQueue;
    bool writeInFlight = false;
    std::string writeBuffer; /// keeps the frame alive during async_write

    /// Topic bookkeeping. pendingTopics wait for connection (+ auth when
    /// required); once sent they move to subscriptions. On reconnect all
    /// subscriptions move back to pendingTopics and are replayed.
    std::vector<std::string> subscriptions;
    std::list<std::string> pendingTopics;
    /// req_id → topics of an unacked subscribe op. V5 failure acks carry no
    /// echo of the request, only the req_id — this map is how a rejected
    /// subscribe gets pruned from `subscriptions`.
    std::map<std::string, std::vector<std::string>> inflightSubscribes;
    std::uint64_t reqIdCounter = 0;
    mutable std::recursive_mutex locker;

    boost::asio::steady_timer pingTimer;
    boost::asio::steady_timer reconnectTimer;
    int reconnectDelayS = 1;
    /// Connection generation. Incremented by every startConnect; completion
    /// handlers capture their generation and bail out when it no longer
    /// matches, so a stale handler of a torn-down socket can neither mutate
    /// the new connection's state nor re-arm ops on the new stream.
    int generation = 0;
    bool connected = false; /// io-thread only: WS handshake completed
    bool authenticated = false; /// io-thread only
    std::atomic<bool> authenticatedFlag{false}; /// cross-thread mirror of `authenticated`
    bool reconnectScheduled = false; /// io-thread only: collapse same-generation error bursts
    bool userClosed = false; /// guarded by locker; checked on the io thread
    std::chrono::steady_clock::time_point lastPongTime{};
    std::chrono::steady_clock::time_point authDeadline{std::chrono::steady_clock::time_point::max()};

    P(boost::asio::io_context &ioContext, boost::asio::ssl::context &sslCtx, const onLogMessage &onLogMessageCB) :
        ioc(ioContext), ctx(sslCtx), strand(boost::asio::make_strand(ioContext)), resolver(strand), logMessageCB(onLogMessageCB), pingTimer(strand),
        reconnectTimer(strand) {}

    void log(const LogSeverity severity, const std::string &message) const {
        if (logMessageCB) {
            logMessageCB(severity, message);
        }
    }

    [[nodiscard]] bool isUserClosed() const {
        std::lock_guard lk(locker);
        return userClosed;
    }

    // ── Outbound write pump ─────────────────────────────────────────

    /// Queue a raw JSON payload. Call from the io thread (pump directly) or
    /// via subscribe() which posts the pump onto the strand.
    void enqueueOp(const std::string &payload) {
        std::lock_guard lk(locker);
        outboundQueue.push_back(payload);
    }

    /// Only called from current-generation contexts (every caller is either
    /// gen-checked or posted while this->ws is current).
    void pump(const std::shared_ptr<WebSocketSession> &self) {
        if (writeInFlight || !connected || !ws || !ws->is_open()) {
            return;
        }

        {
            std::lock_guard lk(locker);

            if (outboundQueue.empty()) {
                return;
            }

            writeBuffer = std::move(outboundQueue.front());
            outboundQueue.pop_front();
        }

        writeInFlight = true;
        ws->async_write(boost::asio::buffer(writeBuffer),
                        [this, self, gen = generation, wsRef = ws](const boost::beast::error_code &ec, const std::size_t bytesTransferred) {
                            boost::ignore_unused(bytesTransferred, wsRef);

                            if (gen != generation) {
                                return; /// stale completion of a torn-down connection
                            }

                            writeInFlight = false;

                            if (ec) {
                                return handleError(self, gen, fmt::format("{}: write: {}", MAKE_FILELINE, ec.message()));
                            }

                            pump(self);
                        });
    }

    // ── Topic bookkeeping ───────────────────────────────────────────

    void addTopic(const std::string &topic) {
        std::lock_guard lk(locker);

        if (std::ranges::find(subscriptions, topic) != subscriptions.end()) {
            return;
        }

        if (std::ranges::find(pendingTopics, topic) != pendingTopics.end()) {
            return;
        }

        pendingTopics.push_back(topic);
    }

    /// Remove a topic from both books. Returns true when its subscribe was
    /// already SENT (the venue must be told to unsubscribe).
    bool removeTopic(const std::string &topic) {
        std::lock_guard lk(locker);
        pendingTopics.remove(topic);

        if (const auto it = std::ranges::find(subscriptions, topic); it != subscriptions.end()) {
            subscriptions.erase(it);
            return true;
        }

        return false;
    }

    [[nodiscard]] bool isSubscribed(const std::string &subscriptionFilter) const {
        std::lock_guard lk(locker);

        if (std::ranges::find(subscriptions, subscriptionFilter) != subscriptions.end()) {
            return true;
        }

        return std::ranges::find(pendingTopics, subscriptionFilter) != pendingTopics.end();
    }

    /// Send all pending topics in one subscribe op. No-op while disconnected
    /// or awaiting the auth ack — called again from both completion paths.
    void flushTopics(const std::shared_ptr<WebSocketSession> &self) {
        if (!connected || (authRequired && !authenticated)) {
            return;
        }

        nlohmann::json subJson;
        {
            std::lock_guard lk(locker);

            if (pendingTopics.empty()) {
                return;
            }

            std::vector<std::string> args;

            for (auto &topic: pendingTopics) {
                args.push_back(topic);
                subscriptions.push_back(topic);
            }

            pendingTopics.clear();

            const auto reqId = std::to_string(++reqIdCounter);
            inflightSubscribes[reqId] = args;
            subJson["op"] = "subscribe";
            subJson["args"] = args;
            subJson["req_id"] = reqId;
        }

        enqueueOp(subJson.dump());
        pump(self);
    }

    // ── Auth ────────────────────────────────────────────────────────

    void sendAuth(const std::shared_ptr<WebSocketSession> &self) {
        const auto expires = getMsTimestamp(currentTime()).count() + AUTH_EXPIRES_OFFSET_IN_MS;
        const std::string payload = fmt::format("GET/realtime{}", expires);

        unsigned char digest[SHA256_DIGEST_LENGTH];
        unsigned int digestLength = SHA256_DIGEST_LENGTH;

        HMAC(EVP_sha256(), apiSecret.data(), static_cast<int>(apiSecret.size()), reinterpret_cast<const unsigned char *>(payload.data()), payload.length(), digest,
             &digestLength);

        nlohmann::json authJson;
        authJson["op"] = "auth";
        authJson["args"] = nlohmann::json::array({apiKey, expires, stringToHex(digest, sizeof(digest))});

        authDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(AUTH_ACK_TIMEOUT_IN_S);
        enqueueOp(authJson.dump());
        pump(self);
    }

    // ── Connect chain ───────────────────────────────────────────────

    void startConnect(const std::shared_ptr<WebSocketSession> &self) {
        if (isUserClosed()) {
            return;
        }

        ++generation;
        const int gen = generation;

        connected = false;
        authenticated = false;
        authenticatedFlag = false;
        writeInFlight = false;
        authDeadline = std::chrono::steady_clock::time_point::max();
        hostHeader = host;
        buffer.consume(buffer.size());
        ws = std::make_shared<WsStream>(strand, ctx);
        // Bind the verified peer certificate to the expected hostname (the ctx
        // has verify_peer on; SNI alone authenticates nothing).
        ws->next_layer().set_verify_callback(boost::asio::ssl::host_name_verification(host));

        resolver.async_resolve(host, port, [this, self, gen](const boost::beast::error_code &ec, const boost::asio::ip::tcp::resolver::results_type &results) {
            if (gen != generation) {
                return;
            }

            onResolve(self, gen, ec, results);
        });
    }

    void onResolve(const std::shared_ptr<WebSocketSession> &self, const int gen, const boost::beast::error_code &ec,
                   const boost::asio::ip::tcp::resolver::results_type &results) {
        if (ec) {
            return handleError(self, gen, fmt::format("{}: {}", MAKE_FILELINE, ec.message()));
        }

        get_lowest_layer(*ws).expires_after(std::chrono::seconds(30));

        get_lowest_layer(*ws).async_connect(results, [this, self, gen, wsRef = ws](const boost::beast::error_code &e,
                                                                                   const boost::asio::ip::tcp::resolver::results_type::endpoint_type &ep) {
            boost::ignore_unused(wsRef);

            if (gen != generation) {
                return;
            }

            onConnect(self, gen, e, ep);
        });
    }

    void onConnect(const std::shared_ptr<WebSocketSession> &self, const int gen, boost::beast::error_code ec,
                   const boost::asio::ip::tcp::resolver::results_type::endpoint_type &ep) {
        if (ec) {
            return handleError(self, gen, fmt::format("{}: {}", MAKE_FILELINE, ec.message()));
        }

        get_lowest_layer(*ws).expires_after(std::chrono::seconds(30));

        if (!SSL_set_tlsext_host_name(ws->next_layer().native_handle(), host.c_str())) {
            ec = boost::beast::error_code(static_cast<int>(ERR_get_error()), boost::asio::error::get_ssl_category());
            return handleError(self, gen, fmt::format("{}: {}", MAKE_FILELINE, ec.message()));
        }

        hostHeader = host + ':' + std::to_string(ep.port());

        ws->next_layer().async_handshake(boost::asio::ssl::stream_base::client, [this, self, gen, wsRef = ws](const boost::beast::error_code &e) {
            boost::ignore_unused(wsRef);

            if (gen != generation) {
                return;
            }

            onSSLHandshake(self, gen, e);
        });
    }

    void onSSLHandshake(const std::shared_ptr<WebSocketSession> &self, const int gen, const boost::beast::error_code &ec) {
        if (ec) {
            return handleError(self, gen, fmt::format("{}: {}", MAKE_FILELINE, ec.message()));
        }

        ws->control_callback([this](const boost::beast::websocket::frame_type kind, boost::beast::string_view payload) {
            boost::ignore_unused(payload);

            if (kind == boost::beast::websocket::frame_type::pong) {
                lastPongTime = std::chrono::steady_clock::now();
            }
        });

        get_lowest_layer(*ws).expires_never();

        ws->set_option(boost::beast::websocket::stream_base::timeout::suggested(boost::beast::role_type::client));

        ws->set_option(boost::beast::websocket::stream_base::decorator(
                [](boost::beast::websocket::request_type &req) { req.set(boost::beast::http::field::user_agent, std::string(BOOST_BEAST_VERSION_STRING) + " bybit-client"); }));

        ws->async_handshake(hostHeader, path, [this, self, gen, wsRef = ws](const boost::beast::error_code &e) {
            boost::ignore_unused(wsRef);

            if (gen != generation) {
                return;
            }

            onHandshake(self, gen, e);
        });
    }

    void onHandshake(const std::shared_ptr<WebSocketSession> &self, const int gen, const boost::beast::error_code &ec) {
        if (ec) {
            return handleError(self, gen, fmt::format("{}: {}", MAKE_FILELINE, ec.message()));
        }

        if (isUserClosed()) {
            return;
        }

        connected = true;
        lastPongTime = std::chrono::steady_clock::now();

        /// NOTE: the reconnect backoff is deliberately NOT reset here. A wrong
        /// API key passes the WS handshake and fails only at auth — resetting
        /// on handshake would turn that into a 1 s connect storm. The reset
        /// happens on the first successfully processed frame of an operational
        /// connection (see onRead).

        pingTimer.expires_after(boost::asio::chrono::seconds(PING_INTERVAL_IN_S));
        pingTimer.async_wait([this, self, gen](const boost::beast::error_code &e) { onPingTimer(self, gen, e); });

        if (authRequired) {
            sendAuth(self);
        } else {
            flushTopics(self);
        }

        ws->async_read(buffer, [this, self, gen, wsRef = ws](const boost::beast::error_code &e, const std::size_t transferred) {
            boost::ignore_unused(wsRef);

            if (gen != generation) {
                return;
            }

            onRead(self, gen, e, transferred);
        });
    }

    // ── Inbound ─────────────────────────────────────────────────────

    void onRead(const std::shared_ptr<WebSocketSession> &self, const int gen, const boost::beast::error_code &ec, std::size_t bytesTransferred) {
        boost::ignore_unused(bytesTransferred);

        if (ec) {
            return handleError(self, gen, fmt::format("{}: {}", MAKE_FILELINE, ec.message()));
        }

        try {
            const auto size = buffer.size();
            std::string strBuffer;
            strBuffer.reserve(size);

            for (const auto &it: buffer.data()) {
                strBuffer.append(static_cast<const char *>(it.data()), it.size());
            }

            buffer.consume(buffer.size());

            if (const nlohmann::json json = nlohmann::json::parse(strBuffer); json.is_object()) {
                if (isApiControlMsg(json)) {
                    handleApiControlMsg(self, gen, json);
                } else {
                    try {
                        Event dataEvent;
                        dataEvent.fromJson(json);

                        if (dataEventCB) {
                            dataEventCB(dataEvent);
                        }
                    } catch (std::exception &e) {
                        log(LogSeverity::Error, fmt::format("{}: {}", MAKE_FILELINE, e.what()));
                    }
                }
            }

            /// handleApiControlMsg may have torn the connection down (auth
            /// failure) — do not re-arm the read on a socket being recycled.
            if (gen != generation || reconnectScheduled || !connected) {
                return;
            }

            /// First successfully processed frame of an operational connection
            /// (public: any frame; private: only once authenticated) proves the
            /// endpoint works — reset the reconnect backoff.
            if (!authRequired || authenticated) {
                reconnectDelayS = 1;
            }

            ws->async_read(buffer, [this, self, gen, wsRef = ws](const boost::beast::error_code &e, const std::size_t transferred) {
                boost::ignore_unused(wsRef);

                if (gen != generation) {
                    return;
                }

                onRead(self, gen, e, transferred);
            });
        } catch (nlohmann::json::exception &exc) {
            handleError(self, gen, fmt::format("{}: {}", MAKE_FILELINE, exc.what()));
        }
    }

    /// Control message = op ack (auth/subscribe/pong), never a topic push.
    /// Public acks carry "success"; the private pong has only "op":"pong".
    static bool isApiControlMsg(const nlohmann::json &json) {
        return (json.contains("success") || json.contains("op")) && !json.contains("topic");
    }

    void handleApiControlMsg(const std::shared_ptr<WebSocketSession> &self, const int gen, const nlohmann::json &json) {
        std::string operation;
        readValue<std::string>(json, "op", operation);

        bool success = true;
        readValue<bool>(json, "success", success);

        std::string retMsg;
        readValue<std::string>(json, "ret_msg", retMsg);

        /// Pong: public replies {"op":"ping","success":true,"ret_msg":"pong"},
        /// private replies {"op":"pong","args":[...]} without "success".
        if (operation == "pong" || retMsg == "pong") {
            lastPongTime = std::chrono::steady_clock::now();
            return;
        }

        if (operation == "auth") {
            if (success) {
                authenticated = true;
                authenticatedFlag = true;
                authDeadline = std::chrono::steady_clock::time_point::max();
                reconnectDelayS = 1;
                log(LogSeverity::Info, "Bybit WS authenticated");
                flushTopics(self);
            } else {
                /// A fresh expires/signature is generated on the next attempt.
                /// The backoff keeps growing (no reset until auth succeeds), so
                /// a bad key degrades to one attempt per 30 s, not a storm.
                handleError(self, gen, fmt::format("Bybit WS auth failed: {}", retMsg));
            }
            return;
        }

        if (operation == "subscribe") {
            std::string reqId;
            readValue<std::string>(json, "req_id", reqId);

            std::lock_guard lk(locker);

            if (const auto it = inflightSubscribes.find(reqId); it != inflightSubscribes.end()) {
                if (!success) {
                    /// V5 failure acks echo nothing but req_id — prune the whole
                    /// op's topics so isSubscribed() reflects reality and the
                    /// dead topics are not replayed on every reconnect.
                    for (const auto &topic: it->second) {
                        if (auto subIt = std::ranges::find(subscriptions, topic); subIt != subscriptions.end()) {
                            subscriptions.erase(subIt);
                        }
                    }

                    log(LogSeverity::Error, fmt::format("Bybit subscribe failed ({}): {}", fmt::join(it->second, ", "), retMsg));
                }

                inflightSubscribes.erase(it);
            }

            if (!success && reqId.empty()) {
                log(LogSeverity::Error, fmt::format("Bybit API Error, operation: {}, message: {}", operation, retMsg));
            }

            return;
        }

        if (!success) {
            log(LogSeverity::Error, fmt::format("Bybit API Error, operation: {}, message: {}", operation, retMsg));
            return;
        }

#ifdef VERBOSE_LOG
        log(LogSeverity::Info, fmt::format("Bybit API control msg: {}", json.dump()));
#endif
    }

    // ── Heartbeat ───────────────────────────────────────────────────

    void onPingTimer(const std::shared_ptr<WebSocketSession> &self, const int gen, const boost::beast::error_code &ec) {
        if (ec || gen != generation) {
            /// Cancelled or superseded by a newer connection — do not re-arm.
            return;
        }

        if (isUserClosed()) {
            return;
        }

        if (connected) {
            const auto now = std::chrono::steady_clock::now();

            if (const auto sincePong = std::chrono::duration_cast<std::chrono::seconds>(now - lastPongTime).count(); sincePong > PONG_TIMEOUT_IN_S) {
                return handleError(self, gen, fmt::format("{}: no pong for {} s", MAKE_FILELINE, sincePong));
            }

            if (authRequired && !authenticated && now > authDeadline) {
                return handleError(self, gen, fmt::format("{}: auth ack timeout", MAKE_FILELINE));
            }

            /// JSON heartbeat per Bybit V5 docs. Its pong is also the wake-up
            /// that keeps the read loop live on a quiet private stream.
            enqueueOp(nlohmann::json{{"op", "ping"}}.dump());
            pump(self);
        }

        pingTimer.expires_after(boost::asio::chrono::seconds(PING_INTERVAL_IN_S));
        pingTimer.async_wait([this, self, gen](const boost::beast::error_code &e) { onPingTimer(self, gen, e); });
    }

    // ── Error / reconnect / close ───────────────────────────────────

    void handleError(const std::shared_ptr<WebSocketSession> &self, const int gen, const std::string &message) {
        if (gen != generation || reconnectScheduled) {
            return; /// stale generation, or several ops failing off one broken socket
        }

        log(LogSeverity::Error, message);

        if (isUserClosed()) {
            return;
        }

        reconnectScheduled = true;
        connected = false;
        authenticated = false;
        authenticatedFlag = false;
        writeInFlight = false;
        authDeadline = std::chrono::steady_clock::time_point::max();
        pingTimer.cancel();

        {
            std::lock_guard lk(locker);
            outboundQueue.clear();
            inflightSubscribes.clear();

            /// Replay every confirmed topic after the reconnect.
            for (auto it = subscriptions.rbegin(); it != subscriptions.rend(); ++it) {
                pendingTopics.push_front(*it);
            }

            subscriptions.clear();
        }

        /// Abort any outstanding ops; their completion handlers are
        /// generation-guarded, so they cannot touch the next connection.
        if (ws) {
            boost::beast::error_code ignored;
            get_lowest_layer(*ws).socket().close(ignored);
        }

        log(LogSeverity::Warning, fmt::format("Bybit WS reconnecting in {} s", reconnectDelayS));
        reconnectTimer.expires_after(boost::asio::chrono::seconds(reconnectDelayS));
        reconnectDelayS = std::min(reconnectDelayS * 2, RECONNECT_DELAY_MAX_IN_S);

        reconnectTimer.async_wait([this, self, gen](const boost::beast::error_code &e) {
            if (gen != generation) {
                return;
            }

            reconnectScheduled = false;

            if (e || isUserClosed()) {
                return;
            }

            startConnect(self);
        });
    }

    void closeByUser() {
        {
            std::lock_guard lk(locker);

            if (userClosed) {
                return; /// double close
            }

            userClosed = true;
        }

        pingTimer.cancel();
        reconnectTimer.cancel();
        resolver.cancel();

        if (!ws) {
            return;
        }

        if (ws->is_open() && !writeInFlight) {
            ws->async_close(boost::beast::websocket::close_code::normal, [this, wsRef = ws](const boost::beast::error_code &ec) {
                boost::ignore_unused(wsRef);

                if (ec) {
                    log(LogSeverity::Error, fmt::format("{}: {}", MAKE_FILELINE, ec.message()));
                }
            });
        } else {
            /// Mid-connect or a write in flight — async_close is either invalid
            /// or would violate Beast's single-writer rule; drop the transport.
            boost::beast::error_code ignored;
            get_lowest_layer(*ws).socket().close(ignored);
        }
    }
};

WebSocketSession::WebSocketSession(boost::asio::io_context &ioc, boost::asio::ssl::context &ctx, const onLogMessage &onLogMessageCB) :
    m_p(std::make_unique<P>(ioc, ctx, onLogMessageCB)) {}

WebSocketSession::~WebSocketSession() {
#ifdef VERBOSE_LOG
    m_p->log(LogSeverity::Info, "WebSocketSession destroyed");
#endif
}

void WebSocketSession::setCredentials(const std::string &apiKey, const std::string &apiSecret) const {
    m_p->apiKey = apiKey;
    m_p->apiSecret = apiSecret;
    m_p->authRequired = !apiKey.empty() && !apiSecret.empty();
}

void WebSocketSession::subscribe(const std::string &subscriptionFilter) const {
    m_p->addTopic(subscriptionFilter);

    /// flushTopics must run on the strand; it no-ops until connected+authed.
    auto self = const_cast<WebSocketSession *>(this)->shared_from_this();
    post(m_p->strand, [self] { self->m_p->flushTopics(self); });
}

void WebSocketSession::unsubscribe(const std::string &subscriptionFilter) const {
    if (!m_p->removeTopic(subscriptionFilter)) {
        return; /// never sent — the venue does not know about it
    }

    auto self = const_cast<WebSocketSession *>(this)->shared_from_this();
    post(m_p->strand, [self, subscriptionFilter] {
        if (!self->m_p->connected) {
            return; /// removed from the replay set — the reconnect won't resubscribe
        }

        nlohmann::json unsubJson;
        unsubJson["op"] = "unsubscribe";
        unsubJson["args"] = std::vector{subscriptionFilter};
        self->m_p->enqueueOp(unsubJson.dump());
        self->m_p->pump(self);
    });
}

bool WebSocketSession::isAuthenticated() const { return m_p->authenticatedFlag.load(); }

bool WebSocketSession::isSubscribed(const std::string &subscriptionFilter) const { return m_p->isSubscribed(subscriptionFilter); }

void WebSocketSession::run(const std::string &host, const std::string &port, const std::string &path, const std::string &subscriptionFilter,
                           const onDataEvent &dataEventCB) {
    if (subscriptionFilter.empty()) {
        throw std::runtime_error("SubscriptionFilter cannot be empty");
    }

    m_p->host = host;
    m_p->port = port;
    m_p->path = path;
    m_p->dataEventCB = dataEventCB;
    m_p->addTopic(subscriptionFilter);

    auto self = shared_from_this();
    post(m_p->strand, [self] { self->m_p->startConnect(self); });
}

void WebSocketSession::close() const {
    auto self = const_cast<WebSocketSession *>(this)->shared_from_this();
    post(m_p->strand, [self] { self->m_p->closeByUser(); });
}
} // namespace stonky::bybit
