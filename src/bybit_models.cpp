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
    readValue<int>(json, "retCode", retCode);

    if (json.contains("retExtInfo")) {
        retExtInfo = json["retExtInfo"];
    }
    readValue<std::string>(json, "retMsg", retMsg);
    readValue<std::int64_t>(json, "time", time);

    if (json.contains("result")) {
        result = json["result"];
    }
}

nlohmann::json Candle::toJson() const {
    throw std::runtime_error("Unimplemented: Candle::toJson()");
}

void Candle::fromJson(const nlohmann::json& json) {
    startTime = stoll(json[0].get<std::string>());
    open = stod(json[1].get<std::string>());
    high = stod(json[2].get<std::string>());
    low = stod(json[3].get<std::string>());
    close = stod(json[4].get<std::string>());
    volume = stod(json[5].get<std::string>());
    turnover = stod(json[6].get<std::string>());
}

nlohmann::json Candles::toJson() const {
    throw std::runtime_error("Unimplemented: Candles::toJson()");
}

void Candles::fromJson(const nlohmann::json& json) {
    Response::fromJson(json);
    readValue<std::string>(result, "symbol", symbol);
    readMagicEnum<Category>(result, "category", category);

    for (const auto& el : result["list"].items()) {
        Candle candle;
        candle.fromJson(el.value());
        candles.push_back(candle);
    }
}

nlohmann::json Coin::toJson() const {
    throw std::runtime_error("Unimplemented: Coin::toJson()");
}

void Coin::fromJson(const nlohmann::json& json) {
    accruedInterest = readStringAsDouble(json, "accruedInterest", accruedInterest);
    availableToBorrow = readStringAsDouble(json, "availableToBorrow", availableToBorrow);
    availableToWithdraw = readStringAsDouble(json, "availableToWithdraw", availableToWithdraw);
    bonus = readStringAsDouble(json, "bonus", bonus);
    borrowAmount = readStringAsDouble(json, "borrowAmount", borrowAmount);
    readValue<std::string>(json, "coin", coin);
    readValue<bool>(json, "collateralSwitch", collateralSwitch);
    cumRealisedPnl = readStringAsDouble(json, "cumRealisedPnl", cumRealisedPnl);
    equity = readStringAsDouble(json, "equity", equity);
    locked = readStringAsDouble(json, "locked", locked);
    readValue<bool>(json, "marginCollateral", marginCollateral);
    totalOrderIM = readStringAsDouble(json, "totalOrderIM", totalOrderIM);
    totalPositionIM = readStringAsDouble(json, "totalPositionIM", totalPositionIM);
    totalPositionMM = readStringAsDouble(json, "totalPositionMM", totalPositionMM);
    unrealisedPnl = readStringAsDouble(json, "unrealisedPnl", unrealisedPnl);
    usdValue = readStringAsDouble(json, "usdValue", usdValue);
    walletBalance = readStringAsDouble(json, "walletBalance", walletBalance);
}

nlohmann::json AccountBalance::toJson() const {
    throw std::runtime_error("Unimplemented: AccountBalance::toJson()");
}

void AccountBalance::fromJson(const nlohmann::json& json) {
    accountIMRate = readStringAsDouble(json, "accountIMRate", accountIMRate);
    accountLTV = readStringAsDouble(json, "accountLTV", accountLTV);
    accountMMRate = readStringAsDouble(json, "accountMMRate", accountMMRate);
    readMagicEnum<AccountType>(json, "accountType", accountType);
    totalAvailableBalance = readStringAsDouble(json, "totalAvailableBalance", totalAvailableBalance);
    totalEquity = readStringAsDouble(json, "totalEquity", totalEquity);
    totalInitialMargin = readStringAsDouble(json, "totalInitialMargin", totalInitialMargin);
    totalMaintenanceMargin = readStringAsDouble(json, "totalMaintenanceMargin", totalMaintenanceMargin);
    totalMarginBalance = readStringAsDouble(json, "totalMarginBalance", totalMarginBalance);
    totalPerpUPL = readStringAsDouble(json, "totalPerpUPL", totalPerpUPL);
    totalWalletBalance = readStringAsDouble(json, "totalWalletBalance", totalWalletBalance);

    for (const auto& el : json["coin"].items()) {
        Coin coin;
        coin.fromJson(el.value());
        coins.push_back(coin);
    }
}

