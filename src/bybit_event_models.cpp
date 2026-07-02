/**
Bybit Event Data Models

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.
*/

#include "stonky/bybit/bybit_event_models.h"
#include "stonky/utils/utils.h"
#include "stonky/utils/json_utils.h"

namespace stonky::bybit {
nlohmann::json Event::toJson() const {
    throw std::runtime_error("Unimplemented: Event::toJson()");
}

void Event::fromJson(const nlohmann::json& json) {
    readValue<std::string>(json, "topic", topic);
    readMagicEnum<ResponseType>(json, "type", type);

    /// Private-stream messages carry "creationTime" instead of "ts" and have
    /// no "type" field; both readers are lenient, so one Event covers both.
    if (!readValue<std::int64_t>(json, "ts", ts)) {
        readValue<std::int64_t>(json, "creationTime", ts);
    }

    if (const auto it = json.find("data"); it != json.end()) {
        data = *it;
    }
}

nlohmann::json EventTicker::toJson() const {
    throw std::runtime_error("Unimplemented: EventTicker::toJson()");
}

void EventTicker::fromJson(const nlohmann::json& json) {
    readValue<std::string>(json, "symbol", symbol);
    ask1Price = readStringAsDouble(json, "ask1Price", ask1Price);
    ask1Size = readStringAsDouble(json, "ask1Size", ask1Size);
    bid1Price = readStringAsDouble(json, "bid1Price", bid1Price);
    bid1Size = readStringAsDouble(json, "bid1Size", bid1Size);
    lastPrice = readStringAsDouble(json, "lastPrice", lastPrice);
}

void EventTicker::loadEventData(const Event& event) {
    switch (event.type) {
    case ResponseType::snapshot:
        fromJson(event.data);
        break;
    case ResponseType::delta:
        fromJson(event.data);
        break;
    }
}

nlohmann::json EventCandlestick::toJson() const {
    throw std::runtime_error("Unimplemented: EventCandlestick::toJson()");
}

void EventCandlestick::fromJson(const nlohmann::json& json) {
    readValue<std::int64_t>(json, "start", start);
    readValue<std::int64_t>(json, "end", end);
    readValue<std::string>(json, "interval", interval);
    open = readStringAsDouble(json, "open", open);
    high = readStringAsDouble(json, "high", high);
    low = readStringAsDouble(json, "low", low);
    close = readStringAsDouble(json, "close", close);
    volume = readStringAsDouble(json, "volume", volume);
    turnover = readStringAsDouble(json, "turnover", turnover);
    readValue<bool>(json, "confirm", confirm);
    readValue<std::int64_t>(json, "timestamp", timestamp);
}

nlohmann::json EventOrderUpdate::toJson() const {
    throw std::runtime_error("Unimplemented: EventOrderUpdate::toJson()");
}

void EventOrderUpdate::fromJson(const nlohmann::json& json) {
    readValue<std::string>(json, "category", category);
    readValue<std::string>(json, "symbol", symbol);
    readValue<std::string>(json, "orderId", orderId);
    readValue<std::string>(json, "orderLinkId", orderLinkId);
    readMagicEnum<Side>(json, "side", side);
    readMagicEnum<OrderType>(json, "orderType", orderType);
    readMagicEnum<OrderStatus>(json, "orderStatus", orderStatus);
    readValue<std::string>(json, "rejectReason", rejectReason);
    readValue<std::string>(json, "cancelType", cancelType);
    readMagicEnum<TimeInForce>(json, "timeInForce", timeInForce);
    price = readStringAsDouble(json, "price", price);
    qty = readStringAsDouble(json, "qty", qty);
    avgPrice = readStringAsDouble(json, "avgPrice", avgPrice);
    leavesQty = readStringAsDouble(json, "leavesQty", leavesQty);
    cumExecQty = readStringAsDouble(json, "cumExecQty", cumExecQty);
    cumExecValue = readStringAsDouble(json, "cumExecValue", cumExecValue);
    readValue<bool>(json, "reduceOnly", reduceOnly);
    createdTime = readStringAsInt64(json, "createdTime", createdTime);
    updatedTime = readStringAsInt64(json, "updatedTime", updatedTime);
}

nlohmann::json EventExecution::toJson() const {
    throw std::runtime_error("Unimplemented: EventExecution::toJson()");
}

void EventExecution::fromJson(const nlohmann::json& json) {
    readValue<std::string>(json, "category", category);
    readValue<std::string>(json, "symbol", symbol);
    readValue<std::string>(json, "orderId", orderId);
    readValue<std::string>(json, "orderLinkId", orderLinkId);
    readValue<std::string>(json, "execId", execId);
    readMagicEnum<Side>(json, "side", side);
    readMagicEnum<ExecType>(json, "execType", execType);
    execPrice = readStringAsDouble(json, "execPrice", execPrice);
    execQty = readStringAsDouble(json, "execQty", execQty);
    execValue = readStringAsDouble(json, "execValue", execValue);
    execFee = readStringAsDouble(json, "execFee", execFee);
    feeRate = readStringAsDouble(json, "feeRate", feeRate);
    leavesQty = readStringAsDouble(json, "leavesQty", leavesQty);
    closedSize = readStringAsDouble(json, "closedSize", closedSize);
    readValue<bool>(json, "isMaker", isMaker);
    execTime = readStringAsInt64(json, "execTime", execTime);
    readValue<std::int64_t>(json, "seq", seq);
}
}
