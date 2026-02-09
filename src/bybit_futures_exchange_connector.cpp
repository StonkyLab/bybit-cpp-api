/**
Bybit Futures Exchange Connector

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@gmail.com>.
*/

#include <vk/bybit/bybit_futures_exchange_connector.h>
#include "vk/bybit/bybit_rest_client.h"

namespace vk {
struct BybitFuturesExchangeConnector::P {
    std::unique_ptr<bybit::RESTClient> restClient{};
};

BybitFuturesExchangeConnector::BybitFuturesExchangeConnector() : m_p(std::make_unique<P>()) { m_p->restClient = std::make_unique<bybit::RESTClient>("", ""); }

BybitFuturesExchangeConnector::~BybitFuturesExchangeConnector() { m_p->restClient.reset(); }

std::string BybitFuturesExchangeConnector::exchangeId() const { return std::string(magic_enum::enum_name(ExchangeId::BybitFutures)); }

std::string BybitFuturesExchangeConnector::version() const { return "1.0.4"; }

void BybitFuturesExchangeConnector::setLoggerCallback(const onLogMessage& onLogMessageCB) {}

void BybitFuturesExchangeConnector::login(const std::tuple<std::string, std::string, std::string>& credentials) {
    m_p->restClient.reset();
    m_p->restClient = std::make_unique<bybit::RESTClient>(std::get<0>(credentials), std::get<1>(credentials));
}

Trade BybitFuturesExchangeConnector::placeOrder(const Order& order) {
    Trade retVal;
    throw std::runtime_error("Unimplemented: BybitFuturesExchangeConnector::placeOrder");
}

TickerPrice BybitFuturesExchangeConnector::getTickerPrice(const std::string& symbol) const {
    TickerPrice retVal;

    for (const auto tickerResponse = m_p->restClient->getTickers(bybit::Category::linear, symbol); const auto& ticker: tickerResponse.tickers) {
        if (ticker.symbol == symbol) {
            retVal.askPrice = ticker.ask1Price;
            retVal.bidPrice = ticker.bid1Price;
            retVal.askQty = ticker.ask1Size;
            retVal.bidQty = ticker.bid1Size;
            retVal.time = tickerResponse.time;
        }
    }

    return retVal;
}

Balance BybitFuturesExchangeConnector::getAccountBalance(const std::string& currency) const {
    Balance retVal;
    throw std::runtime_error("Unimplemented: BybitFuturesExchangeConnector::getAccountBalance");
}

FundingRate BybitFuturesExchangeConnector::getFundingRate(const std::string& symbol) const {
    if (const auto tickerList = m_p->restClient->getTickers(bybit::Category::linear, symbol).tickers; !tickerList.empty()) {
        return {tickerList[0].symbol, tickerList[0].fundingRate, tickerList[0].nextFundingTime};
    }

    return {};
}

std::vector<FundingRate> BybitFuturesExchangeConnector::getFundingRates() const {
    std::vector<FundingRate> retVal;

    for (const auto& ticker: m_p->restClient->getTickers(bybit::Category::linear, "").tickers) {
        FundingRate fr = {ticker.symbol, ticker.fundingRate, ticker.nextFundingTime};
        retVal.push_back(fr);
    }

    return retVal;
}

std::vector<Ticker> BybitFuturesExchangeConnector::getTickerInfo(const std::string& symbol) const {
    throw std::runtime_error("Unimplemented: BybitFuturesExchangeConnector::getTickerInfo");
}

std::int64_t BybitFuturesExchangeConnector::getServerTime() const { return m_p->restClient->getServerTime(); }

std::vector<Position> BybitFuturesExchangeConnector::getPositionInfo(const std::string& symbol) const {
    std::vector<Position> retVal;

    for (const auto positions = m_p->restClient->getPositionInfo(bybit::Category::linear, symbol); const auto& bybitPosition: positions) {
        Position position;
        position.symbol = bybitPosition.symbol;
        position.avgPrice = bybitPosition.avgPrice;
        position.createdTime = bybitPosition.createdTime;
        position.updatedTime = bybitPosition.updatedTime;
        position.leverage = bybitPosition.leverage;
        position.value = bybitPosition.positionValue;

        if (bybitPosition.side == bybit::Side::Buy) {
            position.side = Side::Buy;
        } else {
            position.side = Side::Sell;
        }

        retVal.push_back(position);
    }

    return retVal;
}

namespace {
bybit::CandleInterval toBybitInterval(const CandleInterval interval) {
    switch (interval) {
        case CandleInterval::_1m: return bybit::CandleInterval::_1;
        case CandleInterval::_3m: return bybit::CandleInterval::_3;
        case CandleInterval::_5m: return bybit::CandleInterval::_5;
        case CandleInterval::_15m: return bybit::CandleInterval::_15;
        case CandleInterval::_30m: return bybit::CandleInterval::_30;
        case CandleInterval::_1h: return bybit::CandleInterval::_60;
        case CandleInterval::_2h: return bybit::CandleInterval::_120;
        case CandleInterval::_4h: return bybit::CandleInterval::_240;
        case CandleInterval::_6h: return bybit::CandleInterval::_360;
        case CandleInterval::_12h: return bybit::CandleInterval::_720;
        case CandleInterval::_1d: return bybit::CandleInterval::_D;
        case CandleInterval::_1w: return bybit::CandleInterval::_W;
        case CandleInterval::_1M: return bybit::CandleInterval::_M;
        default: return bybit::CandleInterval::_60;
    }
}
}  // namespace

std::vector<FundingRate> BybitFuturesExchangeConnector::getHistoricalFundingRates(
    const std::string& symbol, const std::int64_t startTime, const std::int64_t endTime) const {
    std::vector<FundingRate> retVal;

    for (const auto& bybitFr : m_p->restClient->getFundingRates(bybit::Category::linear, symbol, startTime, endTime)) {
        FundingRate fr;
        fr.symbol = bybitFr.symbol;
        fr.fundingRate = bybitFr.fundingRate;
        fr.fundingTime = bybitFr.fundingRateTimestamp;
        retVal.push_back(fr);
    }

    // Bybit returns newest first, we need chronological order
    std::ranges::sort(retVal, [](const auto& a, const auto& b) { return a.fundingTime < b.fundingTime; });

    return retVal;
}

std::vector<Candle> BybitFuturesExchangeConnector::getHistoricalCandles(
    const std::string& symbol, const CandleInterval interval,
    const std::int64_t startTime, const std::int64_t endTime) const {
    std::vector<Candle> retVal;

    // REST client handles pagination internally
    for (const auto& bybitCandle : m_p->restClient->getHistoricalPrices(
             bybit::Category::linear, symbol, toBybitInterval(interval), startTime, endTime)) {
        Candle c;
        c.openTime = bybitCandle.startTime;
        c.open = bybitCandle.open;
        c.high = bybitCandle.high;
        c.low = bybitCandle.low;
        c.close = bybitCandle.close;
        c.volume = bybitCandle.volume;
        retVal.push_back(c);
    }

    return retVal;
}

} // namespace vk
