/**
Bybit Exchange Connector

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@gmail.com>.
*/

#include <vk/bybit/bybit_exchange_connector.h>
#include "vk/bybit/bybit_rest_client.h"

namespace vk {
struct BybitFuturesExchangeConnector::P {
    std::shared_ptr<bybit::RESTClient> restClient{};
};

BybitFuturesExchangeConnector::BybitFuturesExchangeConnector() : m_p(std::make_unique<P>()) {
}

BybitFuturesExchangeConnector::~BybitFuturesExchangeConnector() {
    m_p->restClient.reset();
}

std::string BybitFuturesExchangeConnector::name() const {
    return std::string(magic_enum::enum_name(ExchangeId::MEXCFutures));
}

std::string BybitFuturesExchangeConnector::version() const {
    return "1.0.4";
}

void BybitFuturesExchangeConnector::setLoggerCallback(const onLogMessage &onLogMessageCB) {
}

void BybitFuturesExchangeConnector::login(const std::tuple<std::string, std::string, std::string> &credentials) {
    m_p->restClient.reset();
    m_p->restClient = std::make_shared<bybit::RESTClient>(std::get<0>(credentials),
                                                                  std::get<1>(credentials));
}

Trade BybitFuturesExchangeConnector::placeOrder(const Order &order) {
    Trade retVal;
    throw std::runtime_error("Unimplemented: BybitFuturesExchangeConnector::placeOrder");
}

TickerPrice BybitFuturesExchangeConnector::getTickerPrice(const std::string &symbol) const {
    TickerPrice retVal;
    throw std::runtime_error("Unimplemented: BybitFuturesExchangeConnector::getTickerPrice");
}

Balance BybitFuturesExchangeConnector::getAccountBalance(const std::string &currency) const {
    Balance retVal;
    throw std::runtime_error("Unimplemented: BybitFuturesExchangeConnector::getAccountBalance");
}

FundingRate BybitFuturesExchangeConnector::getLastFundingRate(const std::string &symbol) const {
    // const auto fr = m_p->restClient->getLastFundingRate(symbol);
    // return {fr.m_symbol, fr.m_fundingRate, fr.m_fundingTime};
    return {};
}

std::vector<FundingRate> BybitFuturesExchangeConnector::getFundingRates(
    const std::string &symbol, const std::int64_t startTime, const std::int64_t endTime) const {
    std::vector<FundingRate> retVal;
    //
    // for (const auto fRates = m_p->restClient->getFundingRates(symbol, startTime, endTime); const auto& fr: fRates) {
    //     retVal.push_back({fr.m_symbol, fr.m_fundingRate, fr.m_fundingTime, {}});
    // }
    return retVal;
}

std::vector<Ticker> BybitFuturesExchangeConnector::getTickerInfo(const std::string &symbol) const {
    throw std::runtime_error("Unimplemented: BybitFuturesExchangeConnector::getTickerInfo");
}

std::int64_t BybitFuturesExchangeConnector::getServerTime() const {
    return m_p->restClient->getServerTime();
}
}
