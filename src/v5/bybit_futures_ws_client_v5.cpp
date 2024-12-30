/**
Bybit Futures WebSocket Client v5

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@gmail.com>.
*/

#include "vk/bybit/v5/bybit_futures_ws_client_v5.h"
#include <boost/beast/core.hpp>
#include <fmt/format.h>
#include <thread>

using namespace std::chrono_literals;

namespace vk::bybit::v5::futures {
#define STRINGIZE_I(x) #x
#define STRINGIZE(x) STRINGIZE_I(x)

#define MAKE_FILELINE \
    __FILE__ "(" STRINGIZE(__LINE__) ")"

static auto BYBIT_FUTURES_WS_HOST = "stream.bybit.com";
static auto BYBIT_FUTURES_WS_PORT = "443";

struct WebSocketClient::P {
    boost::asio::io_context m_ioContext;
    boost::asio::ssl::context m_ctx;
    std::string m_host = {BYBIT_FUTURES_WS_HOST};
    std::string m_port = {BYBIT_FUTURES_WS_PORT};
    std::weak_ptr<WebSocketSession> m_session;
    std::thread m_ioThread;
    std::atomic<bool> m_isRunning = false;
    onLogMessage m_logMessageCB;
    onDataEvent m_dataEventCB;

    P() : m_ctx(boost::asio::ssl::context::sslv23_client) {
    }
};

WebSocketClient::WebSocketClient() : m_p(std::make_unique<P>()) {
}

WebSocketClient::~WebSocketClient() {
    m_p->m_ioContext.stop();

    if (m_p->m_ioThread.joinable()) {
        m_p->m_ioThread.join();
    }
}

void WebSocketClient::run() const {
    if (m_p->m_isRunning) {
        return;
    }

    m_p->m_isRunning = true;

    if (m_p->m_ioThread.joinable()) {
        m_p->m_ioThread.join();
    }

    m_p->m_ioThread = std::thread([&] {
        for (;;) {
            try {
                m_p->m_isRunning = true;

                if (m_p->m_ioContext.stopped()) {
                    m_p->m_ioContext.restart();
                }
                m_p->m_ioContext.run();
                m_p->m_isRunning = false;
                break;
            }
            catch (std::exception& e) {
                if (m_p->m_logMessageCB) {
                    m_p->m_logMessageCB(LogSeverity::Error, fmt::format("{}: {}\n", MAKE_FILELINE, e.what()));
                }
            }
        }

        m_p->m_isRunning = false;
    });
}

void WebSocketClient::setLoggerCallback(const onLogMessage& onLogMessageCB) const {
    m_p->m_logMessageCB = onLogMessageCB;
}

void WebSocketClient::setDataEventCallback(const onDataEvent& onDataEventCB) const {
    m_p->m_dataEventCB = onDataEventCB;
}

void WebSocketClient::subscribe(const std::string& subscriptionFilter) const {
    if (const auto session = m_p->m_session.lock()) {
        session->subscribe(subscriptionFilter);
        return;
    }

    const auto ws = std::make_shared<WebSocketSession>(m_p->m_ioContext, m_p->m_ctx, m_p->m_logMessageCB);
    std::weak_ptr wp{ws};
    m_p->m_session = std::move(wp);
    ws->run(BYBIT_FUTURES_WS_HOST, BYBIT_FUTURES_WS_PORT, subscriptionFilter, m_p->m_dataEventCB);
}

bool WebSocketClient::isSubscribed(const std::string& subscriptionFilter) const {
    if (const auto session = m_p->m_session.lock()) {
        return session->isSubscribed(subscriptionFilter);
    }

    return false;
}
}
