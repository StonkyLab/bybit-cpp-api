/**
Bybit Futures WebSocket Session

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@gmail.com>.
*/

#include "vk/bybit/bybit_ws_session.h"
#include "vk/utils/log_utils.h"
#include "vk/utils/json_utils.h"
#include <nlohmann/json.hpp>
#include <boost/asio/buffers_iterator.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <list>

namespace vk::bybit {
static constexpr int PING_INTERVAL_IN_S = 20;

struct WebSocketSession::P {
    boost::asio::ip::tcp::resolver resolver;
    boost::beast::websocket::stream<boost::beast::ssl_stream<boost::beast::tcp_stream>> ws;
    boost::beast::multi_buffer buffer;
    std::string host;
    std::vector<std::string> subscriptions;
    std::list<std::string> subscriptionRequests;
    onLogMessage logMessageCB;
    onDataEvent dataEventCB;
    boost::asio::steady_timer pingTimer;
    std::chrono::time_point<std::chrono::system_clock> lastPingTime{};
    std::chrono::time_point<std::chrono::system_clock> lastPongTime{};
    mutable std::recursive_mutex subscriptionLocker;

    P(boost::asio::io_context &ioc, boost::asio::ssl::context &ctx, const onLogMessage &onLogMessageCB) :
        resolver(make_strand(ioc)), ws(make_strand(ioc), ctx), logMessageCB(onLogMessageCB), pingTimer(ioc, boost::asio::chrono::seconds(PING_INTERVAL_IN_S)) {}

    void writeSubscription(const std::string &subscription) {
        std::lock_guard lk(subscriptionLocker);

        for (const auto &filter: subscriptions) {
            if (filter == subscription) {
                return;
            }
        }

        nlohmann::json subJson;

        std::vector<std::string> args;
        args.push_back(subscription);

        subJson["op"] = "subscribe";
        subJson["args"] = args;

        subscriptionRequests.push_back(subJson.dump());
    }

    std::string readSubscription() {
        std::lock_guard lk(subscriptionLocker);
        std::string retVal;

        if (subscriptionRequests.empty()) {
            return "";
        }

        try {
            nlohmann::json subJson = nlohmann::json::parse(subscriptionRequests.front());

            for (const auto &argsJson = subJson["args"]; const auto &arg: argsJson) {
                subscriptions.push_back(arg);
            }

            retVal = subscriptionRequests.front();
        } catch (std::exception &e) {
            logMessageCB(LogSeverity::Error, fmt::format("{}: {}", MAKE_FILELINE, e.what()));
        }

        subscriptionRequests.pop_front();
        return retVal;
    }

    static bool isApiControlMsg(const nlohmann::json &json) {
        if (json.contains("success")) {
            return true;
        }

        return false;
    }

    void handleApiControlMsg(const nlohmann::json &json) {
        std::lock_guard lk(subscriptionLocker);
        bool isError = false;

        if (json.contains("success")) {
            isError = !json["success"];
        }

        if (json.contains("request") && isError) {
            std::string operation;
            const auto &requestJson = json["request"];
            readValue<std::string>(requestJson, "op", operation);

            for (const auto &argsJson = requestJson["args"]; const std::string arg: argsJson) {
                if (auto it = std::ranges::find(subscriptions, arg); it != subscriptions.end()) {
                    subscriptions.erase(it);
                }
            }

            std::string errorMsg;
            readValue<std::string>(json, "ret_msg", errorMsg);
            logMessageCB(LogSeverity::Error, fmt::format("Bybit API Error, operation: {}, message: {}", operation, errorMsg));
        }
#ifdef VERBOSE_LOG
        logMessageCB(LogSeverity::Info, fmt::format("Bybit API control msg: {}", json.dump()));
#endif
    }

    [[nodiscard]] bool isSubscribed(const std::string &subscriptionFilter) const {
        std::lock_guard lk(subscriptionLocker);

        if (const auto it = std::ranges::find(subscriptions, subscriptionFilter); it != subscriptions.end()) {
            return true;
        }

        return false;
    }

