/**
Bybit Exchange Connector

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@gmail.com>.
*/

#include <vk/bybit/bybit_futures_exchange_connector.h>
#include "vk/bybit/bybit_rest_client.h"

namespace vk {
struct BybitFuturesExchangeConnector::P {
    std::unique_ptr<bybit::RESTClient> m_restClient{};
};

BybitFuturesExchangeConnector::BybitFuturesExchangeConnector() : m_p(std::make_unique<P>()) {
    m_p->m_restClient = std::make_unique<bybit::RESTClient>("","");
}

BybitFuturesExchangeConnector::~BybitFuturesExchangeConnector() {
    m_p->m_restClient.reset();
}

std::string BybitFuturesExchangeConnector::exchangeId() const {
    return std::string(magic_enum::enum_name(ExchangeId::MEXCFutures));
}

std::string BybitFuturesExchangeConnector::version() const {
    return "1.0.4";
}

void BybitFuturesExchangeConnector::setLoggerCallback(const onLogMessage& onLogMessageCB) {
}

void BybitFuturesExchangeConnector::login(const std::tuple<std::string, std::string, std::string>& credentials) {
    m_p->m_restClient.reset();
    m_p->m_restClient = std::make_unique<bybit::RESTClient>(std::get<0>(credentials),
                                                          std::get<1>(credentials));
}

Trade BybitFuturesExchangeConnector::placeOrder(const Order& order) {
    Trade retVal;
    throw std::runtime_error("Unimplemented: BybitFuturesExchangeConnector::placeOrder");
}

TickerPrice BybitFuturesExchangeConnector::getTickerPrice(const std::string& symbol) const {
    TickerPrice retVal;

    for (const auto tickers = m_p->m_restClient->getTickers(bybit::Category::linear, symbol); const auto& ticker : tickers.m_tickers) {

        if (ticker.m_symbol == symbol) {
            retVal.askPrice = ticker.m_ask1Price;
            retVal.bidPrice = ticker.m_bid1Price;
            retVal.askQty = ticker.m_ask1Size;
            retVal.bidQty = ticker.m_bid1Size;
            retVal.time = tickers.m_time;
        }
    }

    return retVal;
}

Balance BybitFuturesExchangeConnector::getAccountBalance(const std::string& currency) const {
    Balance retVal;
    throw std::runtime_error("Unimplemented: BybitFuturesExchangeConnector::getAccountBalance");
}

FundingRate BybitFuturesExchangeConnector::getFundingRate(const std::string& symbol) const {
    if (const auto tickers = m_p->m_restClient->getTickers(bybit::Category::linear, symbol).m_tickers; !tickers.empty()) {
        return {tickers[0].m_symbol, tickers[0].m_fundingRate, tickers[0].m_nextFundingTime};
    }

    return {};
}

std::vector<FundingRate> BybitFuturesExchangeConnector::getFundingRates() const {
    std::vector<FundingRate> retVal;

    for (const auto& ticker : m_p->m_restClient->getTickers(bybit::Category::linear, "").m_tickers) {
        FundingRate fr = {ticker.m_symbol, ticker.m_fundingRate, ticker.m_nextFundingTime};
        retVal.push_back(fr);
    }

    return retVal;
}

std::vector<Ticker> BybitFuturesExchangeConnector::getTickerInfo(const std::string& symbol) const {
    throw std::runtime_error("Unimplemented: BybitFuturesExchangeConnector::getTickerInfo");
}

std::int64_t BybitFuturesExchangeConnector::getServerTime() const {
    return m_p->m_restClient->getServerTime();
}
}
