/**
Bybit Common Stuff

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@gmail.com>.
*/

#ifndef INCLUDE_VK_BYBIT_API_H
#define INCLUDE_VK_BYBIT_API_H

#include "vk/bybit/v2/bybit_models_v2.h"

namespace vk::bybit {
class Bybit {
public:
    /**
     * Check if the input resolution in minutes is valid, if so then return corresponding API string
     * @param resolution Candle resolution in minutes.
     * @param candleInterval out: CandleInterval enum value
     * @return Tru if input resolution is valid
     */
    static bool isValidCandleResolution(std::int32_t resolution, CandleInterval& candleInterval);

    /**
     * Get number of ms for given candle interval
     * @param candleInterval
     * @return
     */
    static int64_t numberOfMsForCandleInterval(CandleInterval candleInterval);
};
}
#endif //INCLUDE_VK_BYBIT_API_H
