/**
Bybit Spot Exchange Connector

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@gmail.com>.
*/

#include <vk/bybit/bybit_spot_exchange_connector.h>
#include "vk/bybit/bybit_rest_client.h"

namespace vk {
struct BybitSpotExchangeConnector::P {
    std::unique_ptr<bybit::RESTClient> restClient{};
};

BybitSpotExchangeConnector::BybitSpotExchangeConnector() : m_p(std::make_unique<P>()) { m_p->restClient = std::make_unique<bybit::RESTClient>("", ""); }

BybitSpotExchangeConnector::~BybitSpotExchangeConnector() { m_p->restClient.reset(); }

std::string BybitSpotExchangeConnector::exchangeId() const { return std::string(magic_enum::enum_name(ExchangeId::BybitSpot)); }

std::string BybitSpotExchangeConnector::version() const { return "1.0.4"; }

void BybitSpotExchangeConnector::setLoggerCallback(const onLogMessage& onLogMessageCB) {}

void BybitSpotExchangeConnector::login(const std::tuple<std::string, std::string, std::string>& credentials) {
    m_p->restClient.reset();
    m_p->restClient = std::make_unique<bybit::RESTClient>(std::get<0>(credentials), std::get<1>(credentials));
}

Trade BybitSpotExchangeConnector::placeOrder(const Order& order) {
    Trade retVal;
    throw std::runtime_error("Unimplemented: BybitSpotExchangeConnector::placeOrder");
}

TickerPrice BybitSpotExchangeConnector::getTickerPrice(const std::string& symbol) const {
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

Balance BybitSpotExchangeConnector::getAccountBalance(const std::string& currency) const {
    Balance retVal;
    throw std::runtime_error("Unimplemented: BybitSpotExchangeConnector::getAccountBalance");
}

FundingRate BybitSpotExchangeConnector::getFundingRate(const std::string& symbol) const {
    throw std::runtime_error("BybitSpotExchangeConnector::getFundingRate - SPOT does not have funding rates");
}

std::vector<FundingRate> BybitSpotExchangeConnector::getFundingRates() const {
    throw std::runtime_error("BybitSpotExchangeConnector::getFundingRates - SPOT does not have funding rates");
}

std::vector<Ticker> BybitSpotExchangeConnector::getTickerInfo(const std::string& symbol) const {
    throw std::runtime_error("Unimplemented: BybitSpotExchangeConnector::getTickerInfo");
}

std::int64_t BybitSpotExchangeConnector::getServerTime() const { return m_p->restClient->getServerTime(); }

std::vector<Position> BybitSpotExchangeConnector::getPositionInfo(const std::string& symbol) const {
    std::vector<Position> retVal;

    for (const auto positions = m_p->restClient->getPositionInfo(bybit::Category::spot, symbol); const auto& bybitPosition: positions) {
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

} // namespace vk
