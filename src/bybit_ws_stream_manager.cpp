/**
Bybit Futures WebSocket Stream manager v5

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@gmail.com>.
*/

#include "vk/bybit/bybit_rest_client.h"
#include "vk/bybit/bybit_ws_stream_manager.h"
#include "vk/bybit/bybit_ws_client.h"
#include "vk/utils/utils.h"
#include <mutex>
#include <thread>

using namespace std::chrono_literals;

namespace vk::bybit {
struct WSStreamManager::P {
    std::unique_ptr<WebSocketClient> wsClient;
    int timeout{5};
    mutable std::recursive_mutex instrumentInfoLocker;
    mutable std::recursive_mutex candlestickLocker;
    std::map<std::string, EventTicker> tickers;
    std::map<std::string, std::map<CandleInterval, EventCandlestick>> candlesticks;
    onLogMessage logMessageCB;

    explicit P() : wsClient(std::make_unique<WebSocketClient>()) {
        wsClient->setDataEventCallback([&](const Event& event) {
            if (event.topic.find("tickers") != std::string::npos) {
                std::lock_guard lk(instrumentInfoLocker);

                try {
                    if (const auto it = tickers.find(readSymbolFromFilter(event.topic)); it == tickers.end()) {
                        EventTicker eventTicker;
                        eventTicker.loadEventData(event);
                        tickers.insert_or_assign(eventTicker.symbol, eventTicker);
                    } else {
                        it->second.loadEventData(event);
                    }
                } catch (std::exception& e) {
                    logMessageCB(LogSeverity::Error, fmt::format("{}: {}", MAKE_FILELINE, e.what()));
                }
            } else if (event.topic.find("kline") != std::string::npos) {
                std::lock_guard lk(candlestickLocker);

                try {
                    EventCandlestick eventCandlestick;
                    if (const auto candlesNumber = event.data.size(); candlesNumber != 1) {
                        logMessageCB(LogSeverity::Error, fmt::format("{}: {}: {}", MAKE_FILELINE, "unexpected candles number", candlesNumber));
                    }

                    eventCandlestick.fromJson(event.data[0]);

                    /// Insert new candle
                    {
                        const auto symbol = readSymbolFromFilter(event.topic);
                        auto it = candlesticks.find(symbol);

                        if (it == candlesticks.end()) {
                            candlesticks.insert({symbol, {}});
                        }

                        it = candlesticks.find(symbol);
                        it->second.insert_or_assign(*magic_enum::enum_cast<CandleInterval>(eventCandlestick.interval), eventCandlestick);
                    }
                } catch (std::exception& e) {
                    logMessageCB(LogSeverity::Error, fmt::format("{}: {}", MAKE_FILELINE, e.what()));
                }
            }
        });
    }

    static std::string readSymbolFromFilter(const std::string& subscriptionFilter) {
        if (const auto records = splitString(subscriptionFilter, '.'); !records.empty()) {
            return records.back();
        }

        return "";
    }
};

WSStreamManager::WSStreamManager() : m_p(std::make_unique<P>()) {}

WSStreamManager::~WSStreamManager() {
    m_p->wsClient.reset();
    m_p->timeout = 0;
}

void WSStreamManager::subscribeTickerStream(const std::string& pair) const {
    std::string subscriptionFilter = "tickers.";
    subscriptionFilter.append(pair);

    if (!m_p->wsClient->isSubscribed(subscriptionFilter)) {
        if (m_p->logMessageCB) {
            const auto msgString = fmt::format("subscribing: {}", subscriptionFilter);
            m_p->logMessageCB(LogSeverity::Info, msgString);
        }

        m_p->wsClient->subscribe(subscriptionFilter);
    }

    m_p->wsClient->run();
}

void WSStreamManager::subscribeCandlestickStream(const std::string& pair, const CandleInterval interval) const {
    std::string subscriptionFilter = "kline.";
    subscriptionFilter.append(magic_enum::enum_name(interval));
    subscriptionFilter.append(".");
    subscriptionFilter.append(pair);

    if (!m_p->wsClient->isSubscribed(subscriptionFilter)) {
        if (m_p->logMessageCB) {
            const auto msgString = fmt::format("subscribing: {}", subscriptionFilter);
            m_p->logMessageCB(LogSeverity::Info, msgString);
        }

        m_p->wsClient->subscribe(subscriptionFilter);
    }

    m_p->wsClient->run();
}

void WSStreamManager::setTimeout(const int seconds) const { m_p->timeout = seconds; }

int WSStreamManager::timeout() const { return m_p->timeout; }

void WSStreamManager::setLoggerCallback(const onLogMessage& onLogMessageCB) const {
    m_p->logMessageCB = onLogMessageCB;
    m_p->wsClient->setLoggerCallback(onLogMessageCB);
}

std::optional<EventTicker> WSStreamManager::readEventTicker(const std::string& pair) const {
    int numTries = 0;
    const int maxNumTries = static_cast<int>(m_p->timeout / 0.01);

    while (numTries <= maxNumTries) {
        if (m_p->timeout == 0) {
            /// No need to wait when destroying object
            break;
        }

        m_p->instrumentInfoLocker.lock();

        if (const auto it = m_p->tickers.find(pair); it != m_p->tickers.end()) {
            auto retVal = it->second;

            m_p->instrumentInfoLocker.unlock();
            return retVal;
        }
        m_p->instrumentInfoLocker.unlock();
        numTries++;
        std::this_thread::sleep_for(3ms);
    }

    return {};
}

std::optional<EventCandlestick> WSStreamManager::readEventCandlestick(const std::string& pair, const CandleInterval interval) const {
    int numTries = 0;
    const int maxNumTries = static_cast<int>(m_p->timeout / 0.01);

    while (numTries <= maxNumTries) {
        if (m_p->timeout == 0) {
            /// No need to wait when destroying object
            break;
        }

        m_p->candlestickLocker.lock();

        if (const auto it = m_p->candlesticks.find(pair); it != m_p->candlesticks.end()) {
            if (const auto itCandle = it->second.find(interval); itCandle != it->second.end()) {
                auto retVal = itCandle->second;
                m_p->candlestickLocker.unlock();
                return retVal;
            }
        }
        m_p->candlestickLocker.unlock();
        numTries++;
        std::this_thread::sleep_for(3ms);
    }

    return {};
}
} // namespace vk::bybit
