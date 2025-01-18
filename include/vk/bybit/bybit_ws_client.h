/**
Bybit Futures WebSocket Client

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@gmail.com>.
*/

#ifndef INCLUDE_VK_BYBIT_FUTURES_WS_CLIENT_H
#define INCLUDE_VK_BYBIT_FUTURES_WS_CLIENT_H

#include "bybit_ws_session.h"
#include "vk/utils/log_utils.h"
#include <string>

namespace vk::bybit {
class WebSocketClient {
    struct P;
    std::unique_ptr<P> m_p{};

public:
    WebSocketClient(const WebSocketClient&) = delete;

    WebSocketClient& operator=(const WebSocketClient&) = delete;

    WebSocketClient(WebSocketClient&&) noexcept = default;

    WebSocketClient& operator=(WebSocketClient&&) noexcept = default;

    WebSocketClient();

    ~WebSocketClient();

    /**
     * Run the WebSocket IO Context asynchronously and returns immediately without blocking the thread execution
     */
    void run() const;

    /**
     * Set logger callback, if no set then all errors are writen to the stderr stream only
     * @param onLogMessageCB
     */
    void setLoggerCallback(const onLogMessage& onLogMessageCB) const;

    /**
     * Set Data Message callback
     * @param onDataEventCB
     */
    void setDataEventCallback(const onDataEvent& onDataEventCB) const;

    /**
     * Subscribe WebSocket according to the subscriptionFilter
     * @param subscriptionFilter e.g. instrument_info.100ms.BTCUSD
     * @see https://bybit-exchange.github.io/docs/futuresV2/linear/?console#t-subscribe
     */
    void subscribe(const std::string& subscriptionFilter) const;

    /**
     * Check if a stream is already subscribed
     * @param subscriptionFilter
     * @return True if subscribed
     */
    [[nodiscard]] bool isSubscribed(const std::string& subscriptionFilter) const;
};
}

#endif //INCLUDE_VK_BYBIT_FUTURES_WS_CLIENT_H
