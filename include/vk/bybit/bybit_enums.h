/**
Bybit Enums

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@gmail.com>.
*/

#ifndef INCLUDE_VK_BYBIT_ENUMS_H
#define INCLUDE_VK_BYBIT_ENUMS_H

#include "vk/utils/magic_enum_wrapper.hpp"

namespace vk::bybit {
enum class CandleInterval : std::int32_t {
    _1,
    _3,
    _5,
    _15,
    _30,
    _60,
    _120,
    _240,
    _360,
    _720,
    _D,
    _M,
    _W
};

enum class Side : std::int32_t {
    Buy,
    Sell,
    None
};

enum class TpSlMode : std::int32_t {
    Full,
    Partial
};

enum class ContractType : std::int32_t {
    InversePerpetual,
    LinearPerpetual,
    LinearFutures,
    InverseFutures
};

enum class ContractStatus : std::int32_t {
    PreLaunch,
    Trading,
    Settling,
    Delivering,
    Closed
};

enum class OrderType : std::int32_t {
    Limit,
    Market
};

enum class TriggerPriceType : std::int32_t {
    LastPrice,
    IndexPrice,
    MarkPrice,
    UNKNOWN
};

enum class OrderStatus : std::int32_t {
    Created,
    New,
    Rejected,
    PartiallyFilled,
    Filled,
    PendingCancel,
    Cancelled,
    Untriggered,
    Deactivated,
    Triggered,
    Active
};

enum class AccountType : std::int32_t {
    CONTRACT,
    UNIFIED,
    FUND,
    SPOT,
    OPTION
};

enum class Category : std::int32_t {
    spot,
    linear,
    inverse,
    option
};

enum class PositionStatus : std::int32_t {
    Normal,
    Liq, /// in the liquidation progress
    Adl /// in the auto-deleverage progress
};

enum class PositionMode : std::int32_t {
    MergedSingle = 0,
    BothSides = 3
};

enum class TimeInForce : std::int32_t {
    GTC,
    IOC,
    FOK,
    PostOnly
};

enum class ResponseType :std::int32_t {
    snapshot,
    delta
};
}

template <>
constexpr magic_enum::customize::customize_t magic_enum::customize::enum_name<vk::bybit::CandleInterval>(
    const vk::bybit::CandleInterval value) noexcept {
    switch (value) {
    case vk::bybit::CandleInterval::_1:
        return "1";
    case vk::bybit::CandleInterval::_3:
        return "3";
    case vk::bybit::CandleInterval::_5:
        return "5";
    case vk::bybit::CandleInterval::_15:
        return "15";
    case vk::bybit::CandleInterval::_30:
        return "30";
    case vk::bybit::CandleInterval::_60:
        return "60";
    case vk::bybit::CandleInterval::_120:
        return "120";
    case vk::bybit::CandleInterval::_240:
        return "240";
    case vk::bybit::CandleInterval::_360:
        return "360";
    case vk::bybit::CandleInterval::_720:
        return "720";
    case vk::bybit::CandleInterval::_D:
        return "D";
    case vk::bybit::CandleInterval::_M:
        return "M";
    case vk::bybit::CandleInterval::_W:
        return "W";
    }

    return default_tag;
}

#endif //INCLUDE_VK_BYBIT_ENUMS_H