nlohmann::json WalletBalance::toJson() const {
    throw std::runtime_error("Unimplemented: WalletBalance::toJson()");
}

void WalletBalance::fromJson(const nlohmann::json& json) {
    Response::fromJson(json);

    for (const auto& el : result["list"].items()) {
        AccountBalance accountBalance;
        accountBalance.fromJson(el.value());
        balances.push_back(accountBalance);
    }
}


nlohmann::json ServerTime::toJson() const {
    throw std::runtime_error("Unimplemented: ServerTime::toJson()");
}

void ServerTime::fromJson(const nlohmann::json& json) {
    Response::fromJson(json);

    timeSecond = readStringAsInt64(result, "timeSecond");
    timeNano = readStringAsInt64(result, "timeNano");
}

nlohmann::json Position::toJson() const {
    throw std::runtime_error("Unimplemented: Position::toJson()");
}

void Position::fromJson(const nlohmann::json& json) {
    readValue<int>(json, "positionIdx", positionIdx);
    readValue<int>(json, "riskId", riskId);
    riskLimitValue = readStringAsDouble(json, "riskLimitValue", riskLimitValue);
    readValue<std::string>(json, "symbol", symbol);
    readMagicEnum<Side>(json, "side", side);

    /// We need to be absolutely sure whether the position has a non-zero size which cannot be assured with double type.
    boost::multiprecision::cpp_dec_float_50 positionSize{};
    readDecimalValue(json, "size", positionSize);

    if (positionSize) {
        zeroSize = false;
    }

    size = readStringAsDouble(json, "size", size);
    avgPrice = readStringAsDouble(json, "avgPrice", avgPrice);
    positionValue = readStringAsDouble(json, "positionValue", positionValue);
    readValue<int>(json, "tradeMode", tradeMode);
    readMagicEnum<PositionStatus>(json, "positionStatus", positionStatus);
    readValue<int>(json, "autoAddMargin", autoAddMargin);
    readValue<int>(json, "adlRankIndicator", adlRankIndicator);
    leverage = readStringAsDouble(json, "leverage", leverage);
    positionBalance = readStringAsDouble(json, "positionBalance", positionBalance);
    markPrice = readStringAsDouble(json, "markPrice", markPrice);
    liqPrice = readStringAsDouble(json, "liqPrice", liqPrice);
    bustPrice = readStringAsDouble(json, "bustPrice", bustPrice);
    positionMM = readStringAsDouble(json, "positionMM", positionMM);
    positionIM = readStringAsDouble(json, "positionIM", positionIM);
    readMagicEnum<TpSlMode>(json, "tpSlMode", tpSlMode);
    stopLoss = readStringAsDouble(json, "stopLoss", stopLoss);
    takeProfit = readStringAsDouble(json, "takeProfit", takeProfit);
    trailingStop = readStringAsDouble(json, "trailingStop", trailingStop);
    unrealisedPnl = readStringAsDouble(json, "unrealisedPnl", unrealisedPnl);
    cumRealisedPnl = readStringAsDouble(json, "cumRealisedPnl", cumRealisedPnl);
    readValue<bool>(json, "isReduceOnly", isReduceOnly);
    createdTime = readStringAsInt64(json, "createdTime");
    updatedTime = readStringAsInt64(json, "updatedTime");
    readValue<std::int64_t>(json, "seq", seq);
    readValue<std::string>(json, "mmrSysUpdateTime", mmrSysUpdateTime);
    readValue<std::string>(json, "leverageSysUpdatedTime", leverageSysUpdatedTime);
}

nlohmann::json Positions::toJson() const {
    throw std::runtime_error("Unimplemented: Positions::toJson()");
}

