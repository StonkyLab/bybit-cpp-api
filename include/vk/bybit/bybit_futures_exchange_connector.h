/**
Bybit Futures Exchange Connector

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@gmail.com>.
*/

#ifndef INCLUDE_VK_BYBIT_FUTURES_EXCHANGE_CONNECTOR_H
#define INCLUDE_VK_BYBIT_FUTURES_EXCHANGE_CONNECTOR_H

#include "vk/interface/i_exchange_connector.h"
#include "vk/common/module_factory.h"
#include <memory>

namespace vk {
class BybitFuturesExchangeConnector final : public IExchangeConnector {
    struct P;
    std::unique_ptr<P> m_p{};

public:
    BybitFuturesExchangeConnector();

    ~BybitFuturesExchangeConnector() override;

    [[nodiscard]] std::string exchangeId() const override;

    [[nodiscard]] std::string version() const override;

    void setLoggerCallback(const onLogMessage &onLogMessageCB) override;

    void login(const std::tuple<std::string, std::string, std::string> &credentials) override;

    Trade placeOrder(const Order &order) override;

    [[nodiscard]] TickerPrice getTickerPrice(const std::string &symbol) const override;

    [[nodiscard]] Balance getAccountBalance(const std::string &currency) const override;

    [[nodiscard]] FundingRate getFundingRate(const std::string &symbol) const override;

    [[nodiscard]] std::vector<FundingRate> getFundingRates() const override;

    [[nodiscard]] std::vector<Ticker> getTickerInfo(const std::string& symbol) const override;

    [[nodiscard]] std::int64_t getServerTime() const override;

    [[nodiscard]] std::vector<Position> getPositionInfo(const std::string& symbol) const override;

    static std::shared_ptr<IExchangeConnector> createInstance() {
        return std::make_shared<BybitFuturesExchangeConnector>();
    }
};
}
#endif //INCLUDE_VK_BYBIT_FUTURES_EXCHANGE_CONNECTOR_H
