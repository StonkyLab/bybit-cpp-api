/**
Bybit Private WebSocket Stream Manager

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2026 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.
*/

#include "stonky/bybit/bybit_ws_private_stream_manager.h"
#include "stonky/bybit/bybit_ws_client.h"

namespace stonky::bybit {
static auto BYBIT_WS_PRIVATE_PORT = "443";
static auto BYBIT_WS_PRIVATE_PATH = "/v5/private";

static std::string wsPrivateHostForEnvironment(const Environment env) {
    switch (env) {
        case Environment::Testnet:
            return "stream-testnet.bybit.com";
        case Environment::Demo:
            return "stream-demo.bybit.com";
        case Environment::Mainnet:
            break;
    }

    return "stream.bybit.com";
}

/// All-in-one topics (rows carry a "category" field). Do not mix with the
/// categorised variants (order.linear, ...) in one subscribe request.
static auto ORDER_TOPIC = "order";
static auto EXECUTION_TOPIC = "execution";

struct WSPrivateStreamManager::P {
    std::unique_ptr<WebSocketClient> wsClient;
    onLogMessage logMessageCB;
    onOrderUpdate orderUpdateCB;
    onExecution executionCB;

    explicit P() : wsClient(std::make_unique<WebSocketClient>()) {
        wsClient->setDataEventCallback([&](const Event& event) {
            if (event.topic == ORDER_TOPIC || event.topic.starts_with("order.")) {
                dispatchArray<EventOrderUpdate>(event, orderUpdateCB);
            } else if (event.topic == EXECUTION_TOPIC || event.topic.starts_with("execution.")) {
                dispatchArray<EventExecution>(event, executionCB);
            }
        });
    }

    /// Private topics batch N items per message in Event.data — deliver each
    /// element separately so a parse failure of one item cannot drop the rest.
    template <typename EventType, typename Callback>
    void dispatchArray(const Event& event, const Callback& cb) const {
        if (!cb || !event.data.is_array()) {
            return;
        }

        for (const auto& item: event.data) {
            try {
                EventType typedEvent;
                typedEvent.fromJson(item);
                cb(typedEvent);
            } catch (std::exception& e) {
                if (logMessageCB) {
                    logMessageCB(LogSeverity::Error, fmt::format("{}: {}", MAKE_FILELINE, e.what()));
                }
            }
        }
    }
};

WSPrivateStreamManager::WSPrivateStreamManager(const std::string& apiKey, const std::string& apiSecret, const Environment env) : m_p(std::make_unique<P>()) {
    m_p->wsClient->setEndpoint(wsPrivateHostForEnvironment(env), BYBIT_WS_PRIVATE_PORT, BYBIT_WS_PRIVATE_PATH);
    m_p->wsClient->setCredentials(apiKey, apiSecret);
}

WSPrivateStreamManager::~WSPrivateStreamManager() { m_p->wsClient.reset(); }

void WSPrivateStreamManager::setLoggerCallback(const onLogMessage& onLogMessageCB) const {
    m_p->logMessageCB = onLogMessageCB;
    m_p->wsClient->setLoggerCallback(onLogMessageCB);
}

void WSPrivateStreamManager::setOrderUpdateCallback(const onOrderUpdate& onOrderUpdateCB) const { m_p->orderUpdateCB = onOrderUpdateCB; }

void WSPrivateStreamManager::setExecutionCallback(const onExecution& onExecutionCB) const { m_p->executionCB = onExecutionCB; }

bool WSPrivateStreamManager::isAuthenticated() const { return m_p->wsClient->isAuthenticated(); }

void WSPrivateStreamManager::connect() const {
    if (!m_p->wsClient->isSubscribed(ORDER_TOPIC)) {
        m_p->wsClient->subscribe(ORDER_TOPIC);
    }

    if (!m_p->wsClient->isSubscribed(EXECUTION_TOPIC)) {
        m_p->wsClient->subscribe(EXECUTION_TOPIC);
    }

    m_p->wsClient->run();
}
} // namespace stonky::bybit