void Positions::fromJson(const nlohmann::json& json) {
    Response::fromJson(json);
    readMagicEnum<Category>(result, "category", category);

    for (const auto& el : result["list"].items()) {
        Position position;
        position.fromJson(el.value());
        positions.push_back(position);
    }
}

nlohmann::json PriceFilter::toJson() const {
    throw std::runtime_error("Unimplemented: PriceFilter::toJson()");
}

void PriceFilter::fromJson(const nlohmann::json& json) {
    minPrice = readStringAsDouble(json, "minPrice", minPrice);
    maxPrice = readStringAsDouble(json, "maxPrice", maxPrice);
    tickSize = readStringAsDouble(json, "tickSize", tickSize);
}

nlohmann::json LeverageFilter::toJson() const {
    throw std::runtime_error("Unimplemented: LeverageFilter::toJson()");
}

void LeverageFilter::fromJson(const nlohmann::json& json) {
    minLeverage = readStringAsDouble(json, "minLeverage", minLeverage);
    maxLeverage = readStringAsDouble(json, "maxLeverage", maxLeverage);
    leverageStep = readStringAsDouble(json, "leverageStep", leverageStep);
}

nlohmann::json LotSizeFilter::toJson() const {
    throw std::runtime_error("Unimplemented: LotSizeFilter::toJson()");
}

void LotSizeFilter::fromJson(const nlohmann::json& json) {
    maxOrderQty = readStringAsDouble(json, "maxOrderQty", maxOrderQty);
    minOrderQty = readStringAsDouble(json, "minOrderQty", minOrderQty);
    qtyStep = readStringAsDouble(json, "qtyStep", qtyStep);
    postOnlyMaxTradingQty = readStringAsDouble(json, "postOnlyMaxTradingQty", postOnlyMaxTradingQty);
}

nlohmann::json Instrument::toJson() const {
    throw std::runtime_error("Unimplemented: Instrument::toJson()");
}

void Instrument::fromJson(const nlohmann::json& json) {
    readValue<std::string>(json, "symbol", symbol);
    readMagicEnum<ContractType>(json, "contractType", contractType);
    readMagicEnum<ContractStatus>(json, "contractStatus", contractStatus);
    readValue<std::string>(json, "baseCoin", baseCoin);
    readValue<std::string>(json, "quoteCoin", quoteCoin);
    launchTime = readStringAsInt64(json, "launchTime");
    deliveryTime = readStringAsInt64(json, "deliveryTime");
    deliveryFeeRate = readStringAsDouble(json, "deliveryFeeRate", deliveryFeeRate);
    priceScale = readStringAsInt(json, "priceScale", priceScale);
    readValue<bool>(json, "unifiedMarginTrade", unifiedMarginTrade);
    readValue<int>(json, "fundingInterval", fundingInterval);
    readValue<std::string>(json, "settleCoin", settleCoin);

    if (json.contains("leverageFilter")) {
      leverageFilter.fromJson(json["leverageFilter"]);
    }

    priceFilter.fromJson(json["priceFilter"]);
    lotSizeFilter.fromJson(json["lotSizeFilter"]);
}

nlohmann::json Instruments::toJson() const {
    throw std::runtime_error("Unimplemented: Instruments::toJson()");
}

void Instruments::fromJson(const nlohmann::json& json) {
    Response::fromJson(json);

    readMagicEnum<Category>(result, "category", category);
    readValue<std::string>(result, "nextPageCursor", nextPageCursor);

    for (const auto& el : result["list"].items()) {
        Instrument instrument;
        instrument.fromJson(el.value());
        instruments.push_back(instrument);
    }
}

