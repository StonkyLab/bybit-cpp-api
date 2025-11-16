/**
Bybit Data Models

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@gmail.com>.
*/

#include "vk/bybit/bybit_models.h"
#include "vk/utils/utils.h"
#include "vk/utils/json_utils.h"
#include <boost/multiprecision/cpp_dec_float.hpp>

namespace vk::bybit {
bool
readDecimalValue(const nlohmann::json& json, const std::string& key, boost::multiprecision::cpp_dec_float_50& value,
                 boost::multiprecision::cpp_dec_float_50 defaultVal = boost::multiprecision::cpp_dec_float_50("0")) {
    if (const auto it = json.find(key); it != json.end()) {
        if (!it.value().is_null() && it->is_string() && !it->get<std::string>().empty()) {
            value.assign(it->get<std::string>());
            return true;
        }
        value = std::move(defaultVal);
        return false;
    }

    return false;
}

nlohmann::json Response::toJson() const {
    throw std::runtime_error("Unimplemented: Response::toJson()");
}

void Response::fromJson(const nlohmann::json& json) {
    readValue<int>(json, "retCode", m_retCode);

    if (json.contains("retExtInfo")) {
        m_retExtInfo = json["retExtInfo"];
    }
    readValue<std::string>(json, "retMsg", m_retMsg);
    readValue<std::int64_t>(json, "time", m_time);

    if (json.contains("result")) {
        m_result = json["result"];
    }
}

nlohmann::json Candle::toJson() const {
    throw std::runtime_error("Unimplemented: Candle::toJson()");
}

void Candle::fromJson(const nlohmann::json& json) {
    m_startTime = stoll(json[0].get<std::string>());
    m_open = stod(json[1].get<std::string>());
    m_high = stod(json[2].get<std::string>());
    m_low = stod(json[3].get<std::string>());
    m_close = stod(json[4].get<std::string>());
    m_volume = stod(json[5].get<std::string>());
    m_turnover = stod(json[6].get<std::string>());
}

nlohmann::json Candles::toJson() const {
    throw std::runtime_error("Unimplemented: Candles::toJson()");
}

void Candles::fromJson(const nlohmann::json& json) {
    Response::fromJson(json);
    readValue<std::string>(m_result, "symbol", m_symbol);
    readMagicEnum<Category>(m_result, "category", m_category);

    for (const auto& el : m_result["list"].items()) {
        Candle candle;
        candle.fromJson(el.value());
        m_candles.push_back(candle);
    }
}

nlohmann::json Coin::toJson() const {
    throw std::runtime_error("Unimplemented: Coin::toJson()");
}

void Coin::fromJson(const nlohmann::json& json) {
    m_accruedInterest = readStringAsDouble(json, "accruedInterest", m_accruedInterest);
    m_availableToBorrow = readStringAsDouble(json, "availableToBorrow", m_availableToBorrow);
    m_availableToWithdraw = readStringAsDouble(json, "availableToWithdraw", m_availableToWithdraw);
    m_bonus = readStringAsDouble(json, "bonus", m_bonus);
    m_borrowAmount = readStringAsDouble(json, "borrowAmount", m_borrowAmount);
    readValue<std::string>(json, "coin", m_coin);
    readValue<bool>(json, "collateralSwitch", m_collateralSwitch);
    m_cumRealisedPnl = readStringAsDouble(json, "cumRealisedPnl", m_cumRealisedPnl);
    m_equity = readStringAsDouble(json, "equity", m_equity);
    m_locked = readStringAsDouble(json, "locked", m_locked);
    readValue<bool>(json, "marginCollateral", m_marginCollateral);
    m_totalOrderIM = readStringAsDouble(json, "totalOrderIM", m_totalOrderIM);
    m_totalPositionIM = readStringAsDouble(json, "totalPositionIM", m_totalPositionIM);
    m_totalPositionMM = readStringAsDouble(json, "totalPositionMM", m_totalPositionMM);
    m_unrealisedPnl = readStringAsDouble(json, "unrealisedPnl", m_unrealisedPnl);
    m_usdValue = readStringAsDouble(json, "usdValue", m_usdValue);
    m_walletBalance = readStringAsDouble(json, "walletBalance", m_walletBalance);
}

nlohmann::json AccountBalance::toJson() const {
    throw std::runtime_error("Unimplemented: AccountBalance::toJson()");
}

void AccountBalance::fromJson(const nlohmann::json& json) {
    m_accountIMRate = readStringAsDouble(json, "accountIMRate", m_accountIMRate);
    m_accountLTV = readStringAsDouble(json, "accountLTV", m_accountLTV);
    m_accountMMRate = readStringAsDouble(json, "accountMMRate", m_accountMMRate);
    readMagicEnum<AccountType>(json, "accountType", m_accountType);
    m_totalAvailableBalance = readStringAsDouble(json, "totalAvailableBalance", m_totalAvailableBalance);
    m_totalEquity = readStringAsDouble(json, "totalEquity", m_totalEquity);
    m_totalInitialMargin = readStringAsDouble(json, "totalInitialMargin", m_totalInitialMargin);
    m_totalMaintenanceMargin = readStringAsDouble(json, "totalMaintenanceMargin", m_totalMaintenanceMargin);
    m_totalMarginBalance = readStringAsDouble(json, "totalMarginBalance", m_totalMarginBalance);
    m_totalPerpUPL = readStringAsDouble(json, "totalPerpUPL", m_totalPerpUPL);
    m_totalWalletBalance = readStringAsDouble(json, "totalWalletBalance", m_totalWalletBalance);

    for (const auto& el : json["coin"].items()) {
        Coin coin;
        coin.fromJson(el.value());
        m_coins.push_back(coin);
    }
}

nlohmann::json WalletBalance::toJson() const {
    throw std::runtime_error("Unimplemented: WalletBalance::toJson()");
}

void WalletBalance::fromJson(const nlohmann::json& json) {
    Response::fromJson(json);

    for (const auto& el : m_result["list"].items()) {
        AccountBalance accountBalance;
        accountBalance.fromJson(el.value());
        m_balances.push_back(accountBalance);
    }
}


nlohmann::json ServerTime::toJson() const {
    throw std::runtime_error("Unimplemented: ServerTime::toJson()");
}

void ServerTime::fromJson(const nlohmann::json& json) {
    Response::fromJson(json);

    m_timeSecond = readStringAsInt64(m_result, "timeSecond");
    m_timeNano = readStringAsInt64(m_result, "timeNano");
}

nlohmann::json Position::toJson() const {
    throw std::runtime_error("Unimplemented: Position::toJson()");
}

void Position::fromJson(const nlohmann::json& json) {
    readValue<int>(json, "positionIdx", m_positionIdx);
    readValue<int>(json, "riskId", m_riskId);
    m_riskLimitValue = readStringAsDouble(json, "riskLimitValue", m_riskLimitValue);
    readValue<std::string>(json, "symbol", m_symbol);
    readMagicEnum<Side>(json, "side", m_side);

    /// We need to be absolutely sure whether the position has a non-zero size which cannot be assured with double type.
    boost::multiprecision::cpp_dec_float_50 positionSize{};
    readDecimalValue(json, "size", positionSize);

    if (positionSize) {
        m_zeroSize = false;
    }

    m_size = readStringAsDouble(json, "size", m_size);
    m_avgPrice = readStringAsDouble(json, "avgPrice", m_avgPrice);
    m_positionValue = readStringAsDouble(json, "positionValue", m_positionValue);
    readValue<int>(json, "tradeMode", m_tradeMode);
    readMagicEnum<PositionStatus>(json, "positionStatus", m_positionStatus);
    readValue<int>(json, "autoAddMargin", m_autoAddMargin);
    readValue<int>(json, "adlRankIndicator", m_adlRankIndicator);
    m_leverage = readStringAsDouble(json, "leverage", m_leverage);
    m_positionBalance = readStringAsDouble(json, "positionBalance", m_positionBalance);
    m_markPrice = readStringAsDouble(json, "markPrice", m_markPrice);
    m_liqPrice = readStringAsDouble(json, "liqPrice", m_liqPrice);
    m_bustPrice = readStringAsDouble(json, "bustPrice", m_bustPrice);
    m_positionMM = readStringAsDouble(json, "positionMM", m_positionMM);
    m_positionIM = readStringAsDouble(json, "positionIM", m_positionIM);
    readMagicEnum<TpSlMode>(json, "tpSlMode", m_tpSlMode);
    m_stopLoss = readStringAsDouble(json, "stopLoss", m_stopLoss);
    m_takeProfit = readStringAsDouble(json, "takeProfit", m_takeProfit);
    m_trailingStop = readStringAsDouble(json, "trailingStop", m_trailingStop);
    m_unrealisedPnl = readStringAsDouble(json, "unrealisedPnl", m_unrealisedPnl);
    m_cumRealisedPnl = readStringAsDouble(json, "cumRealisedPnl", m_cumRealisedPnl);
    readValue<bool>(json, "isReduceOnly", m_isReduceOnly);
    m_createdTime = readStringAsInt64(json, "createdTime");
    m_updatedTime = readStringAsInt64(json, "updatedTime");
    readValue<std::int64_t>(json, "seq", m_seq);
    readValue<std::string>(json, "mmrSysUpdateTime", m_mmrSysUpdateTime);
    readValue<std::string>(json, "leverageSysUpdatedTime", m_leverageSysUpdatedTime);
}

nlohmann::json Positions::toJson() const {
    throw std::runtime_error("Unimplemented: Positions::toJson()");
}

void Positions::fromJson(const nlohmann::json& json) {
    Response::fromJson(json);
    readMagicEnum<Category>(m_result, "category", m_category);

    for (const auto& el : m_result["list"].items()) {
        Position position;
        position.fromJson(el.value());
        m_positions.push_back(position);
    }
}

nlohmann::json PriceFilter::toJson() const {
    throw std::runtime_error("Unimplemented: PriceFilter::toJson()");
}

void PriceFilter::fromJson(const nlohmann::json& json) {
    m_minPrice = readStringAsDouble(json, "minPrice", m_minPrice);
    m_maxPrice = readStringAsDouble(json, "maxPrice", m_maxPrice);
    m_tickSize = readStringAsDouble(json, "tickSize", m_tickSize);
}

nlohmann::json LeverageFilter::toJson() const {
    throw std::runtime_error("Unimplemented: LeverageFilter::toJson()");
}

void LeverageFilter::fromJson(const nlohmann::json& json) {
    m_minLeverage = readStringAsDouble(json, "minLeverage", m_minLeverage);
    m_maxLeverage = readStringAsDouble(json, "maxLeverage", m_maxLeverage);
    m_leverageStep = readStringAsDouble(json, "leverageStep", m_leverageStep);
}

nlohmann::json LotSizeFilter::toJson() const {
    throw std::runtime_error("Unimplemented: LotSizeFilter::toJson()");
}

void LotSizeFilter::fromJson(const nlohmann::json& json) {
    m_maxOrderQty = readStringAsDouble(json, "maxOrderQty", m_maxOrderQty);
    m_minOrderQty = readStringAsDouble(json, "minOrderQty", m_minOrderQty);
    m_qtyStep = readStringAsDouble(json, "qtyStep", m_qtyStep);
    m_postOnlyMaxTradingQty = readStringAsDouble(json, "postOnlyMaxTradingQty", m_postOnlyMaxTradingQty);
}

nlohmann::json Instrument::toJson() const {
    throw std::runtime_error("Unimplemented: Instrument::toJson()");
}

void Instrument::fromJson(const nlohmann::json& json) {
    readValue<std::string>(json, "symbol", m_symbol);
    readMagicEnum<ContractType>(json, "contractType", m_contractType);
    readMagicEnum<ContractStatus>(json, "contractStatus", m_contractStatus);
    readValue<std::string>(json, "baseCoin", m_baseCoin);
    readValue<std::string>(json, "quoteCoin", m_quoteCoin);
    m_launchTime = readStringAsInt64(json, "launchTime");
    m_deliveryTime = readStringAsInt64(json, "deliveryTime");
    m_deliveryFeeRate = readStringAsDouble(json, "deliveryFeeRate", m_deliveryFeeRate);
    m_priceScale = readStringAsInt(json, "priceScale", m_priceScale);
    readValue<bool>(json, "unifiedMarginTrade", m_unifiedMarginTrade);
    readValue<int>(json, "fundingInterval", m_fundingInterval);
    readValue<std::string>(json, "settleCoin", m_settleCoin);
    m_leverageFilter.fromJson(json["leverageFilter"]);
    m_priceFilter.fromJson(json["priceFilter"]);
    m_lotSizeFilter.fromJson(json["lotSizeFilter"]);
}

nlohmann::json Instruments::toJson() const {
    throw std::runtime_error("Unimplemented: Instruments::toJson()");
}

void Instruments::fromJson(const nlohmann::json& json) {
    Response::fromJson(json);

    readMagicEnum<Category>(m_result, "category", m_category);
    readValue<std::string>(m_result, "nextPageCursor", m_nextPageCursor);

    for (const auto& el : m_result["list"].items()) {
        Instrument instrument;
        instrument.fromJson(el.value());
        m_instruments.push_back(instrument);
    }
}

nlohmann::json Order::toJson() const {
    nlohmann::json json;

    json["category"] = m_category;
    json["side"] = m_side;
    json["symbol"] = m_symbol;
    json["orderType"] = m_orderType;
    json["timeInForce"] = m_timeInForce;
    json["reduceOnly"] = m_reduceOnly;
    json["closeOnTrigger"] = m_closeOnTrigger;
    json["positionIdx"] = m_positionIdx;

    if (!m_orderLinkId.empty()) {
        json["orderLinkId"] = m_orderLinkId;
    }

    if (m_takeProfit != 0.0) {
        json["takeProfit"] = m_takeProfit;
    }

    if (m_stopLoss != 0.0) {
        json["stopLoss"] = m_stopLoss;
    }

    if (m_orderType == OrderType::Limit) {
        json["price"] = std::to_string(m_price);
    }

    if (m_tpTriggerBy == TriggerPriceType::LastPrice) {
        json["tpTriggerBy"] = m_tpTriggerBy;
    }

    if (m_slTriggerBy == TriggerPriceType::LastPrice) {
        json["slTriggerBy"] = m_slTriggerBy;
    }

    /// Fix number of decimal in qty attribute
    {
        const boost::multiprecision::cpp_dec_float_50 precision_dec(std::to_string(m_qtyStep));
        const auto parts = splitString(precision_dec.str(), '.');
        int precision = 0;

        if (parts.size() == 2) {
            precision = parts[1].length();
        }

        json["qty"] = formatDouble(precision, m_qty);
    }

    /// Fix number of decimal in price attribute
    {
        const boost::multiprecision::cpp_dec_float_50 precision_dec(std::to_string(m_priceStep));
        const auto parts = splitString(precision_dec.str(), '.');
        int precision = 0;

        if (parts.size() == 2) {
            precision = parts[1].length();
        }
        if (m_orderType == OrderType::Limit) {
            json["price"] = formatDouble(precision, m_price);
        }
    }
    return json;
}

void Order::fromJson(const nlohmann::json& json) {
    throw std::runtime_error("Unimplemented: Order::fromJson()");
}

nlohmann::json OrderId::toJson() const {
    throw std::runtime_error("Unimplemented: OrderId::toJson()");
}

void OrderId::fromJson(const nlohmann::json& json) {
    Response::fromJson(json);

    readValue<std::string>(m_result, "orderId", m_orderId);
    readValue<std::string>(m_result, "orderLinkId", m_orderLinkId);
}

nlohmann::json OrderResponse::toJson() const {
    nlohmann::json json;

    json["orderId"] = m_orderId;
    json["orderLinkId"] = m_orderLinkId;
    json["symbol"] = m_symbol;
    json["side"] = m_side;
    json["price"] = std::to_string(m_price);
    json["qty"] = std::to_string(m_qty);
    json["positionIdx"] = m_positionIdx;
    json["orderStatus"] = m_orderStatus;
    json["rejectReason"] = m_rejectReason;
    json["cumExecQty"] = std::to_string(m_cumExecQty);
    json["cumExecValue"] = std::to_string(m_cumExecValue);
    json["cumExecFee"] = std::to_string(m_cumExecFee);
    json["avgPrice"] = std::to_string(m_avgPrice);
    json["timeInForce"] = m_timeInForce;
    json["orderType"] = m_orderType;
    json["reduceOnly"] = m_reduceOnly;
    json["closeOnTrigger"] = m_closeOnTrigger;
    json["lastPriceOnCreated"] = std::to_string(m_lastPriceOnCreated);
    json["createdTime"] = m_createdTime;
    json["updatedTime"] = m_updatedTime;
    json["takeProfit"] = std::to_string(m_takeProfit);
    json["stopLoss"] = std::to_string(m_stopLoss);
    json["tpTriggerBy"] = m_tpTriggerBy;
    json["slTriggerBy"] = m_slTriggerBy;
    return json;
}

void OrderResponse::fromJson(const nlohmann::json& json) {
    readValue<std::string>(json, "orderId", m_orderId);
    readValue<std::string>(json, "orderLinkId", m_orderLinkId);
    readValue<std::string>(json, "symbol", m_symbol);
    readMagicEnum<Side>(json, "side", m_side);
    m_price = readStringAsDouble(json, "price", m_price);
    m_qty = readStringAsDouble(json, "qty", m_qty);
    readValue<std::int64_t>(json, "positionIdx", m_positionIdx);
    readMagicEnum<OrderStatus>(json, "orderStatus", m_orderStatus);
    readValue<std::string>(json, "rejectReason", m_rejectReason);
    m_avgPrice = readStringAsDouble(json, "avgPrice", m_avgPrice);
    m_cumExecQty = readStringAsDouble(json, "cumExecQty", m_cumExecQty);
    m_cumExecValue = readStringAsDouble(json, "cumExecValue", m_cumExecValue);
    m_cumExecFee = readStringAsDouble(json, "cumExecFee", m_cumExecFee);
    readMagicEnum<TimeInForce>(json, "timeInForce", m_timeInForce);
    readMagicEnum<OrderType>(json, "orderType", m_orderType);
    readValue<bool>(json, "reduceOnly", m_reduceOnly);
    readValue<bool>(json, "closeOnTrigger", m_closeOnTrigger);
    m_lastPriceOnCreated = readStringAsDouble(json, "lastPriceOnCreated", m_lastPriceOnCreated);
    readValue<std::string>(json, "createdTime", m_createdTime);
    readValue<std::string>(json, "updatedTime", m_updatedTime);
    m_takeProfit = readStringAsDouble(json, "takeProfit", m_takeProfit);
    m_stopLoss = readStringAsDouble(json, "stopLoss", m_stopLoss);
    readMagicEnum<TriggerPriceType>(json, "tpTriggerBy", m_tpTriggerBy);
    readMagicEnum<TriggerPriceType>(json, "slTriggerBy", m_slTriggerBy);
}

nlohmann::json OrdersResponse::toJson() const {
    throw std::runtime_error("Unimplemented: OrdersResponse::toJson()");
}

void OrdersResponse::fromJson(const nlohmann::json& json) {
    Response::fromJson(json);

    readMagicEnum<Category>(m_result, "category", m_category);

    for (const auto& el : m_result["list"].items()) {
        OrderResponse orderResponse;
        orderResponse.fromJson(el.value());
        m_orders.push_back(orderResponse);
    }
}

nlohmann::json FundingRate::toJson() const {
    throw std::runtime_error("Unimplemented: FundingRate::toJson()");
}

void FundingRate::fromJson(const nlohmann::json& json) {
    readValue<std::string>(json, "symbol", m_symbol);
    m_fundingRate = readStringAsDouble(json, "fundingRate", m_fundingRate);
    m_fundingRateTimestamp = readStringAsInt64(json, "fundingRateTimestamp", m_fundingRateTimestamp);
}

nlohmann::json FundingRates::toJson() const {
    throw std::runtime_error("Unimplemented: FundingRates::toJson()");
}

void FundingRates::fromJson(const nlohmann::json& json) {
    Response::fromJson(json);

    readMagicEnum<Category>(m_result, "category", m_category);

    for (const auto& el : m_result["list"].items()) {
        FundingRate fundingRate;
        fundingRate.fromJson(el.value());
        m_fundingRates.push_back(fundingRate);
    }
}

nlohmann::json Ticker::toJson() const {
    throw std::runtime_error("Unimplemented: Ticker::toJson()");
}

void Ticker::fromJson(const nlohmann::json& json) {

    readValue<std::string>(json, "symbol", m_symbol);
    m_lastPrice = readStringAsDouble(json, "lastPrice", m_lastPrice);
    m_indexPrice = readStringAsDouble(json, "indexPrice", m_indexPrice);
    m_markPrice = readStringAsDouble(json, "markPrice", m_markPrice);
    m_prevPrice24h = readStringAsDouble(json, "prevPrice24h", m_prevPrice24h);
    m_price24hPcnt = readStringAsDouble(json, "price24hPcnt", m_price24hPcnt);
    m_highPrice24h = readStringAsDouble(json, "highPrice24h", m_highPrice24h);
    m_prevPrice1h = readStringAsDouble(json, "prevPrice1h", m_prevPrice1h);
    m_openInterest = readStringAsInt64(json, "openInterest", m_openInterest);
    m_openInterestValue = readStringAsDouble(json, "openInterestValue", m_openInterestValue);
    m_turnover24h = readStringAsDouble(json, "turnover24h", m_turnover24h);
    m_volume24h = readStringAsDouble(json, "volume24h", m_volume24h);
    m_fundingRate = readStringAsDouble(json, "fundingRate", m_fundingRate);
    m_nextFundingTime = readStringAsInt64(json, "nextFundingTime", m_nextFundingTime);
    m_ask1Size = readStringAsDouble(json, "ask1Size", m_ask1Size);
    m_bid1Price = readStringAsDouble(json, "bid1Price", m_bid1Price);
    m_ask1Price = readStringAsDouble(json, "ask1Price", m_ask1Price);
    m_bid1Size = readStringAsDouble(json, "bid1Size", m_bid1Size);
}

nlohmann::json Tickers::toJson() const {
    throw std::runtime_error("Unimplemented: Tickers::toJson()");
}

void Tickers::fromJson(const nlohmann::json& json) {
    Response::fromJson(json);

    readMagicEnum<Category>(m_result, "category", m_category);

    for (const auto& el : m_result["list"].items()) {
        Ticker ticker;
        ticker.fromJson(el.value());
        m_tickers.push_back(ticker);
    }
}
}
