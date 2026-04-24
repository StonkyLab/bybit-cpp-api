/**
Bybit Common Stuff

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.
*/

#ifndef INCLUDE_STONKY_BYBIT_API_H
#define INCLUDE_STONKY_BYBIT_API_H

#include "stonky/bybit/bybit_models.h"

namespace stonky::bybit {
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
} // namespace stonky::bybit
#endif // INCLUDE_STONKY_BYBIT_API_H
