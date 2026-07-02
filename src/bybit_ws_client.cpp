/**
Bybit Futures WebSocket Client

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.
*/

#include "stonky/bybit/bybit_ws_client.h"
#include <boost/beast/core.hpp>
#include <thread>

using namespace std::chrono_literals;

namespace stonky::bybit {
#define STRINGIZE_I(x) #x
#define STRINGIZE(x) STRINGIZE_I(x)

#define MAKE_FILELINE \
    __FILE__ "(" STRINGIZE(__LINE__) ")"

static auto BYBIT_FUTURES_WS_HOST = "stream.bybit.com";
static auto BYBIT_FUTURES_WS_PORT = "443";
static auto BYBIT_FUTURES_WS_PATH = "/v5/public/linear";

struct WebSocketClient::P {
    boost::asio::io_context ioContext;
    boost::asio::ssl::context ctx;
    std::string host = {BYBIT_FUTURES_WS_HOST};
    std::string port = {BYBIT_FUTURES_WS_PORT};
    std::string path = {BYBIT_FUTURES_WS_PATH};
    std::string apiKey;
    std::string apiSecret;
    /// Keeps the session alive across reconnect cycles; the session's own
    /// async chain also holds shared_from_this while ops are pending.
    std::shared_ptr<WebSocketSession> session;
    std::thread ioThread;
    std::atomic<bool> isRunning = false;
    onLogMessage logMessageCB;
    onDataEvent dataEventCB;

    P() : ctx(boost::asio::ssl::context::sslv23_client) {
    }
};

WebSocketClient::WebSocketClient() : m_p(std::make_unique<P>()) {
}

WebSocketClient::~WebSocketClient() {
    if (m_p->session) {
        m_p->session->close();
    }

    m_p->ioContext.stop();

    if (m_p->ioThread.joinable()) {
        m_p->ioThread.join();
    }
}

void WebSocketClient::setEndpoint(const std::string& host, const std::string& port, const std::string& path) const {
    m_p->host = host;
    m_p->port = port;
    m_p->path = path;
}

void WebSocketClient::setCredentials(const std::string& apiKey, const std::string& apiSecret) const {
    m_p->apiKey = apiKey;
    m_p->apiSecret = apiSecret;
}

void WebSocketClient::run() const {
    if (m_p->isRunning) {
        return;
    }

    m_p->isRunning = true;

    if (m_p->ioThread.joinable()) {
        m_p->ioThread.join();
    }

    m_p->ioThread = std::thread([&] {
        for (;;) {
            try {
                m_p->isRunning = true;

                if (m_p->ioContext.stopped()) {
                    m_p->ioContext.restart();
                }
                m_p->ioContext.run();
                m_p->isRunning = false;
                break;
            }
            catch (std::exception& e) {
                if (m_p->logMessageCB) {
                    m_p->logMessageCB(LogSeverity::Error, fmt::format("{}: {}\n", MAKE_FILELINE, e.what()));
                }
            }
        }

        m_p->isRunning = false;
    });
}

void WebSocketClient::setLoggerCallback(const onLogMessage& onLogMessageCB) const {
    m_p->logMessageCB = onLogMessageCB;
}

void WebSocketClient::setDataEventCallback(const onDataEvent& onDataEventCB) const {
    m_p->dataEventCB = onDataEventCB;
}

void WebSocketClient::subscribe(const std::string& subscriptionFilter) const {
    if (m_p->session) {
        m_p->session->subscribe(subscriptionFilter);
        return;
    }

    const auto ws = std::make_shared<WebSocketSession>(m_p->ioContext, m_p->ctx, m_p->logMessageCB);
    m_p->session = ws;

    if (!m_p->apiKey.empty() && !m_p->apiSecret.empty()) {
        ws->setCredentials(m_p->apiKey, m_p->apiSecret);
    }

    ws->run(m_p->host, m_p->port, m_p->path, subscriptionFilter, m_p->dataEventCB);
}

void WebSocketClient::unsubscribe(const std::string& subscriptionFilter) const {
    if (m_p->session) {
        m_p->session->unsubscribe(subscriptionFilter);
    }
}

bool WebSocketClient::isSubscribed(const std::string& subscriptionFilter) const {
    if (m_p->session) {
        return m_p->session->isSubscribed(subscriptionFilter);
    }

    return false;
}

bool WebSocketClient::isAuthenticated() const {
    if (m_p->session) {
        return m_p->session->isAuthenticated();
    }

    return false;
}
}
