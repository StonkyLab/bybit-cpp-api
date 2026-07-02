/**
Bybit Futures WebSocket Client

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.
*/

#ifndef INCLUDE_STONKY_BYBIT_FUTURES_WS_CLIENT_H
#define INCLUDE_STONKY_BYBIT_FUTURES_WS_CLIENT_H

#include "bybit_ws_session.h"
#include "stonky/utils/log_utils.h"
#include <string>

namespace stonky::bybit {
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
     * Override the stream endpoint. Must be called before the first subscribe().
     * Defaults to the public linear mainnet stream
     * (stream.bybit.com:443, /v5/public/linear).
     * @param host e.g. stream.bybit.com or stream-testnet.bybit.com
     * @param port e.g. 443
     * @param path e.g. /v5/public/linear or /v5/private
     */
    void setEndpoint(const std::string& host, const std::string& port, const std::string& path) const;

    /**
     * Set API credentials for a private stream. Must be called before the first
     * subscribe(). The session then authenticates after every (re)connect.
     * @param apiKey
     * @param apiSecret
     */
    void setCredentials(const std::string& apiKey, const std::string& apiSecret) const;

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
     * Unsubscribe a previously subscribed stream. No-op when unknown.
     * @param subscriptionFilter
     */
    void unsubscribe(const std::string& subscriptionFilter) const;

    /**
     * Check if a stream is already subscribed
     * @param subscriptionFilter
     * @return True if subscribed
     */
    [[nodiscard]] bool isSubscribed(const std::string& subscriptionFilter) const;

    /**
     * True once the private session authenticated its CURRENT connection.
     * Always false without credentials.
     */
    [[nodiscard]] bool isAuthenticated() const;
};
}

#endif //INCLUDE_STONKY_BYBIT_FUTURES_WS_CLIENT_H
