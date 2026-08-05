/**
Bybit Futures WebSocket Stream manager v5

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.
*/

#include "stonky/bybit/bybit_rest_client.h"
#include "stonky/bybit/bybit_ws_stream_manager.h"
#include "stonky/bybit/bybit_ws_client.h"
#include "stonky/utils/utils.h"
#include <boost/algorithm/string/case_conv.hpp>
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

using namespace std::chrono_literals;

namespace stonky::bybit {
/// Granularity of the polling loops of the read operations
static constexpr auto READ_POLL_INTERVAL = 3ms;

struct WSStreamManager::P {
    std::unique_ptr<WebSocketClient> wsClient;
    std::atomic<int> timeout{5};
    std::atomic<int> maxTickAge{60};
    mutable std::recursive_mutex instrumentInfoLocker;
    mutable std::recursive_mutex candlestickLocker;
    std::map<std::string, EventTicker> tickers;
    std::map<std::string, std::map<CandleInterval, EventCandlestick>> candlesticks;
    std::weak_ptr<RESTClient> restClient;
    onLogMessage logMessageCB;
    onTickerUpdate tickerUpdateCB;

    /// Bybit sends symbols in upper case, normalize the map keys so that a lower case query still hits the entry
    static std::string normalizeSymbol(const std::string& pair) { return boost::algorithm::to_upper_copy(pair); }

    /// The same clock EventTicker::receivedTimestamp is stamped with
    static std::int64_t steadyNowMs() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    /// The logger callback is optional, never call it unconditionally
    void log(const LogSeverity severity, const std::string& message) const {
        if (logMessageCB) {
            logMessageCB(severity, message);
        }
    }

    [[nodiscard]] bool isStale(const EventTicker& ticker) const {
        const auto maxAge = maxTickAge.load();

        if (maxAge <= 0) {
            return false;
        }

        return ticker.receivedTimestamp <= 0 || steadyNowMs() - ticker.receivedTimestamp > static_cast<std::int64_t>(maxAge) * 1000;
    }

    /**
     * Fetch the current quote over REST and store it in the ticker cache. Used both for seeding a fresh
     * subscription and for refreshing an entry that went stale because the stream is silent or dead.
     * @param symbol normalized (upper case) symbol
     * @return the stored EventTicker or bad option when the snapshot could not be obtained
     */
    std::optional<EventTicker> refreshTickerFromREST(const std::string& symbol) {
        const auto client = restClient.lock();

        if (!client) {
            return {};
        }

        Ticker snapshot;

        try {
            const auto tickersResponse = client->getTickers(Category::linear, symbol);

            if (tickersResponse.tickers.empty()) {
                log(LogSeverity::Warning, fmt::format("{}: REST ticker snapshot for {} returned nothing", MAKE_FILELINE, symbol));
                return {};
            }

            snapshot = tickersResponse.tickers.front();
        } catch (std::exception& e) {
            log(LogSeverity::Error, fmt::format("{}: REST ticker snapshot for {} failed: {}", MAKE_FILELINE, symbol, e.what()));
            return {};
        }

        if (snapshot.ask1Price <= 0.0 || snapshot.bid1Price <= 0.0) {
            log(LogSeverity::Warning, fmt::format("{}: REST ticker snapshot for {} has no valid bid/ask", MAKE_FILELINE, symbol));
            return {};
        }

        EventTicker eventTicker;
        eventTicker.symbol = symbol;
        eventTicker.ask1Price = snapshot.ask1Price;
        eventTicker.ask1Size = snapshot.ask1Size;
        eventTicker.bid1Price = snapshot.bid1Price;
        eventTicker.bid1Size = snapshot.bid1Size;
        eventTicker.lastPrice = snapshot.lastPrice;
        eventTicker.receivedTimestamp = steadyNowMs();

        std::lock_guard lk(instrumentInfoLocker);
        tickers.insert_or_assign(symbol, eventTicker);
        return eventTicker;
    }

