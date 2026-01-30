/**
Bybit Futures WebSocket Client

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@gmail.com>.
*/

#include "vk/bybit/bybit_ws_client.h"
#include <boost/beast/core.hpp>
#include <thread>

using namespace std::chrono_literals;

namespace vk::bybit {
#define STRINGIZE_I(x) #x
#define STRINGIZE(x) STRINGIZE_I(x)

#define MAKE_FILELINE \
    __FILE__ "(" STRINGIZE(__LINE__) ")"

static auto BYBIT_FUTURES_WS_HOST = "stream.bybit.com";
static auto BYBIT_FUTURES_WS_PORT = "443";

struct WebSocketClient::P {
    boost::asio::io_context ioContext;
    boost::asio::ssl::context ctx;
    std::string host = {BYBIT_FUTURES_WS_HOST};
    std::string port = {BYBIT_FUTURES_WS_PORT};
    std::weak_ptr<WebSocketSession> session;
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
    m_p->ioContext.stop();

    if (m_p->ioThread.joinable()) {
        m_p->ioThread.join();
    }
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
    if (const auto session = m_p->session.lock()) {
        session->subscribe(subscriptionFilter);
        return;
    }

    const auto ws = std::make_shared<WebSocketSession>(m_p->ioContext, m_p->ctx, m_p->logMessageCB);
    std::weak_ptr wp{ws};
    m_p->session = std::move(wp);
    ws->run(BYBIT_FUTURES_WS_HOST, BYBIT_FUTURES_WS_PORT, subscriptionFilter, m_p->dataEventCB);
}

bool WebSocketClient::isSubscribed(const std::string& subscriptionFilter) const {
    if (const auto session = m_p->session.lock()) {
        return session->isSubscribed(subscriptionFilter);
    }

    return false;
}
}