    void onResolve(const std::shared_ptr<WebSocketSession> &self, const boost::beast::error_code &ec, const boost::asio::ip::tcp::resolver::results_type &results) {
        if (ec) {
            return logMessageCB(LogSeverity::Error, fmt::format("{}: {}", MAKE_FILELINE, ec.message()));
        }

        get_lowest_layer(ws).expires_after(std::chrono::seconds(30));

        get_lowest_layer(ws).async_connect(
                results, [this, self](const boost::beast::error_code &e, const boost::asio::ip::tcp::resolver::results_type::endpoint_type &ep) { onConnect(self, e, ep); });
    }

    void onConnect(const std::shared_ptr<WebSocketSession> &self, boost::beast::error_code ec, const boost::asio::ip::tcp::resolver::results_type::endpoint_type &ep) {
        if (ec) {
            return logMessageCB(LogSeverity::Error, fmt::format("{}: {}", MAKE_FILELINE, ec.message()));
        }

        get_lowest_layer(ws).expires_after(std::chrono::seconds(30));

        if (!SSL_set_tlsext_host_name(ws.next_layer().native_handle(), host.c_str())) {
            ec = boost::beast::error_code(static_cast<int>(ERR_get_error()), boost::asio::error::get_ssl_category());
            return logMessageCB(LogSeverity::Error, fmt::format("{}: {}", MAKE_FILELINE, ec.message()));
        }

        host += ':' + std::to_string(ep.port());

        ws.next_layer().async_handshake(boost::asio::ssl::stream_base::client, [this, self](const boost::beast::error_code &e) { onSSLHandshake(self, e); });
    }

    void onSSLHandshake(const std::shared_ptr<WebSocketSession> &self, const boost::beast::error_code &ec) {
        if (ec) {
            return logMessageCB(LogSeverity::Error, fmt::format("{}: {}", MAKE_FILELINE, ec.message()));
        }

        ws.control_callback([this](boost::beast::websocket::frame_type kind, boost::beast::string_view payload) {
            boost::ignore_unused(kind, payload);

            if (kind == boost::beast::websocket::frame_type::pong) {
                lastPongTime = std::chrono::system_clock::now();
            }
        });

        get_lowest_layer(ws).expires_never();

        ws.set_option(boost::beast::websocket::stream_base::timeout::suggested(boost::beast::role_type::client));

        ws.set_option(boost::beast::websocket::stream_base::decorator(
                [](boost::beast::websocket::request_type &req) { req.set(boost::beast::http::field::user_agent, std::string(BOOST_BEAST_VERSION_STRING) + " bybit-client"); }));

        ws.async_handshake(host, "/v5/public/linear", [this, self](const boost::beast::error_code &e) { onHandshake(self, e); });
    }

    void onHandshake(const std::shared_ptr<WebSocketSession> &self, const boost::beast::error_code &ec) {
        if (ec) {
            return logMessageCB(LogSeverity::Error, fmt::format("{}: {}", MAKE_FILELINE, ec.message()));
        }

        pingTimer.async_wait([this, self](const boost::beast::error_code &e) { onPingTimer(self, e); });

        ws.async_write(boost::asio::buffer(readSubscription()),
                       [this, self](const boost::beast::error_code &e, const std::size_t bytesTransferred) { onWrite(self, e, bytesTransferred); });
    }

    void onWrite(const std::shared_ptr<WebSocketSession> &self, const boost::beast::error_code &ec, std::size_t bytesTransferred) {
        boost::ignore_unused(bytesTransferred);

        if (ec) {
            return logMessageCB(LogSeverity::Error, fmt::format("{}: {}", MAKE_FILELINE, ec.message()));
        }

        ws.async_read(buffer, [this, self](const boost::beast::error_code &e, const std::size_t transferred) { onRead(self, e, transferred); });
    }

    void onRead(const std::shared_ptr<WebSocketSession> &self, const boost::beast::error_code &ec, std::size_t bytesTransferred) {
        boost::ignore_unused(bytesTransferred);

        if (ec) {
            pingTimer.cancel();
            return logMessageCB(LogSeverity::Error, fmt::format("{}: {}", MAKE_FILELINE, ec.message()));
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
                    handleApiControlMsg(json);
                } else {
                    try {
                        Event dataEvent;
                        dataEvent.fromJson(json);

                        if (dataEventCB) {
                            dataEventCB(dataEvent);
                        }
                    } catch (std::exception &e) {
                        logMessageCB(LogSeverity::Error, fmt::format("{}: {}", MAKE_FILELINE, e.what()));
                    }
                }
            }

