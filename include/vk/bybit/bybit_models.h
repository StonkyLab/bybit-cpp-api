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
    int m_retCode{};
    std::string m_retMsg{};
    nlohmann::json m_retExtInfo{};
    std::int64_t m_time{};
    nlohmann::json m_result{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

struct Candle final : IJson {
    std::int64_t m_startTime{};
    double m_open{};
    double m_high{};
    double m_low{};
    double m_close{};
    double m_volume{};
    double m_turnover{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

struct Candles final : Response {
    Category m_category{Category::linear};
    std::string m_symbol{};
    std::vector<Candle> m_candles{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

struct Coin final : IJson {
    double m_accruedInterest{};
    double m_availableToBorrow{};
    double m_availableToWithdraw{};
    double m_bonus{};
    double m_borrowAmount{};
    std::string m_coin{};
    bool m_collateralSwitch{true};
    double m_cumRealisedPnl{};
    double m_equity{};
    double m_locked{};
    bool m_marginCollateral{true};
    double m_totalOrderIM{};
    double m_totalPositionIM{};
    double m_totalPositionMM{};
    double m_unrealisedPnl{};
    double m_usdValue{};
    double m_walletBalance{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

struct AccountBalance final : IJson {
    double m_accountIMRate{};
    double m_accountLTV{};
    double m_accountMMRate{};
    AccountType m_accountType{AccountType::UNIFIED};
    double m_totalAvailableBalance{};
    double m_totalEquity{};
    double m_totalInitialMargin{};
    double m_totalMaintenanceMargin{};
    double m_totalMarginBalance{};
    double m_totalPerpUPL{};
    double m_totalWalletBalance{};
    std::vector<Coin> m_coins{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

struct WalletBalance final : Response {
    std::vector<AccountBalance> m_balances{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

struct ServerTime final : Response {
    std::int64_t m_timeSecond{};
    std::int64_t m_timeNano{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

struct Position final : IJson {
    int m_positionIdx{};
    int m_riskId{};
    double m_riskLimitValue{};
    std::string m_symbol{};
    Side m_side{Side::Buy};
    double m_size{};
    double m_avgPrice{};
    double m_positionValue{};
    int m_tradeMode{};
    PositionStatus m_positionStatus{PositionStatus::Normal};
    int m_autoAddMargin{};
    int m_adlRankIndicator{};
    double m_leverage{};
    double m_positionBalance{};
    double m_markPrice{};
    double m_liqPrice{};
    double m_bustPrice{};
    double m_positionMM{};
    double m_positionIM{};
    TpSlMode m_tpSlMode{TpSlMode::Full};
    double m_stopLoss{};
    double m_takeProfit{};
    double m_trailingStop{};
    double m_unrealisedPnl{};
    double m_cumRealisedPnl{};
    bool m_isReduceOnly{false};
    std::int64_t m_createdTime{};
    std::int64_t m_updatedTime{};
    std::int64_t m_seq{};
    std::string m_mmrSysUpdateTime{};
    std::string m_leverageSysUpdatedTime{};
    bool m_zeroSize{true};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

struct Positions final : Response {
    Category m_category{Category::linear};
    std::vector<Position> m_positions{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

struct PriceFilter final : IJson {
    double m_minPrice{};
    double m_maxPrice{};
    double m_tickSize{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

struct LeverageFilter final : IJson {
    double m_minLeverage{};
    double m_maxLeverage{};
    double m_leverageStep{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

struct LotSizeFilter final : IJson {
    double m_maxOrderQty{};
    double m_minOrderQty{};
    double m_qtyStep{};
    double m_postOnlyMaxTradingQty{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

struct Instrument final : IJson {
    std::string m_symbol{};
    ContractType m_contractType{ContractType::LinearPerpetual};
    ContractStatus m_contractStatus{ContractStatus::Trading};
    std::string m_baseCoin{};
    std::string m_quoteCoin{};
    std::int64_t m_launchTime{};
    std::int64_t m_deliveryTime{};
    double m_deliveryFeeRate{};
    int m_priceScale{};
    bool m_unifiedMarginTrade{true};
    int m_fundingInterval{};
    std::string m_settleCoin{};

    LeverageFilter m_leverageFilter{};
    PriceFilter m_priceFilter{};
    LotSizeFilter m_lotSizeFilter{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

struct Instruments final : Response {
    Category m_category{Category::linear};
    std::vector<Instrument> m_instruments{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

struct Order final : IJson {
    Category m_category{Category::linear};
    std::string m_symbol{};
    Side m_side{Side::Buy};
    OrderType m_orderType{OrderType::Market};
    double m_qty{};
    double m_price{};
    TimeInForce m_timeInForce{TimeInForce::GTC};
    std::int64_t m_positionIdx{};
    std::string m_orderLinkId{};
    double m_takeProfit{};
    double m_stopLoss{};
    TriggerPriceType m_tpTriggerBy{TriggerPriceType::LastPrice};
    TriggerPriceType m_slTriggerBy{TriggerPriceType::LastPrice};
    bool m_reduceOnly{false};
    bool m_closeOnTrigger{false};

    /// priceStep is not part of Bybit API, it serves for formatting only
    double m_priceStep{0.001};

    /// m_qtyStep is not part of Bybit API, it serves for formatting only
    double m_qtyStep{0.001};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

struct OrderId final : Response {
    std::string m_orderId{};
    std::string m_orderLinkId{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

struct OrderResponse final : IJson {
    std::string m_orderId{};
    std::string m_orderLinkId{};
    std::string m_symbol{};
    double m_price{};
    double m_qty{};
    Side m_side{Side::Buy};
    std::int64_t m_positionIdx{};
    OrderStatus m_orderStatus{OrderStatus::Created};
    std::string m_rejectReason{};
    double m_avgPrice{};
    double m_cumExecQty{};
    double m_cumExecValue{};
    double m_cumExecFee{};
    TimeInForce m_timeInForce{TimeInForce::GTC};
    OrderType m_orderType{OrderType::Market};
    bool m_reduceOnly{false};
    bool m_closeOnTrigger{false};
    double m_lastPriceOnCreated{};
    std::string m_createdTime{};
    std::string m_updatedTime{};
    double m_takeProfit{};
    double m_stopLoss{};
    TriggerPriceType m_tpTriggerBy{TriggerPriceType::LastPrice};
    TriggerPriceType m_slTriggerBy{TriggerPriceType::LastPrice};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

struct OrdersResponse final : Response {
    Category m_category{Category::linear};
    std::vector<OrderResponse> m_orders{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};
}

#endif //INCLUDE_VK_BYBIT_MODELS_H
