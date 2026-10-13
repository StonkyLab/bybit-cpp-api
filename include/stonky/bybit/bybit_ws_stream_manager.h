/**
Bybit Futures WebSocket Stream manager

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.
*/

#ifndef INCLUDE_STONKY_BYBIT_FUTURES_WS_STREAM_MANAGER_H
#define INCLUDE_STONKY_BYBIT_FUTURES_WS_STREAM_MANAGER_H

#include "stonky/utils/log_utils.h"
#include "stonky/bybit/bybit_event_models.h"
#include <memory>
#include <optional>

namespace stonky::bybit {
using onTickerUpdate = std::function<void(const EventTicker&)>;

class RESTClient;

class WSStreamManager {
    struct P;
    std::unique_ptr<P> m_p{};

public:
    explicit WSStreamManager();

    ~ WSStreamManager();

    /**
     * Provide a REST client used to seed a fresh subscription and to refresh a quote that went stale because the
     * stream is silent or dead. Optional - without it readEventTicker() only ever returns what the stream delivered.
     * @param restClient
     */
    void setRestClient(const std::weak_ptr<RESTClient>& restClient) const;

    /**
     * Check if the Ticker Stream is subscribed for a selected pair, if not then subscribe it. When force parameter
     * is true then re-subscribe if already subscribed.
     *
     * When a REST client is set, the current quote is seeded from the REST snapshot so that a price is available
     * immediately instead of only after the first stream message.
     *
     * @param pair e.g BTCUSDT
     */
    void subscribeTickerStream(const std::string& pair) const;

    /**
     * Unsubscribe the Ticker Stream of a pair and drop its cached last value.
     * No-op when not subscribed.
     * @param pair e.g BTCUSDT
     */
    void unsubscribeTickerStream(const std::string& pair) const;

    /**
     * Set a callback invoked on EVERY ticker update (snapshot or delta), with
     * the merged EventTicker. Invoked on the WebSocket io thread — must be
     * fast and non-blocking. Set before subscribing.
     * @param onTickerUpdateCB
     */
    void setTickerUpdateCallback(const onTickerUpdate& onTickerUpdateCB) const;

    /**
     * Check if the Candlestick Stream is subscribed for a selected pair, if not then subscribe it. When force parameter
     * is true then re-subscribe if already subscribed
     * @param pair e.g BTCUSDT
     * @param interval e.g CandleInterval::_1
     */
    void subscribeCandlestickStream(const std::string& pair, CandleInterval interval) const;

    /**
     * Set time of all reading operations
     * @param seconds
     */
    void setTimeout(int seconds) const;

    /**
     * Get time of all reading operations
     * @return
     */
    [[nodiscard]] int timeout() const;

    /**
     * Set the maximum age of a cached quote. Once the last update is older than this, the cached value is not
     * returned anymore - a fresh REST snapshot is fetched instead. This guards against a silently dead WebSocket
     * session serving stale prices forever.
     * @param seconds age in seconds, 0 disables the check
     */
    void setMaxTickAge(int seconds) const;

    /**
     * Get the maximum age of a cached quote
     * @return age in seconds, 0 means the check is disabled
     */
    [[nodiscard]] int maxTickAge() const;

    /**
     * Set logger callback, if no set then all errors are writen to the stderr stream only
     * @param onLogMessageCB
     */
    void setLoggerCallback(const onLogMessage& onLogMessageCB) const;

    /**
     * Try to read EventTicker structure. It will block at most Timeout time.
     *
     * If nothing arrived within the timeout, or the cached quote is older than maxTickAge(), a REST snapshot is
     * fetched and returned instead (when a REST client is set), so an illiquid symbol never looks like a failure.
     *
     * @param pair e.g BTCUSDT
     * @return EventTicker structure if successful
     */
    [[nodiscard]] std::optional<EventTicker> readEventTicker(const std::string& pair) const;

    /**
     * Try to read EventCandlestick structure. It will block at most Timeout time.
     * @param pair e.g BTCUSDT
     * @param interval e.g CandleInterval::_1
     * @return EventCandlestick structure if successful
     */
    [[nodiscard]] std::optional<EventCandlestick>
    readEventCandlestick(const std::string& pair, CandleInterval interval) const;
};
}

#endif //INCLUDE_STONKY_BYBIT_FUTURES_WS_STREAM_MANAGER_H