            if (const auto subscription = readSubscription(); !subscription.empty()) {
                ws.async_write(boost::asio::buffer(subscription),
                               [this, self](const boost::beast::error_code &e, const std::size_t transferred) { onWrite(self, e, transferred); });
            } else {
                std::lock_guard lk(subscriptionLocker);
                if (subscriptions.empty()) {
                    logMessageCB(LogSeverity::Warning, fmt::format("No subscriptions, WebSocketSession quit: {}", MAKE_FILELINE));
                    closeWs();
                }
                ws.async_read(buffer, [this, self](const boost::beast::error_code &e, const std::size_t transferred) { onRead(self, e, transferred); });
            }
        } catch (nlohmann::json::exception &exc) {
            logMessageCB(LogSeverity::Error, fmt::format("{}: {}", MAKE_FILELINE, exc.what()));
            ws.async_close(boost::beast::websocket::close_code::normal, [this](const boost::beast::error_code &e) { onClose(e); });
        }
    }

    void ping() {
        if (const std::chrono::duration<double> elapsed = lastPingTime - lastPongTime; elapsed.count() > PING_INTERVAL_IN_S) {
            logMessageCB(LogSeverity::Warning, fmt::format("{}: {}", MAKE_FILELINE, "ping expired"));
        }

        if (ws.is_open()) {
            constexpr boost::beast::websocket::ping_data pingWebSocketFrame;
            ws.async_ping(pingWebSocketFrame, [this](const boost::beast::error_code &ec) {
                if (ec) {
                    logMessageCB(LogSeverity::Error, fmt::format("{}: {}", MAKE_FILELINE, ec.message()));
                } else {
                    lastPingTime = std::chrono::system_clock::now();
                }
            });
        }
    }

    void closeWs() {
        ws.async_close(boost::beast::websocket::close_code::normal, [this](const boost::beast::error_code &ec) { onClose(ec); });
    }

    void onClose(const boost::beast::error_code &ec) {
        pingTimer.cancel();

        if (ec) {
            return logMessageCB(LogSeverity::Error, fmt::format("{}: {}", MAKE_FILELINE, ec.message()));
        }
    }

    void onPingTimer(const std::shared_ptr<WebSocketSession> &self, const boost::beast::error_code &ec) {
        if (ec) {
            return logMessageCB(LogSeverity::Error, fmt::format("{}: {}", MAKE_FILELINE, ec.message()));
        }

        ping();
        pingTimer.expires_after(boost::asio::chrono::seconds(PING_INTERVAL_IN_S));
        pingTimer.async_wait([this, self](const boost::beast::error_code &e) { onPingTimer(self, e); });
    }
};

WebSocketSession::WebSocketSession(boost::asio::io_context &ioc, boost::asio::ssl::context &ctx, const onLogMessage &onLogMessageCB) :
    m_p(std::make_unique<P>(ioc, ctx, onLogMessageCB)) {}

WebSocketSession::~WebSocketSession() {
    m_p->pingTimer.cancel();

#ifdef VERBOSE_LOG
    m_p->logMessageCB(LogSeverity::Info, "WebSocketSession destroyed");
#endif
}

void WebSocketSession::subscribe(const std::string &subscriptionFilter) const { m_p->writeSubscription(subscriptionFilter); }

bool WebSocketSession::isSubscribed(const std::string &subscriptionFilter) const { return m_p->isSubscribed(subscriptionFilter); }

void WebSocketSession::run(const std::string &host, const std::string &port, const std::string &subscriptionFilter, const onDataEvent &dataEventCB) {
    if (subscriptionFilter.empty()) {
        throw std::runtime_error("SubscriptionFilter cannot be empty");
    }

    m_p->host = host;
    m_p->writeSubscription(subscriptionFilter);
    m_p->dataEventCB = dataEventCB;

    auto self = shared_from_this();
    m_p->resolver.async_resolve(
            host, port, [this, self](const boost::beast::error_code &ec, const boost::asio::ip::tcp::resolver::results_type &results) { m_p->onResolve(self, ec, results); });
}

void WebSocketSession::close() const { m_p->closeWs(); }
} // namespace vk::bybit
