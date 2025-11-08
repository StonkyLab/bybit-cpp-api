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
    std::unique_ptr<WebSocketClient> m_wsClient;
    int m_timeout{5};
    mutable std::recursive_mutex m_instrumentInfoLocker;
    mutable std::recursive_mutex m_candlestickLocker;
    std::map<std::string, EventTicker> m_tickers;
    std::map<std::string, std::map<CandleInterval, EventCandlestick>> m_candlesticks;

    onLogMessage m_logMessageCB;

    explicit P() : m_wsClient(std::make_unique<WebSocketClient>()) {
        m_wsClient->setDataEventCallback([&](const Event& event) {
            if (event.m_topic.find("tickers") != std::string::npos) {
                std::lock_guard lk(m_instrumentInfoLocker);

                try {
                    if (const auto it = m_tickers.find(readSymbolFromFilter(event.m_topic)); it == m_tickers.end()) {
                        EventTicker eventTicker;
                        eventTicker.loadEventData(event);
                        m_tickers.insert_or_assign(eventTicker.m_symbol, eventTicker);
                    }
                    else {
                        it->second.loadEventData(event);
                    }
                }
                catch (std::exception& e) {
                    m_logMessageCB(LogSeverity::Error, fmt::format("{}: {}", MAKE_FILELINE, e.what()));
                }
            }
            else if (event.m_topic.find("kline") != std::string::npos) {
                std::lock_guard lk(m_candlestickLocker);

                try {
                    EventCandlestick eventCandlestick;
                    if (const auto candlesNumber = event.m_data.size(); candlesNumber != 1) {
                        m_logMessageCB(LogSeverity::Error,
                                       fmt::format("{}: {}: {}", MAKE_FILELINE, "unexpected candles number",
                                                   candlesNumber));
                    }

                    eventCandlestick.fromJson(event.m_data[0]);

                    /// Insert new candle
                    {
                        const auto symbol = readSymbolFromFilter(event.m_topic);
                        auto it = m_candlesticks.find(symbol);

                        if (it == m_candlesticks.end()) {
                            m_candlesticks.insert({symbol, {}});
                        }

                        it = m_candlesticks.find(symbol);
                        it->second.insert_or_assign(*magic_enum::enum_cast<CandleInterval>(eventCandlestick.m_interval),
                                                    eventCandlestick);
                    }
                }
                catch (std::exception& e) {
                    m_logMessageCB(LogSeverity::Error, fmt::format("{}: {}", MAKE_FILELINE, e.what()));
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

WSStreamManager::WSStreamManager() : m_p(std::make_unique<P>()) {
}

WSStreamManager::~WSStreamManager() {
    m_p->m_wsClient.reset();
    m_p->m_timeout = 0;
}

void WSStreamManager::subscribeTickerStream(const std::string& pair) const {
    std::string subscriptionFilter = "tickers.";
    subscriptionFilter.append(pair);

    if (!m_p->m_wsClient->isSubscribed(subscriptionFilter)) {
        if (m_p->m_logMessageCB) {
            const auto msgString = fmt::format("subscribing: {}", subscriptionFilter);
            m_p->m_logMessageCB(LogSeverity::Info, msgString);
        }

        m_p->m_wsClient->subscribe(subscriptionFilter);
    }

    m_p->m_wsClient->run();
}

void WSStreamManager::subscribeCandlestickStream(const std::string& pair, const CandleInterval interval) const {
    std::string subscriptionFilter = "kline.";
    subscriptionFilter.append(magic_enum::enum_name(interval));
    subscriptionFilter.append(".");
    subscriptionFilter.append(pair);

    if (!m_p->m_wsClient->isSubscribed(subscriptionFilter)) {
        if (m_p->m_logMessageCB) {
            const auto msgString = fmt::format("subscribing: {}", subscriptionFilter);
            m_p->m_logMessageCB(LogSeverity::Info, msgString);
        }

        m_p->m_wsClient->subscribe(subscriptionFilter);
    }

    m_p->m_wsClient->run();
}

void WSStreamManager::setTimeout(const int seconds) const {
    m_p->m_timeout = seconds;
}

int WSStreamManager::timeout() const {
    return m_p->m_timeout;
}

void WSStreamManager::setLoggerCallback(const onLogMessage& onLogMessageCB) const {
    m_p->m_logMessageCB = onLogMessageCB;
    m_p->m_wsClient->setLoggerCallback(onLogMessageCB);
}

std::optional<EventTicker> WSStreamManager::readEventTicker(const std::string& pair) const {
    int numTries = 0;
    const int maxNumTries = static_cast<int>(m_p->m_timeout / 0.01);

    while (numTries <= maxNumTries) {
        if (m_p->m_timeout == 0) {
            /// No need to wait when destroying object
            break;
        }

        m_p->m_instrumentInfoLocker.lock();

        if (const auto it = m_p->m_tickers.find(pair); it != m_p->m_tickers.end()) {
            auto retVal = it->second;

            m_p->m_instrumentInfoLocker.unlock();
            return retVal;
        }
        m_p->m_instrumentInfoLocker.unlock();
        numTries++;
        std::this_thread::sleep_for(3ms);
    }

    return {};
}

std::optional<EventCandlestick>
WSStreamManager::readEventCandlestick(const std::string& pair, const CandleInterval interval) const {
    int numTries = 0;
    const int maxNumTries = static_cast<int>(m_p->m_timeout / 0.01);

    while (numTries <= maxNumTries) {
        if (m_p->m_timeout == 0) {
            /// No need to wait when destroying object
            break;
        }

        m_p->m_candlestickLocker.lock();

        if (const auto it = m_p->m_candlesticks.find(pair); it != m_p->m_candlesticks.end()) {
            if (const auto itCandle = it->second.find(interval); itCandle != it->second.end()) {
                auto retVal = itCandle->second;
                m_p->m_candlestickLocker.unlock();
                return retVal;
            }
        }
        m_p->m_candlestickLocker.unlock();
        numTries++;
        std::this_thread::sleep_for(3ms);
    }

    return {};
}
}
