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
namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;

/**
 * Thrown when a request fails on the transport level - name resolution, TCP, TLS or a timeout. In contrast to an API
 * error this means the outcome is UNKNOWN: an order may or may not have reached the exchange, so the caller must not
 * treat it as a rejection.
 */
class TransportError final : public std::runtime_error {
public:
    explicit TransportError(const std::string& message) : std::runtime_error(message) {}
};

class HTTPSession {
    struct P;
    std::unique_ptr<P> m_p{};

public:
    HTTPSession(const std::string& apiKey, const std::string& apiSecret, const std::string& host = "");

    ~HTTPSession();

    [[nodiscard]] http::response<http::string_body> get(const std::string& path, const std::map<std::string, std::string>& parameters) const;

    [[nodiscard]] http::response<http::string_body> post(const std::string& path, const nlohmann::json& json) const;
};
} // namespace stonky::bybit
#endif // INCLUDE_STONKY_BYBIT_HTTP_SESSION_H
