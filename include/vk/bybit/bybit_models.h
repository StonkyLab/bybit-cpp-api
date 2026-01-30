/**
Bybit Data Models

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@gmail.com>.
*/

#ifndef INCLUDE_VK_BYBIT_MODELS_H
#define INCLUDE_VK_BYBIT_MODELS_H

#include "vk/bybit/bybit_enums.h"
#include "vk/interface/i_json.h"
#include <nlohmann/json.hpp>

namespace vk::bybit {
struct Response : IJson {
    int retCode{};
    std::string retMsg{};
    nlohmann::json retExtInfo{};
    std::int64_t time{};
    nlohmann::json result{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

struct Candle final : IJson {
    std::int64_t startTime{};
    double open{};
    double high{};
    double low{};
    double close{};
    double volume{};
    double turnover{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

struct Candles final : Response {
    Category category{Category::linear};
    std::string symbol{};
    std::vector<Candle> candles{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

struct Coin final : IJson {
    double accruedInterest{};
    double availableToBorrow{};
    double availableToWithdraw{};
    double bonus{};
    double borrowAmount{};
    std::string coin{};
    bool collateralSwitch{true};
    double cumRealisedPnl{};
    double equity{};
    double locked{};
    bool marginCollateral{true};
    double totalOrderIM{};
    double totalPositionIM{};
    double totalPositionMM{};
    double unrealisedPnl{};
    double usdValue{};
    double walletBalance{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

struct AccountBalance final : IJson {
    double accountIMRate{};
    double accountLTV{};
    double accountMMRate{};
    AccountType accountType{AccountType::UNIFIED};
    double totalAvailableBalance{};
    double totalEquity{};
    double totalInitialMargin{};
    double totalMaintenanceMargin{};
    double totalMarginBalance{};
    double totalPerpUPL{};
    double totalWalletBalance{};
    std::vector<Coin> coins{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

struct WalletBalance final : Response {
    std::vector<AccountBalance> balances{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

struct ServerTime final : Response {
    std::int64_t timeSecond{};
    std::int64_t timeNano{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

struct Position final : IJson {
    int positionIdx{};
    int riskId{};
    double riskLimitValue{};
    std::string symbol{};
    Side side{Side::Buy};
    double size{};
    double avgPrice{};
    double positionValue{};
    int tradeMode{};
    PositionStatus positionStatus{PositionStatus::Normal};
    int autoAddMargin{};
    int adlRankIndicator{};
    double leverage{};
    double positionBalance{};
    double markPrice{};
    double liqPrice{};
    double bustPrice{};
    double positionMM{};
    double positionIM{};
    TpSlMode tpSlMode{TpSlMode::Full};
    double stopLoss{};
    double takeProfit{};
    double trailingStop{};
    double unrealisedPnl{};
    double cumRealisedPnl{};
    bool isReduceOnly{false};
    std::int64_t createdTime{};
    std::int64_t updatedTime{};
    std::int64_t seq{};
    std::string mmrSysUpdateTime{};
    std::string leverageSysUpdatedTime{};
    bool zeroSize{true};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

struct Positions final : Response {
    Category category{Category::linear};
    std::vector<Position> positions{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

struct PriceFilter final : IJson {
    double minPrice{};
    double maxPrice{};
    double tickSize{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

struct LeverageFilter final : IJson {
    double minLeverage{};
    double maxLeverage{};
    double leverageStep{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

struct LotSizeFilter final : IJson {
    double maxOrderQty{};
    double minOrderQty{};
    double qtyStep{};
    double postOnlyMaxTradingQty{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

struct Instrument final : IJson {
    std::string symbol{};
    ContractType contractType{ContractType::LinearPerpetual};
    ContractStatus contractStatus{ContractStatus::Trading};
    std::string baseCoin{};
    std::string quoteCoin{};
    std::int64_t launchTime{};
    std::int64_t deliveryTime{};
    double deliveryFeeRate{};
    int priceScale{};
    bool unifiedMarginTrade{true};
    int fundingInterval{};
    std::string settleCoin{};
    LeverageFilter leverageFilter{};
    PriceFilter priceFilter{};
    LotSizeFilter lotSizeFilter{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

struct Instruments final : Response {
    Category category{Category::linear};
    std::vector<Instrument> instruments{};
    std::string nextPageCursor{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

struct Order final : IJson {
    Category category{Category::linear};
    std::string symbol{};
    Side side{Side::Buy};
    OrderType orderType{OrderType::Market};
    double qty{};
    double price{};
    TimeInForce timeInForce{TimeInForce::GTC};
    std::int64_t positionIdx{};
    std::string orderLinkId{};
    double takeProfit{};
    double stopLoss{};
    TriggerPriceType tpTriggerBy{TriggerPriceType::LastPrice};
    TriggerPriceType slTriggerBy{TriggerPriceType::LastPrice};
    bool reduceOnly{false};
    bool closeOnTrigger{false};

    /// priceStep is not part of Bybit API, it serves for formatting only
    double priceStep{0.001};

    /// m_qtyStep is not part of Bybit API, it serves for formatting only
    double qtyStep{0.001};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

struct OrderId final : Response {
    std::string orderId{};
    std::string orderLinkId{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

struct OrderResponse final : IJson {
    std::string orderId{};
    std::string orderLinkId{};
    std::string symbol{};
    double price{};
    double qty{};
    Side side{Side::Buy};
    std::int64_t positionIdx{};
    OrderStatus orderStatus{OrderStatus::Created};
    std::string rejectReason{};
    double avgPrice{};
    double cumExecQty{};
    double cumExecValue{};
    double cumExecFee{};
    TimeInForce timeInForce{TimeInForce::GTC};
    OrderType orderType{OrderType::Market};
    bool reduceOnly{false};
    bool closeOnTrigger{false};
    double lastPriceOnCreated{};
    std::string createdTime{};
    std::string updatedTime{};
    double takeProfit{};
    double stopLoss{};
    TriggerPriceType tpTriggerBy{TriggerPriceType::LastPrice};
    TriggerPriceType slTriggerBy{TriggerPriceType::LastPrice};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

struct OrdersResponse final : Response {
    Category category{Category::linear};
    std::vector<OrderResponse> orders{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

struct FundingRate final : IJson {
    std::string symbol{};
    double fundingRate{};
    std::int64_t fundingRateTimestamp{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

struct FundingRates final : Response {
    Category category{Category::linear};
    std::vector<FundingRate> fundingRates{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

struct Ticker final : IJson {
    std::string symbol{};
    double lastPrice{};
    double indexPrice{};
    double markPrice{};
    double prevPrice24h{};
    double price24hPcnt{};
    double highPrice24h{};
    double prevPrice1h{};
    std::int64_t openInterest{};
    double openInterestValue{};
    double turnover24h{};
    double volume24h{};
    double fundingRate{};
    std::int64_t nextFundingTime{};
    double ask1Size{};
    double bid1Price{};
    double ask1Price{};
    double bid1Size{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

struct Tickers final : Response {
    Category category{Category::linear};
    std::vector<Ticker> tickers{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};
}

#endif //INCLUDE_VK_BYBIT_MODELS_H
