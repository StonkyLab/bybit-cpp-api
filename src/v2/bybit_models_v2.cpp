/**
Bybit Data Models v2

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@gmail.com>.
*/

#include "vk/bybit/v2/bybit_models_v2.h"
#include "vk/utils/utils.h"
#include "vk/utils/json_utils.h"
#include <boost/multiprecision/cpp_dec_float.hpp>
#include <vk/bybit/bybit_enums.h>

namespace vk::bybit::v2 {

nlohmann::json Response::toJson() const {
    throw std::runtime_error("Unimplemented: Response::toJson()");
}

void Response::fromJson(const nlohmann::json &json) {
    readValue<int>(json, "ret_code", m_retCode);
    readValue<std::string>(json, "ret_msg", m_retMsg);
    readValue<std::string>(json, "ext_code", m_extCode);
    readValue<std::string>(json, "ext_info", m_extInfo);
    readValue<std::string>(json, "time_now", m_timeNow);
    readValue<std::int64_t>(json, "rate_limit_status", m_rateLimitStatus);
    readValue<std::int64_t>(json, "rate_limit_reset_ms", m_rateLimitResetMs);
    readValue<std::int64_t>(json, "rate_limit", m_rateLimit);
    m_result = json["result"];
}

nlohmann::json Candle::toJson() const {
    throw std::runtime_error("Unimplemented: Candle::toJson()");
}

void Candle::fromJson(const nlohmann::json &json) {

    readValue<std::string>(json, "symbol", m_symbol);
    readValue<std::string>(json, "interval", m_interval);
    readValue<std::int64_t>(json, "open_time", m_openTime);
    readValue<double>(json, "open", m_open);
    readValue<double>(json, "high", m_high);
    readValue<double>(json, "low", m_low);
    readValue<double>(json, "close", m_close);
    readValue<double>(json, "volume", m_volume);
    readValue<double>(json, "turnover", m_turnover);
}

nlohmann::json Candles::toJson() const {
    throw std::runtime_error("Unimplemented: Candles::toJson()");
}

void Candles::fromJson(const nlohmann::json &json) {
    Response::fromJson(json);

    for (const auto &el: m_result.items()) {
        Candle candle;
        candle.fromJson(el.value());
        m_candles.push_back(candle);
    }
}

nlohmann::json AssetBalance::toJson() const {
    throw std::runtime_error("Unimplemented: AssetBalance::toJson()");
}

void AssetBalance::fromJson(const nlohmann::json &json) {
    readValue<double>(json, "equity", m_equity);
    readValue<double>(json, "available_balance", m_availableBalance);
    readValue<double>(json, "used_margin", m_usedMargin);
    readValue<double>(json, "order_margin", m_orderMargin);
    readValue<double>(json, "position_margin", m_positionMargin);
    readValue<double>(json, "occ_closing_fee", m_occClosingFee);
    readValue<double>(json, "occ_funding_fee", m_occFundingFee);
    readValue<double>(json, "wallet_balance", m_walletBalance);
    readValue<double>(json, "realised_pnl", m_realisedPnl);
    readValue<double>(json, "unrealised_pnl", m_unrealisedPnl);
    readValue<double>(json, "cum_realised_pnl", m_cumRealisedPnl);
    readValue<double>(json, "given_cash", m_givenCash);
    readValue<double>(json, "service_cash", m_serviceCash);
}

nlohmann::json WalletBalance::toJson() const {
    throw std::runtime_error("Unimplemented: WalletBalance::toJson()");
}

void WalletBalance::fromJson(const nlohmann::json &json) {
    Response::fromJson(json);

    for (const auto &el: m_result.items()) {
        AssetBalance assetBalance;
        assetBalance.fromJson(el.value());
        m_balances.insert_or_assign(el.key(), assetBalance);
    }
}

nlohmann::json Order::toJson() const {
    nlohmann::json json;

    json["side"] = m_side;
    json["symbol"] = m_symbol;
    json["order_type"] = m_orderType;
    json["qty"] = m_qty;
    json["time_in_force"] = m_timeInForce;
    json["reduce_only"] = m_reduceOnly;
    json["close_on_trigger"] = m_closeOnTrigger;
    json["position_idx"] = m_positionIdx;

    if (!m_orderLinkId.empty()) {
        json["order_link_id"] = m_orderLinkId;
    }

    if (m_takeProfit != 0.0) {
        json["take_profit"] = m_takeProfit;
    }

    if (m_stopLoss != 0.0) {
        json["stop_loss"] = m_stopLoss;
    }

    if (m_orderType == OrderType::Limit) {
        json["price"] = m_price;
    }

    if (m_tpTriggerBy == TriggerPriceType::LastPrice) {
        json["tp_trigger_by"] = m_tpTriggerBy;
    }

    if (m_slTriggerBy == TriggerPriceType::LastPrice) {
        json["sl_trigger_by"] = m_slTriggerBy;
    }

    /// Shitty workaround (should be a string!) - fix number of decimal in qty attribute
    const boost::multiprecision::cpp_dec_float_50 precision_dec(std::to_string(m_qtyStep));
    const auto parts = splitString(precision_dec.str(), '.');
    int tickSizePrecision = m_priceScale;

    if (parts.size() == 2) {
        tickSizePrecision = parts[1].length();
    }

    const int precision = std::max(m_priceScale, tickSizePrecision);

    auto orderStr = json.dump();

    nlohmann::json numJson;
    numJson["qty"] = m_qty;

    auto orderStrDumped = numJson.dump();
    orderStrDumped.pop_back();
    orderStrDumped.erase(0, 1);

    std::string replacement("\"qty\"");
    replacement.append(":");
    replacement.append(formatDouble(precision, m_qty));

    replaceAll(orderStr, orderStrDumped, replacement);
    return nlohmann::json::parse(orderStr);
}

void Order::fromJson(const nlohmann::json &json) {
    throw std::runtime_error("Unimplemented: Order::fromJson()");
}

nlohmann::json OrderResponse::toJson() const {
    nlohmann::json json;

    json["side"] = m_side;
    json["symbol"] = m_symbol;
    json["order_type"] = m_orderType;
    json["qty"] = m_qty;
    json["time_in_force"] = m_timeInForce;
    json["reduce_only"] = m_reduceOnly;
    json["close_on_trigger"] = m_closeOnTrigger;
    json["position_idx"] = m_positionIdx;
    json["price"] = m_price;
    json["order_id"] = m_orderId;
    json["user_id"] = m_userId;
    json["order_status"] = m_orderStatus;
    json["last_exec_price"] = m_lastExecPrice;
    json["cum_exec_qty"] = m_cumExecQty;
    json["cum_exec_value"] = m_cumExecValue;
    json["cum_exec_fee"] = m_cumExecFee;
    json["order_link_id"] = m_orderLinkId;
    json["created_time"] = m_createdTime;
    json["updated_time"] = m_updatedTime;
    json["take_profit"] = m_takeProfit;
    json["stop_loss"] = m_stopLoss;
    json["tp_trigger_by"] = m_tpTriggerBy;
    json["sl_trigger_by"] = m_slTriggerBy;
    return json;
}

void OrderResponse::fromJson(const nlohmann::json &json) {
    Response::fromJson(json);

    readMagicEnum<Side>(m_result, "side", m_side);
    readValue<std::string>(m_result, "symbol", m_symbol);
    readMagicEnum<OrderType>(m_result, "order_type", m_orderType);
    readValue<double>(m_result, "qty", m_qty);
    readMagicEnum<TimeInForce>(m_result, "time_in_force", m_timeInForce);
    readValue<bool>(m_result, "reduce_only", m_reduceOnly);
    readValue<bool>(m_result, "close_on_trigger", m_closeOnTrigger);
    readValue<std::int64_t>(m_result, "position_idx", m_positionIdx);
    readValue<double>(m_result, "price", m_price);
    readValue<std::string>(m_result, "order_id", m_orderId);
    readValue<std::int64_t>(m_result, "user_id", m_userId);
    readMagicEnum<OrderStatus>(m_result, "order_status", m_orderStatus);
    readValue<double>(m_result, "last_exec_price", m_lastExecPrice);
    readValue<double>(m_result, "cum_exec_qty", m_cumExecQty);
    readValue<double>(m_result, "cum_exec_value", m_cumExecValue);
    readValue<double>(m_result, "cum_exec_fee", m_cumExecFee);
    readValue<std::string>(m_result, "order_link_id", m_orderLinkId);
    readValue<std::string>(m_result, "created_time", m_createdTime);
    readValue<std::string>(m_result, "updated_time", m_updatedTime);
    readValue<double>(m_result, "take_profit", m_takeProfit);
    readValue<double>(m_result, "stop_loss", m_stopLoss);
    readMagicEnum<TriggerPriceType>(m_result, "tp_trigger_by", m_tpTriggerBy);
    readMagicEnum<TriggerPriceType>(m_result, "sl_trigger_by", m_slTriggerBy);
}

nlohmann::json OrdersResponse::toJson() const {
    throw std::runtime_error("Unimplemented: OrdersResponse::toJson()");
}

void OrdersResponse::fromJson(const nlohmann::json &json) {
    Response::fromJson(json);

    for (auto &el: m_result["data"]) {
        OrderResponse orderResponse;
        el["result"] = el;
        orderResponse.fromJson(el);
        m_orders.push_back(orderResponse);
    }
}

nlohmann::json Position::toJson() const {
    throw std::runtime_error("Unimplemented: Position::toJson()");
}

void Position::fromJson(const nlohmann::json &json) {

    readValue<std::int64_t>(json, "user_id", m_userId);
    readValue<std::string>(json, "symbol", m_symbol);
    readMagicEnum<Side>(json, "side", m_side);
    readValue<double>(json, "size", m_size);
    readValue<double>(json, "position_value", m_positionValue);
    readValue<double>(json, "entry_price", m_entryPrice);
    readValue<double>(json, "liq_price", m_liqPrice);
    readValue<double>(json, "bust_price", m_bustPrice);
    readValue<double>(json, "leverage", m_leverage);
    readValue<double>(json, "auto_add_margin", m_autoAddMargin);
    readValue<bool>(json, "is_isolated", m_isIsolated);
    readValue<double>(json, "position_margin", m_positionMargin);
    readValue<double>(json, "occ_closing_fee", m_occClosingFee);
    readValue<double>(json, "realised_pnl", m_realisedPnl);
    readValue<double>(json, "cum_realised_pnl", m_cumRealisedPnl);
    readValue<double>(json, "free_qty", m_freeQty);
    readMagicEnum<TpSlMode>(json, "tp_sl_mode", m_tpSlMode);
    readValue<double>(json, "unrealised_pnl", m_unrealisedPnl);
    readValue<double>(json, "deleverage_indicator", m_deleverageIndicator);
    readValue<int>(json, "risk_id", m_riskId);
    readValue<double>(json, "stop_loss", m_stopLoss);
    readValue<double>(json, "take_profit", m_takeProfit);
    readValue<double>(json, "trailing_stop", m_trailingStop);
    readValue<int>(json, "position_idx", m_positionIdx);
    readMagicEnum<PositionMode>(json, "mode", m_mode);
}

nlohmann::json Positions::toJson() const {
    throw std::runtime_error("Unimplemented: Positions::toJson()");
}

void Positions::fromJson(const nlohmann::json &json) {
    Response::fromJson(json);

    for (const auto &el: m_result.items()) {

        Position position;
        if (el.value().contains("data")) {
            position.fromJson(el.value()["data"]);
            m_positions.push_back(position);
        } else {
            position.fromJson(el.value());
            m_positions.push_back(position);
        }
    }
}

nlohmann::json PriceFilter::toJson() const {
    throw std::runtime_error("Unimplemented: PriceFilter::toJson()");
}

void PriceFilter::fromJson(const nlohmann::json &json) {

    m_minPrice = readStringAsDouble(json, "min_price", m_minPrice);
    m_maxPrice = readStringAsDouble(json, "max_price", m_maxPrice);
    m_tickSize = readStringAsDouble(json, "tick_size", m_tickSize);
}

nlohmann::json LeverageFilter::toJson() const {
    throw std::runtime_error("Unimplemented: LeverageFilter::toJson()");
}

void LeverageFilter::fromJson(const nlohmann::json &json) {

    readValue<double>(json, "min_leverage", m_minLeverage);
    readValue<double>(json, "max_leverage", m_maxLeverage);
    m_leverageStep = readStringAsDouble(json, "leverage_step", m_leverageStep);
}

nlohmann::json LotSizeFilter::toJson() const {
    throw std::runtime_error("Unimplemented: LotSizeFilter::toJson()");
}

void LotSizeFilter::fromJson(const nlohmann::json &json) {

    readValue<double>(json, "max_trading_qty", m_maxTradingQty);
    readValue<double>(json, "min_trading_qty", m_minTradingQty);
    readValue<double>(json, "qty_step", m_qtyStep);
    m_postOnlyMaxTradingQty = readStringAsDouble(json, "post_only_max_trading_qty", m_postOnlyMaxTradingQty);
}

nlohmann::json Symbol::toJson() const {
    throw std::runtime_error("Unimplemented: Symbol::toJson()");
}

void Symbol::fromJson(const nlohmann::json &json) {

    readValue<std::string>(json, "name", m_name);
    readValue<std::string>(json, "alias", m_alias);
    readMagicEnum<ContractStatus>(json, "status", m_status);
    readValue<std::string>(json, "base_currency", m_baseCurrency);
    readValue<std::string>(json, "quote_currency", m_quoteCurrency);
    readValue<int>(json, "price_scale", m_priceScale);
    m_takerFee = readStringAsDouble(json, "taker_fee", m_takerFee);
    m_makerFee = readStringAsDouble(json, "maker_fee", m_makerFee);
    readValue<int>(json, "funding_interval", m_fundingInterval);
    m_leverageFilter.fromJson(json["leverage_filter"]);
    m_priceFilter.fromJson(json["price_filter"]);
    m_lotSizeFilter.fromJson(json["lot_size_filter"]);
}

nlohmann::json Symbols::toJson() const {
    throw std::runtime_error("Unimplemented: Symbols::toJson()");
}

void Symbols::fromJson(const nlohmann::json &json) {
    Response::fromJson(json);

    for (const auto &el: m_result.items()) {
        Symbol symbol;
        symbol.fromJson(el.value());
        m_symbols.push_back(symbol);
    }
}

nlohmann::json FundingRate::toJson() const {
    throw std::runtime_error("Unimplemented: FundingRate::toJson()");
}

void FundingRate::fromJson(const nlohmann::json &json) {
    Response::fromJson(json);

    readValue<std::string>(m_result, "symbol", m_symbol);
    readValue<double>(m_result, "funding_rate", m_fundingRate);
    readValue<std::string>(m_result, "funding_rate_timestamp", m_fundingRateTimestamp);
}
}