    explicit P() : wsClient(std::make_unique<WebSocketClient>()) {
        wsClient->setDataEventCallback([&](const Event& event) {
            if (event.topic.find("tickers") != std::string::npos) {
                EventTicker merged;
                bool haveUpdate = false;

                {
                    std::lock_guard lk(instrumentInfoLocker);

                    try {
                        const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();

                        const auto symbol = normalizeSymbol(readSymbolFromFilter(event.topic));

                        if (const auto it = tickers.find(symbol); it == tickers.end()) {
                            EventTicker eventTicker;
                            eventTicker.loadEventData(event);
                            /// A delta message carries no symbol, take it from the topic so the entry is complete
                            eventTicker.symbol = symbol;
                            eventTicker.receivedTimestamp = nowMs;
                            tickers.insert_or_assign(symbol, eventTicker);
                            merged = eventTicker;
                            haveUpdate = true;
                        } else {
                            it->second.loadEventData(event);
                            it->second.symbol = symbol;
                            it->second.receivedTimestamp = nowMs;
                            merged = it->second;
                            haveUpdate = true;
                        }
                    } catch (std::exception& e) {
                        log(LogSeverity::Error, fmt::format("{}: {}", MAKE_FILELINE, e.what()));
                    }
                }

                /// Outside the lock — the callback may take its own locks.
                if (haveUpdate && tickerUpdateCB) {
                    tickerUpdateCB(merged);
                }
            } else if (event.topic.find("kline") != std::string::npos) {
                std::lock_guard lk(candlestickLocker);

                try {
                    EventCandlestick eventCandlestick;
                    if (const auto candlesNumber = event.data.size(); candlesNumber != 1) {
                        log(LogSeverity::Error, fmt::format("{}: {}: {}", MAKE_FILELINE, "unexpected candles number", candlesNumber));
                    }

                    eventCandlestick.fromJson(event.data[0]);

                    /// Insert new candle
                    {
                        const auto symbol = normalizeSymbol(readSymbolFromFilter(event.topic));
                        auto it = candlesticks.find(symbol);

                        if (it == candlesticks.end()) {
                            candlesticks.insert({symbol, {}});
                        }

                        it = candlesticks.find(symbol);
                        it->second.insert_or_assign(*magic_enum::enum_cast<CandleInterval>(eventCandlestick.interval), eventCandlestick);
                    }
                } catch (std::exception& e) {
                    log(LogSeverity::Error, fmt::format("{}: {}", MAKE_FILELINE, e.what()));
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
    /// Release the readers first, they poll on the timeout value
    m_p->timeout = 0;
    m_p->wsClient.reset();
}

void WSStreamManager::setRestClient(const std::weak_ptr<RESTClient>& restClient) const { m_p->restClient = restClient; }

void WSStreamManager::subscribeTickerStream(const std::string& pair) const {
    std::string subscriptionFilter = "tickers.";
    subscriptionFilter.append(pair);

    /// This method is called on every price poll, so anything beyond the cheap isSubscribed check must run only on
    /// the FIRST subscription of a pair - otherwise every poll would fire a REST request.
    const bool alreadySubscribed = m_p->wsClient->isSubscribed(subscriptionFilter);

    if (!alreadySubscribed) {
        m_p->log(LogSeverity::Info, fmt::format("subscribing: {}", subscriptionFilter));
        m_p->wsClient->subscribe(subscriptionFilter);
    }

    m_p->wsClient->run();

    if (!alreadySubscribed) {
        /// The first stream message can take a while - handshake, subscription confirmation and, for an illiquid
        /// symbol, the wait for something to actually change. Seed the cache from the REST snapshot instead.
        m_p->refreshTickerFromREST(P::normalizeSymbol(pair));
    }
}

void WSStreamManager::unsubscribeTickerStream(const std::string& pair) const {
    std::string subscriptionFilter = "tickers.";
    subscriptionFilter.append(pair);

    m_p->wsClient->unsubscribe(subscriptionFilter);

    std::lock_guard lk(m_p->instrumentInfoLocker);
    m_p->tickers.erase(P::normalizeSymbol(pair));
}

void WSStreamManager::setTickerUpdateCallback(const onTickerUpdate& onTickerUpdateCB) const { m_p->tickerUpdateCB = onTickerUpdateCB; }

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

void WSStreamManager::setMaxTickAge(const int seconds) const { m_p->maxTickAge = seconds; }

int WSStreamManager::maxTickAge() const { return m_p->maxTickAge; }

void WSStreamManager::setLoggerCallback(const onLogMessage& onLogMessageCB) const {
    m_p->logMessageCB = onLogMessageCB;
    m_p->wsClient->setLoggerCallback(onLogMessageCB);
}

std::optional<EventTicker> WSStreamManager::readEventTicker(const std::string& pair) const {
    const auto symbol = P::normalizeSymbol(pair);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(m_p->timeout.load());

    for (;;) {
        if (m_p->timeout == 0) {
            /// No need to wait when destroying object
            return {};
        }

        {
            std::lock_guard lk(m_p->instrumentInfoLocker);

            if (const auto it = m_p->tickers.find(symbol); it != m_p->tickers.end()) {
                if (!m_p->isStale(it->second)) {
                    return it->second;
                }

                /// The cached quote is too old - the stream is silent or dead. Do not serve it, go to REST.
                break;
            }
        }

        if (std::chrono::steady_clock::now() >= deadline) {
            break;
        }

        std::this_thread::sleep_for(READ_POLL_INTERVAL);
    }

    /// Nothing usable arrived over the stream, fall back to the REST snapshot
    return m_p->refreshTickerFromREST(symbol);
}

std::optional<EventCandlestick> WSStreamManager::readEventCandlestick(const std::string& pair, const CandleInterval interval) const {
    const auto symbol = P::normalizeSymbol(pair);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(m_p->timeout.load());

    for (;;) {
        if (m_p->timeout == 0) {
            /// No need to wait when destroying object
            break;
        }

        {
            std::lock_guard lk(m_p->candlestickLocker);

            if (const auto it = m_p->candlesticks.find(symbol); it != m_p->candlesticks.end()) {
                if (const auto itCandle = it->second.find(interval); itCandle != it->second.end()) {
                    return itCandle->second;
                }
            }
        }

        if (std::chrono::steady_clock::now() >= deadline) {
            break;
        }

        std::this_thread::sleep_for(READ_POLL_INTERVAL);
    }

    return {};
}
} // namespace stonky::bybit
