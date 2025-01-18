/**
Bybit HTTPS Session

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@gmail.com>.
*/

#include "vk/bybit/bybit_http_session.h"
#include "vk/utils/utils.h"
#include "vk/utils/json_utils.h"
#include "nlohmann/json.hpp"
#include <boost/asio/ssl.hpp>
#include <boost/beast/version.hpp>
#include <openssl/hmac.h>

namespace vk::bybit {
namespace ssl = boost::asio::ssl;
using tcp = net::ip::tcp;

auto API_MAINNET_URI = "api.bybit.com";
auto API_TESTNET_URI = "api-testnet.bybit.com";

struct HTTPSession::P {
    net::io_context m_ioc;
    std::string m_apiKey;
    int m_receiveWindow = 25000;
    std::string m_apiSecret;
    std::string m_uri;
    const EVP_MD* m_evp_md;

    P() : m_evp_md(EVP_sha256()) {
    }

    http::response<http::string_body> request(http::request<http::string_body> req);

    static std::string createQueryStr(const std::map<std::string, std::string>& parameters) {
        std::string queryStr;

        for (const auto& [fst, snd] : parameters) {
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
        const auto ts = getMsTimestamp(currentTime()).count();

        nlohmann::json extendedJson = json;
        extendedJson["timestamp"] = ts;
        extendedJson["recv_window"] = m_receiveWindow;
        extendedJson["api_key"] = m_apiKey;

        const std::string queryString = queryStringFromJson(extendedJson);

        unsigned char digest[SHA256_DIGEST_LENGTH];
        unsigned int digestLength = SHA256_DIGEST_LENGTH;

        HMAC(m_evp_md, m_apiSecret.data(), m_apiSecret.size(),
             reinterpret_cast<const unsigned char*>(queryString.data()),
             queryString.length(), digest, &digestLength);

        std::string signature = stringToHex(digest, sizeof(digest));

        extendedJson["sign"] = signature;

        req.body() = extendedJson.dump();
        req.prepare_payload();

        req.set(http::field::content_type, "application/json");
    }

    void authenticateNonPost(http::request<http::string_body>& req) const {
        std::string path(req.target());
        const std::size_t pos = path.find('?');
        std::string queryString;

        if (pos != std::string::npos) {
            queryString = path.substr(pos + 1);
        }

        std::string parameterString;

        const auto ts = getMsTimestamp(currentTime()).count();
        parameterString.append(std::to_string(ts));
        parameterString.append(m_apiKey);
        parameterString.append(std::to_string(m_receiveWindow));
        parameterString.append(queryString);

        unsigned char digest[SHA256_DIGEST_LENGTH];
        unsigned int digestLength = SHA256_DIGEST_LENGTH;

        HMAC(m_evp_md, m_apiSecret.data(), m_apiSecret.size(),
             reinterpret_cast<const unsigned char*>(parameterString.data()),
             parameterString.length(), digest, &digestLength);

        const std::string signature = stringToHex(digest, sizeof(digest));

        req.set("X-BAPI-API-KEY", m_apiKey);
        req.set("X-BAPI-SIGN", signature);
        req.set("X-BAPI-SIGN-TYPE", "2");
        req.set("X-BAPI-TIMESTAMP", std::to_string(ts));
        req.set("X-BAPI-RECV-WINDOW", std::to_string(m_receiveWindow));
    }
};

HTTPSession::HTTPSession(const std::string& apiKey, const std::string& apiSecret) : m_p(std::make_unique<P>()) {
    m_p->m_uri = API_MAINNET_URI;
    m_p->m_apiKey = apiKey;
    m_p->m_apiSecret = apiSecret;
}

HTTPSession::~HTTPSession() = default;

http::response<http::string_body> HTTPSession::get(const std::string& path,
                                                   const std::map<std::string, std::string>& parameters) const {
    std::string finalPath = path;

    if (const auto queryString = m_p->createQueryStr(parameters); !queryString.empty()) {
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

http::response<http::string_body> HTTPSession::P::request(
    http::request<http::string_body> req) {
    req.set(http::field::host, m_uri.c_str());
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

    ssl::context ctx{ssl::context::sslv23_client};
    ctx.set_default_verify_paths();

    tcp::resolver resolver{m_ioc};
    ssl::stream<tcp::socket> stream{m_ioc, ctx};

    // Set SNI Hostname (many hosts need this to handshake successfully)
    if (!SSL_set_tlsext_host_name(stream.native_handle(), m_uri.c_str())) {
        boost::system::error_code ec{
            static_cast<int>(ERR_get_error()),
            net::error::get_ssl_category()
        };
        throw boost::system::system_error{ec};
    }

    auto const results = resolver.resolve(m_uri, "443");
    net::connect(stream.next_layer(), results.begin(), results.end());
    stream.handshake(ssl::stream_base::client);

    http::write(stream, req);
    beast::flat_buffer buffer;
    http::response<http::string_body> response;
    http::read(stream, buffer, response);

    boost::system::error_code ec;
    stream.shutdown(ec);
    if (ec == boost::asio::error::eof) {
        // Rationale:
        // http://stackoverflow.com/questions/25587403/boost-asio-ssl-async-shutdown-always-finishes-with-an-error
        ec.assign(0, ec.category());
    }

    return response;
}
}
