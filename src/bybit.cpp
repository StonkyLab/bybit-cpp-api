/**
Bybit Common Stuff

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@gmail.com>.
*/

#include "vk/bybit/bybit.h"

namespace vk::bybit {
int64_t Bybit::numberOfMsForCandleInterval(const CandleInterval candleInterval) {
    switch (candleInterval) {
        case CandleInterval::_1:
            return 60000;
        case CandleInterval::_3:
            return 60000 * 3;
        case CandleInterval::_5:
            return 60000 * 5;
        case CandleInterval::_15:
            return 60000 * 15;
        case CandleInterval::_30:
            return 60000 * 30;
        case CandleInterval::_60:
            return 60000 * 60;
        case CandleInterval::_120:
            return 60000 * 120;
        case CandleInterval::_240:
            return 60000 * 240;
        case CandleInterval::_360:
            return 60000 * 360;
        case CandleInterval::_720:
            return 60000 * 720;
        case CandleInterval::_D:
            return 86400000;
        case CandleInterval::_W:
            return 86400000 * 7;
        case CandleInterval::_M:
            return static_cast<int64_t>(86400000) * 30;
        default:
            return 0;
    }
}

bool Bybit::isValidCandleResolution(const std::int32_t resolution, CandleInterval& candleInterval) {
    switch (resolution) {
        case 1:
            candleInterval = CandleInterval::_1;
            return true;
        case 3:
            candleInterval = CandleInterval::_3;
            return true;
        case 5:
            candleInterval = CandleInterval::_5;
            return true;
        case 15:
            candleInterval = CandleInterval::_15;
            return true;
        case 30:
            candleInterval = CandleInterval::_30;
            return true;
        case 60:
            candleInterval = CandleInterval::_60;
            return true;
        case 120:
            candleInterval = CandleInterval::_120;
            return true;
        case 240:
            candleInterval = CandleInterval::_240;
            return true;
        case 360:
            candleInterval = CandleInterval::_360;
            return true;
        case 720:
            candleInterval = CandleInterval::_720;
            return true;
        case 1440:
            candleInterval = CandleInterval::_D;
            return true;
        case 10080:
            candleInterval = CandleInterval::_W;
            return true;
        case 40320:
            candleInterval = CandleInterval::_M;
            return true;
        default:
            return false;
    }
}
} // namespace vk::bybit
