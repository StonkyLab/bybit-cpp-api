/**
Bybit HTTPS Session

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.
*/

#include "stonky/bybit/bybit_http_session.h"
#include "stonky/bybit/tls_verify.h"
#include "stonky/utils/utils.h"
#include "stonky/utils/json_utils.h"
#include "nlohmann/json.hpp"
#include <boost/asio/ssl.hpp>
#include <boost/asio/ssl/host_name_verification.hpp>
#include <boost/beast/version.hpp>
#include <openssl/hmac.h>
#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <atomic>
#include <limits>

namespace stonky::bybit {
namespace ssl = boost::asio::ssl;
using tcp = net::ip::tcp;

auto API_MAINNET_URI = "api.bybit.com";
auto API_TESTNET_URI = "api-testnet.bybit.com";

/// How often the local clock is re-synchronized against the exchange clock
static constexpr std::int64_t TIME_SYNC_INTERVAL_MS = 30 * 60 * 1000;

/// Stall timeout for every blocking socket syscall (connect, TLS handshake,
/// write, each read). Without it a dead peer/black-holed route blocks the
/// calling worker for the OS default (minutes) — past funding cutoffs and
/// scheduler slots the chase deadlines are supposed to protect. Applied per
/// syscall, so a large-but-flowing archive download never trips it; only a
/// genuine stall does. Timeouts surface as boost system_errors inside the
/// request try-block → TransportError (outcome unknown), which is exactly
/// right: a timed-out order POST may still have been executed.
static constexpr int IO_STALL_TIMEOUT_S = 15;

namespace {
void setSocketStallTimeouts(tcp::socket& socket) {
    const timeval tv{.tv_sec = IO_STALL_TIMEOUT_S, .tv_usec = 0};
    ::setsockopt(socket.native_handle(), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    ::setsockopt(socket.native_handle(), SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
}
} // namespace

struct HTTPSession::P {
    net::io_context ioc;
    std::string apiKey;
    int receiveWindow = 25000;
    std::string apiSecret;
    std::string uri;
    const EVP_MD* evpMd;

    /// Difference between the exchange clock and the local clock. Signed requests are rejected once the local clock
    /// drifts more than recv_window away from the server, so the timestamps are corrected by this offset.
    mutable std::atomic<std::int64_t> timeOffsetMs{0};
    mutable std::atomic<std::int64_t> lastTimeSyncMs{0};
    const HTTPSession* parent{nullptr};

    P() : evpMd(EVP_sha256()) {}

    http::response<http::string_body> request(http::request<http::string_body> req);

    /// Local clock corrected by the measured exchange offset
    [[nodiscard]] std::int64_t signingTimestamp() const {
        ensureTimeSync();
        return getMsTimestamp(currentTime()).count() + timeOffsetMs.load();
    }

    void ensureTimeSync() const;

    static std::string createQueryStr(const std::map<std::string, std::string>& parameters) {
        std::string queryStr;

        for (const auto& [fst, snd]: parameters) {
            queryStr.append(fst);
            queryStr.append("=");
            queryStr.append(snd);
            queryStr.append("&");
        }

        if (!queryStr.empty()) {
            queryStr.pop_back();
        }
        return queryStr;
    }

    void authenticatePost(http::request<http::string_body>& req, const nlohmann::json& json) const {
        const auto ts = signingTimestamp();

        nlohmann::json extendedJson = json;
        extendedJson["timestamp"] = ts;
        extendedJson["recv_window"] = receiveWindow;
        extendedJson["api_key"] = apiKey;

        const std::string queryString = queryStringFromJson(extendedJson);

        unsigned char digest[SHA256_DIGEST_LENGTH];
        unsigned int digestLength = SHA256_DIGEST_LENGTH;

        HMAC(evpMd, apiSecret.data(), static_cast<int>(apiSecret.size()), reinterpret_cast<const unsigned char*>(queryString.data()), queryString.length(), digest, &digestLength);

        std::string signature = stringToHex(digest, sizeof(digest));

        extendedJson["sign"] = signature;

        req.body() = extendedJson.dump();
        req.prepare_payload();

        req.set(http::field::content_type, "application/json");
    }

    void authenticateNonPost(http::request<http::string_body>& req) const {
        if (apiKey.empty()) return;
        std::string path(req.target());
        const std::size_t pos = path.find('?');
        std::string queryString;

        if (pos != std::string::npos) {
            queryString = path.substr(pos + 1);
        }

        std::string parameterString;

        const auto ts = signingTimestamp();
        parameterString.append(std::to_string(ts));
        parameterString.append(apiKey);
        parameterString.append(std::to_string(receiveWindow));
        parameterString.append(queryString);

        unsigned char digest[SHA256_DIGEST_LENGTH];
        unsigned int digestLength = SHA256_DIGEST_LENGTH;

        HMAC(evpMd, apiSecret.data(), static_cast<int>(apiSecret.size()), reinterpret_cast<const unsigned char*>(parameterString.data()), parameterString.length(), digest,
             &digestLength);

        const std::string signature = stringToHex(digest, sizeof(digest));

        req.set("X-BAPI-API-KEY", apiKey);
        req.set("X-BAPI-SIGN", signature);
        req.set("X-BAPI-SIGN-TYPE", "2");
        req.set("X-BAPI-TIMESTAMP", std::to_string(ts));
        req.set("X-BAPI-RECV-WINDOW", std::to_string(receiveWindow));
    }
};

HTTPSession::HTTPSession(const std::string& apiKey, const std::string& apiSecret, const std::string& host) : m_p(std::make_unique<P>()) {
    m_p->parent = this;
    m_p->uri = host.empty() ? API_MAINNET_URI : host;
    m_p->apiKey = apiKey;
    m_p->apiSecret = apiSecret;
}

HTTPSession::~HTTPSession() = default;

http::response<http::string_body> HTTPSession::get(const std::string& path, const std::map<std::string, std::string>& parameters) const {
    std::string finalPath = path;

    if (const auto queryString = P::createQueryStr(parameters); !queryString.empty()) {
        finalPath.append("?");
        finalPath.append(queryString);
    }

    http::request<http::string_body> req{http::verb::get, finalPath, 11};
    m_p->authenticateNonPost(req);
    return m_p->request(req);
}

http::response<http::string_body> HTTPSession::post(const std::string& path, const nlohmann::json& json) const {
    http::request<http::string_body> req{http::verb::post, path, 11};
    m_p->authenticatePost(req, json);
    return m_p->request(req);
}

void HTTPSession::P::ensureTimeSync() const {
    const auto now = getMsTimestamp(currentTime()).count();

    if (lastTimeSyncMs != 0 && now - lastTimeSyncMs < TIME_SYNC_INTERVAL_MS) {
        return;
    }

    /// Set upfront so that a failing endpoint is not hammered on every single signed request
    lastTimeSyncMs = now;

    try {
        /// Public endpoint. Recursion through the signing path is prevented by lastTimeSyncMs being set above -
        /// the nested signingTimestamp() call sees a fresh sync mark and returns immediately.
        const auto response = parent->get("/v5/market/time", {});

        if (response.result() != http::status::ok) {
            spdlog::warn(fmt::format("Time synchronization failed, HTTP {}", response.result_int()));
            return;
        }

        const auto json = nlohmann::json::parse(response.body());

        if (!json.contains("result")) {
            return;
        }

        /// timeNano arrives as a STRING, like every number in the Bybit API
        const auto timeNano = readStringAsInt64(json["result"], "timeNano");

        if (timeNano <= 0) {
            return;
        }

        const auto offset = timeNano / 1000000 - getMsTimestamp(currentTime()).count();
        timeOffsetMs = offset;

        if (std::abs(offset) > 1000) {
            spdlog::warn(fmt::format("Local clock differs from the exchange clock by {} ms, compensating", offset));
        }
    } catch (const std::exception& e) {
        spdlog::warn(fmt::format("Time synchronization failed: {}", e.what()));
    }
}

http::response<http::string_body> HTTPSession::P::request(http::request<http::string_body> req) {
    req.set(http::field::host, uri);
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

    ssl::context ctx{ssl::context::sslv23_client};
    enableTlsPeerVerification(ctx);

    tcp::resolver resolver{ioc};
    ssl::stream<tcp::socket> stream{ioc, ctx};
    stream.set_verify_callback(ssl::host_name_verification(uri));

    // Set SNI Hostname (many hosts need this to handshake successfully)
    if (!SSL_set_tlsext_host_name(stream.native_handle(), uri.c_str())) {
        boost::system::error_code ec{static_cast<int>(ERR_get_error()), net::error::get_ssl_category()};
        throw boost::system::system_error{ec};
    }

    beast::flat_buffer buffer;
    http::response_parser<http::string_body> parser;

    /// Beast defaults to an 8 MB body limit, which the public data archives (daily trade dumps) exceed
    parser.body_limit((std::numeric_limits<std::uint64_t>::max)());

    /// Everything below can fail without the exchange ever seeing the request - or after it has seen it. The caller
    /// must be able to tell that apart from a rejection, hence the dedicated exception type.
    try {
        auto const results = resolver.resolve(uri, "443");

        /// Manual endpoint loop instead of net::connect: the stall timeouts
        /// must be set on the OPEN socket before connect so they also bound
        /// the connect itself (Linux honors SO_SNDTIMEO for blocking connect).
        auto& socket = stream.next_layer();
        boost::system::error_code connectEc = net::error::host_not_found;

        for (const auto& entry: results) {
            boost::system::error_code ec;
            socket.close(ec);
            socket.open(entry.endpoint().protocol(), ec);

            if (ec) {
                connectEc = ec;
                continue;
            }

            setSocketStallTimeouts(socket);
            socket.connect(entry.endpoint(), ec);
            connectEc = ec;

            if (!ec) {
                break;
            }
        }

        if (connectEc) {
            throw boost::system::system_error{connectEc};
        }

        stream.handshake(ssl::stream_base::client);
        http::write(stream, req);
        http::read(stream, buffer, parser);
    } catch (const boost::system::system_error& e) {
        throw TransportError(fmt::format("Transport failure for {}: {}", std::string(req.target()), e.what()));
    }

    auto response = parser.release();

    boost::system::error_code ec;
    [[maybe_unused]] const auto rc = stream.shutdown(ec);
    if (ec == boost::asio::error::eof) {
        // Rationale:
        // http://stackoverflow.com/questions/25587403/boost-asio-ssl-async-shutdown-always-finishes-with-an-error
        ec.assign(0, ec.category());
    }

    return response;
}
} // namespace stonky::bybit
