/**
Bybit Event Data Models

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@gmail.com>.
*/

#include "vk/bybit/bybit_event_models.h"
#include "vk/utils/utils.h"
#include "vk/utils/json_utils.h"

namespace vk::bybit {
nlohmann::json Event::toJson() const {
    throw std::runtime_error("Unimplemented: Event::toJson()");
}

void Event::fromJson(const nlohmann::json& json) {
    readValue<std::string>(json, "topic", topic);
    readMagicEnum<ResponseType>(json, "type", type);
    readValue<std::int64_t>(json, "ts", ts);
    data = json["data"];
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
}
