/**
Bybit Futures WebSocket Stream manager v5

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@gmail.com>.
*/

#ifndef INCLUDE_VK_BYBIT_FUTURES_WS_STREAM_MANAGER_V5_H
#define INCLUDE_VK_BYBIT_FUTURES_WS_STREAM_MANAGER_V5_H

#include "vk/tools/log_utils.h"
#include "vk/bybit/v5/bybit_event_models_v5.h"
#include "vk/bybit/v5/bybit_models_v5.h"
#include <optional>

namespace vk::bybit::v5::futures {
class WSStreamManager {
    struct P;
    std::unique_ptr<P> m_p{};

public:
    explicit WSStreamManager();

    ~ WSStreamManager();

    /**
     * Check if the Ticker Stream is subscribed for a selected pair, if not then subscribe it. When force parameter
     * is true then re-subscribe if already subscribed
     * @param pair e.g BTCUSDT
     */
    void subscribeTickerStream(const std::string& pair) const;

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
     * Set logger callback, if no set then all errors are writen to the stderr stream only
     * @param onLogMessageCB
     */
    void setLoggerCallback(const onLogMessage& onLogMessageCB) const;

    /**
     * Try to read EventTicker structure. It will block at most Timeout time.
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

#endif //INCLUDE_VK_BYBIT_FUTURES_WS_STREAM_MANAGER_V5_H
