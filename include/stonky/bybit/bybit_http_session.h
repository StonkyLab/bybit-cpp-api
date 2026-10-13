/**
Bybit HTTPS Session

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.
*/

#ifndef INCLUDE_STONKY_BYBIT_HTTP_SESSION_H
#define INCLUDE_STONKY_BYBIT_HTTP_SESSION_H

#include <boost/asio/connect.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <stdexcept>
#include <string>
#include <map>
#include <nlohmann/json_fwd.hpp>

namespace stonky::bybit {
/// Default inactivity bound for each network operation of a request
static constexpr int DEFAULT_REQUEST_TIMEOUT_MS = 15000;

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;

/**
 * Base of every failure that leaves the outcome of the request UNKNOWN: it may or may not have been executed by the
 * exchange. An order that fails this way must never be treated as rejected - it has to be reconciled by querying it
 * back, otherwise a retry silently doubles the position.
 */
class UnknownOutcomeError : public std::runtime_error {
public:
    explicit UnknownOutcomeError(const std::string& message) : std::runtime_error(message) {}
};

/// The request failed on the transport level - name resolution, TCP, TLS or a timeout
class TransportError final : public UnknownOutcomeError {
public:
    explicit TransportError(const std::string& message) : UnknownOutcomeError(message) {}
};

/// The exchange answered with a status that leaves the execution state open - the HTTP 5xx family and Bybit's
/// internal server errors
class ExecutionUnknown final : public UnknownOutcomeError {
public:
    explicit ExecutionUnknown(const std::string& message) : UnknownOutcomeError(message) {}
};

/**
 * The exchange answered definitively that it does not know the order. Unlike every other error this DOES settle the
 * question: the order never reached the matching engine, so the caller may treat it as never placed.
 */
class OrderNotFound final : public std::runtime_error {
public:
    explicit OrderNotFound(const std::string& message) : std::runtime_error(message) {}
};

class HTTPSession {
    struct P;
    std::unique_ptr<P> m_p{};

public:
    HTTPSession(const std::string& apiKey, const std::string& apiSecret, const std::string& host = "");

    ~HTTPSession();

    [[nodiscard]] http::response<http::string_body> get(const std::string& path, const std::map<std::string, std::string>& parameters) const;

    [[nodiscard]] http::response<http::string_body> post(const std::string& path, const nlohmann::json& json) const;

    /**
     * Wall clock time in ms of the last response that was received in full. Zero when nothing has been received yet.
     * Lets callers judge whether the connection is alive without issuing a probe request of their own.
     */
    [[nodiscard]] std::int64_t lastSuccessfulResponseMs() const;

    /**
     * Inactivity bound for DNS, connect, TLS, write and each response read.
     * @param timeoutMs 0 or less leaves the operating system defaults in place
     */
    void setRequestTimeout(int timeoutMs) const;
};
} // namespace stonky::bybit
#endif // INCLUDE_STONKY_BYBIT_HTTP_SESSION_H