nlohmann::json Order::toJson() const {
    nlohmann::json json;

    json["category"] = category;
    json["side"] = side;
    json["symbol"] = symbol;
    json["orderType"] = orderType;
    json["timeInForce"] = timeInForce;
    json["reduceOnly"] = reduceOnly;
    json["closeOnTrigger"] = closeOnTrigger;
    json["positionIdx"] = positionIdx;

    if (!orderLinkId.empty()) {
        json["orderLinkId"] = orderLinkId;
    }

    if (takeProfit != 0.0) {
        json["takeProfit"] = takeProfit;
    }

    if (stopLoss != 0.0) {
        json["stopLoss"] = stopLoss;
    }

    if (orderType == OrderType::Limit) {
        json["price"] = std::to_string(price);
    }

    if (tpTriggerBy == TriggerPriceType::LastPrice) {
        json["tpTriggerBy"] = tpTriggerBy;
    }

    if (slTriggerBy == TriggerPriceType::LastPrice) {
        json["slTriggerBy"] = slTriggerBy;
    }

    /// Fix number of decimal in qty attribute
    {
        const boost::multiprecision::cpp_dec_float_50 precision_dec(std::to_string(qtyStep));
        const auto parts = splitString(precision_dec.str(), '.');
        int precision = 0;

        if (parts.size() == 2) {
            precision = parts[1].length();
        }

        json["qty"] = formatDouble(precision, qty);
    }

    /// Fix number of decimal in price attribute
    {
        const boost::multiprecision::cpp_dec_float_50 precision_dec(std::to_string(priceStep));
        const auto parts = splitString(precision_dec.str(), '.');
        int precision = 0;

        if (parts.size() == 2) {
            precision = parts[1].length();
        }
        if (orderType == OrderType::Limit) {
            json["price"] = formatDouble(precision, price);
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

    readValue<std::string>(result, "orderId", orderId);
    readValue<std::string>(result, "orderLinkId", orderLinkId);
}

nlohmann::json OrderResponse::toJson() const {
    nlohmann::json json;

    json["orderId"] = orderId;
    json["orderLinkId"] = orderLinkId;
    json["symbol"] = symbol;
    json["side"] = side;
    json["price"] = std::to_string(price);
    json["qty"] = std::to_string(qty);
    json["positionIdx"] = positionIdx;
    json["orderStatus"] = orderStatus;
    json["rejectReason"] = rejectReason;
    json["cumExecQty"] = std::to_string(cumExecQty);
    json["cumExecValue"] = std::to_string(cumExecValue);
    json["cumExecFee"] = std::to_string(cumExecFee);
    json["avgPrice"] = std::to_string(avgPrice);
    json["timeInForce"] = timeInForce;
    json["orderType"] = orderType;
    json["reduceOnly"] = reduceOnly;
    json["closeOnTrigger"] = closeOnTrigger;
    json["lastPriceOnCreated"] = std::to_string(lastPriceOnCreated);
    json["createdTime"] = createdTime;
    json["updatedTime"] = updatedTime;
    json["takeProfit"] = std::to_string(takeProfit);
    json["stopLoss"] = std::to_string(stopLoss);
    json["tpTriggerBy"] = tpTriggerBy;
    json["slTriggerBy"] = slTriggerBy;
    return json;
}

void OrderResponse::fromJson(const nlohmann::json& json) {
    readValue<std::string>(json, "orderId", orderId);
    readValue<std::string>(json, "orderLinkId", orderLinkId);
    readValue<std::string>(json, "symbol", symbol);
    readMagicEnum<Side>(json, "side", side);
    price = readStringAsDouble(json, "price", price);
    qty = readStringAsDouble(json, "qty", qty);
    readValue<std::int64_t>(json, "positionIdx", positionIdx);
    readMagicEnum<OrderStatus>(json, "orderStatus", orderStatus);
    readValue<std::string>(json, "rejectReason", rejectReason);
    avgPrice = readStringAsDouble(json, "avgPrice", avgPrice);
    cumExecQty = readStringAsDouble(json, "cumExecQty", cumExecQty);
    cumExecValue = readStringAsDouble(json, "cumExecValue", cumExecValue);
    cumExecFee = readStringAsDouble(json, "cumExecFee", cumExecFee);
    readMagicEnum<TimeInForce>(json, "timeInForce", timeInForce);
    readMagicEnum<OrderType>(json, "orderType", orderType);
    readValue<bool>(json, "reduceOnly", reduceOnly);
    readValue<bool>(json, "closeOnTrigger", closeOnTrigger);
    lastPriceOnCreated = readStringAsDouble(json, "lastPriceOnCreated", lastPriceOnCreated);
    readValue<std::string>(json, "createdTime", createdTime);
    readValue<std::string>(json, "updatedTime", updatedTime);
    takeProfit = readStringAsDouble(json, "takeProfit", takeProfit);
    stopLoss = readStringAsDouble(json, "stopLoss", stopLoss);
    readMagicEnum<TriggerPriceType>(json, "tpTriggerBy", tpTriggerBy);
    readMagicEnum<TriggerPriceType>(json, "slTriggerBy", slTriggerBy);
}

nlohmann::json OrdersResponse::toJson() const {
    throw std::runtime_error("Unimplemented: OrdersResponse::toJson()");
}

void OrdersResponse::fromJson(const nlohmann::json& json) {
    Response::fromJson(json);

    readMagicEnum<Category>(result, "category", category);

    for (const auto& el : result["list"].items()) {
        OrderResponse orderResponse;
        orderResponse.fromJson(el.value());
        orders.push_back(orderResponse);
    }
}

nlohmann::json FundingRate::toJson() const {
    throw std::runtime_error("Unimplemented: FundingRate::toJson()");
}

void FundingRate::fromJson(const nlohmann::json& json) {
    readValue<std::string>(json, "symbol", symbol);
    fundingRate = readStringAsDouble(json, "fundingRate", fundingRate);
    fundingRateTimestamp = readStringAsInt64(json, "fundingRateTimestamp", fundingRateTimestamp);
}

nlohmann::json FundingRates::toJson() const {
    throw std::runtime_error("Unimplemented: FundingRates::toJson()");
}

void FundingRates::fromJson(const nlohmann::json& json) {
    Response::fromJson(json);

    readMagicEnum<Category>(result, "category", category);

    for (const auto& el : result["list"].items()) {
        FundingRate rate;
        rate.fromJson(el.value());
        fundingRates.push_back(rate);
    }
}

nlohmann::json Ticker::toJson() const {
    throw std::runtime_error("Unimplemented: Ticker::toJson()");
}

void Ticker::fromJson(const nlohmann::json& json) {
    readValue<std::string>(json, "symbol", symbol);
    lastPrice = readStringAsDouble(json, "lastPrice", lastPrice);
    indexPrice = readStringAsDouble(json, "indexPrice", indexPrice);
    markPrice = readStringAsDouble(json, "markPrice", markPrice);
    prevPrice24h = readStringAsDouble(json, "prevPrice24h", prevPrice24h);
    price24hPcnt = readStringAsDouble(json, "price24hPcnt", price24hPcnt);
    highPrice24h = readStringAsDouble(json, "highPrice24h", highPrice24h);
    prevPrice1h = readStringAsDouble(json, "prevPrice1h", prevPrice1h);
    openInterest = readStringAsInt64(json, "openInterest", openInterest);
    openInterestValue = readStringAsDouble(json, "openInterestValue", openInterestValue);
    turnover24h = readStringAsDouble(json, "turnover24h", turnover24h);
    volume24h = readStringAsDouble(json, "volume24h", volume24h);
    fundingRate = readStringAsDouble(json, "fundingRate", fundingRate);
    nextFundingTime = readStringAsInt64(json, "nextFundingTime", nextFundingTime);
    ask1Size = readStringAsDouble(json, "ask1Size", ask1Size);
    bid1Price = readStringAsDouble(json, "bid1Price", bid1Price);
    ask1Price = readStringAsDouble(json, "ask1Price", ask1Price);
    bid1Size = readStringAsDouble(json, "bid1Size", bid1Size);
}

nlohmann::json Tickers::toJson() const {
    throw std::runtime_error("Unimplemented: Tickers::toJson()");
}

void Tickers::fromJson(const nlohmann::json& json) {
    Response::fromJson(json);

    readMagicEnum<Category>(result, "category", category);

    for (const auto& el : result["list"].items()) {
        Ticker ticker;
        ticker.fromJson(el.value());
        tickers.push_back(ticker);
    }
}
}
