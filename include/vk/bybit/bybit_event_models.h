/**
Bybit Event Data Models

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@gmail.com>.
*/

#ifndef INCLUDE_VK_BYBIT_EVENT_MODELS_H
#define INCLUDE_VK_BYBIT_EVENT_MODELS_H

#include "vk/interface/i_json.h"
#include "vk/bybit/bybit_enums.h"
#include <nlohmann/json.hpp>

namespace vk::bybit {
struct Event final : IJson {
    std::string topic{};
    ResponseType type{ResponseType::snapshot};
    std::int64_t ts{};
    nlohmann::json data{};

    ~Event() override = default;

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

struct EventTicker final : IJson {
    std::string symbol{};
    double ask1Price{};
    double ask1Size{};
    double bid1Price{};
    double bid1Size{};
    double lastPrice{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;

    void loadEventData(const Event& event);
};

struct EventCandlestick final : IJson {
    std::int64_t start{};
    std::int64_t end{};
    std::string interval{};
    double open{};
    double high{};
    double low{};
    double close{};
    double volume{};
    double turnover{};
    bool confirm{false};
    std::int64_t timestamp{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};
}
#endif //INCLUDE_VK_BYBIT_EVENT_MODELS_H
