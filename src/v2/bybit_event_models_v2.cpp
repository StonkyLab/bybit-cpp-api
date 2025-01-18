/**
Bybit Event Data Models v2

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@gmail.com>.
*/

#include "vk/bybit/v2/bybit_event_models_v2.h"
#include "vk/utils/utils.h"
#include "vk/utils/json_utils.h"

namespace vk::bybit::v2 {

nlohmann::json Event::toJson() const {
    throw std::runtime_error("Unimplemented: Event::toJson()");
}

void Event::fromJson(const nlohmann::json &json) {
    readValue<std::string>(json, "topic", m_topic);
    readMagicEnum<ResponseType>(json, "type", m_type);
    readValue<std::string>(json, "cross_seq", m_crossSeq);
    readValue<std::string>(json, "timestamp_e6", m_timestampE6);
    m_data = json["data"];
}

nlohmann::json EventInstrumentInfo::toJson() const {
    throw std::runtime_error("Unimplemented: EventInstrumentInfo::toJson()");
}

void EventInstrumentInfo::fromJson(const nlohmann::json &json) {
    readValue<std::string>(json, "symbol", m_symbol);
    m_ask1Price = readStringAsDouble(json, "ask1_price", m_ask1Price);
    m_bid1Price = readStringAsDouble(json, "bid1_price", m_bid1Price);
    m_lastPrice = readStringAsDouble(json, "last_price", m_lastPrice);
}

void EventInstrumentInfo::loadEventData(const Event &event) {
    switch (event.m_type) {
        case ResponseType::Snapshot:
            fromJson(event.m_data);
            break;
        case ResponseType::Delta:
            for (const auto &el: event.m_data["update"].items()) {
                fromJson(el.value());
            }
            break;
    }
}

nlohmann::json EventCandlestick::toJson() const {
    throw std::runtime_error("Unimplemented: EventCandlestick::toJson()");
}

void EventCandlestick::fromJson(const nlohmann::json &json) {

    readValue<std::int64_t>(json, "start", m_start);
    readValue<std::int64_t>(json, "end", m_end);
    readValue<std::string>(json, "period", m_period);
    readValue<double>(json, "open", m_open);
    readValue<double>(json, "high", m_high);
    readValue<double>(json, "low", m_low);
    readValue<double>(json, "close", m_close);
    m_volume = readStringAsDouble(json, "volume", m_volume);
    m_turnover = readStringAsDouble(json, "turnover", m_turnover);
    readValue<bool>(json, "confirm", m_confirm);
    readValue<std::int64_t>(json, "timestamp", m_timestamp);
    readValue<std::int64_t>(json, "cross_seq", m_crossSeq);
}

}