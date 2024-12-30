/**
Bybit Data Models v2

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@gmail.com>.
*/

#ifndef INCLUDE_VK_BYBIT_MODELS_V2_H
#define INCLUDE_VK_BYBIT_MODELS_V2_H

#include "vk/tools/i_json.h"
#include <nlohmann/json.hpp>
#include <vk/bybit/bybit_enums.h>

namespace vk::bybit::v2 {
struct Response : IJson {
    int m_retCode{};
    std::string m_retMsg{};
    std::string m_extCode{};
    std::string m_extInfo{};
    std::string m_timeNow{};
    std::int64_t m_rateLimitStatus{};
    std::int64_t m_rateLimitResetMs{};
    std::int64_t m_rateLimit{};
    nlohmann::json m_result{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

struct Candle final : IJson {
    std::string m_symbol{};
    std::string m_interval{};
    std::int64_t m_openTime{};
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
    std::vector<Candle> m_candles{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

struct AssetBalance final : IJson {
    double m_equity{};
    double m_availableBalance{};
    double m_usedMargin{};
    double m_orderMargin{};
    double m_positionMargin{};
    double m_occClosingFee{};
    double m_occFundingFee{};
    double m_walletBalance{};
    double m_realisedPnl{};
    double m_unrealisedPnl{};
    double m_cumRealisedPnl{};
    double m_givenCash{};
    double m_serviceCash{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

struct WalletBalance final : Response {
    std::map<std::string, AssetBalance> m_balances{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

struct Order final : IJson {
    Side m_side{Side::Buy};
    std::string m_symbol{};
    OrderType m_orderType{OrderType::Market};
    double m_qty{};
    double m_price{};
    TimeInForce m_timeInForce{TimeInForce::GoodTillCancel};
    bool m_reduceOnly{false};
    bool m_closeOnTrigger{false};
    std::string m_orderLinkId{};
    double m_takeProfit{};
    double m_stopLoss{};
    TriggerPriceType m_tpTriggerBy{TriggerPriceType::LastPrice};
    TriggerPriceType m_slTriggerBy{TriggerPriceType::LastPrice};
    std::int64_t m_positionIdx{};

    /// priceScale is not part of Bybit API, it serves for formatting only (shitty API design!)
    int m_priceScale{2};

    /// m_qtyStep is not part of Bybit API, it serves for formatting only (shitty API design!)
    double m_qtyStep{0.001};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

struct OrderResponse final : Response {
    Side m_side{Side::Buy};
    std::string m_symbol{};
    OrderType m_orderType{OrderType::Market};
    double m_qty{};
    TimeInForce m_timeInForce{TimeInForce::GoodTillCancel};
    bool m_reduceOnly{false};
    bool m_closeOnTrigger{false};
    std::int64_t m_positionIdx = 0;
    double m_price{};
    std::string m_orderId{};
    std::int64_t m_userId{};
    OrderStatus m_orderStatus{OrderStatus::Created};
    double m_lastExecPrice{};
    double m_cumExecQty{};
    double m_cumExecValue{};
    double m_cumExecFee{};
    std::string m_orderLinkId{};
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
    std::vector<OrderResponse> m_orders;

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

struct Position final : IJson {
    std::int64_t m_userId = 0;
    std::string m_symbol{};
    Side m_side{Side::Buy};
    double m_size{};
    double m_positionValue{};
    double m_entryPrice{};
    double m_liqPrice{};
    double m_bustPrice{};
    double m_leverage{};
    double m_autoAddMargin{};
    bool m_isIsolated{false};
    double m_positionMargin{};
    double m_occClosingFee{};
    double m_realisedPnl{};
    double m_cumRealisedPnl{};
    double m_freeQty{};
    TpSlMode m_tpSlMode{TpSlMode::Full};
    double m_unrealisedPnl{};
    double m_deleverageIndicator{};
    int m_riskId{};
    double m_stopLoss{};
    double m_takeProfit{};
    double m_trailingStop{};
    int m_positionIdx{};
    PositionMode m_mode{PositionMode::MergedSingle};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

struct Positions final : Response {
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
    double m_maxTradingQty{};
    double m_minTradingQty{};
    double m_qtyStep{};
    double m_postOnlyMaxTradingQty{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

struct Symbol final : IJson {
    std::string m_name{};
    std::string m_alias{};
    ContractStatus m_status = ContractStatus::Trading;
    std::string m_baseCurrency{};
    std::string m_quoteCurrency{};
    int m_priceScale{};
    double m_takerFee{};
    double m_makerFee{};
    int m_fundingInterval{};
    LeverageFilter m_leverageFilter{};
    PriceFilter m_priceFilter{};
    LotSizeFilter m_lotSizeFilter{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

struct Symbols final : Response {
    std::vector<Symbol> m_symbols{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

struct FundingRate final : Response {
    std::string m_symbol{};
    double m_fundingRate{};
    std::string m_fundingRateTimestamp{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};
}

#endif //INCLUDE_VK_BYBIT_MODELS_V2_H
