/**
Bybit Module Factory

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2026 Vitezslav Kot <vitezslav.kot@gmail.com>.
*/

#ifndef BYBIT_API_BYBIT_MODULE_H
#define BYBIT_API_BYBIT_MODULE_H

#include "bybit_futures_exchange_connector.h"
#include "bybit_spot_exchange_connector.h"
#include "vk/common/module_factory.h"
#include "vk/interface/exchange_enums.h"
#include "vk/interface/i_module_factory.h"
#include "vk/utils//magic_enum_wrapper.hpp"

namespace vk {
BOOST_SYMBOL_EXPORT IModuleFactory *getModuleFactory() {
    if (!g_moduleFactory) {
        FactoryInfo factoryInfo;
        factoryInfo.m_id = std::string("Bybit");
        factoryInfo.m_description = "Bybit CEX";

        g_moduleFactory = new ModuleFactory(factoryInfo);
        g_moduleFactory->registerClassByName<IExchangeConnector>(std::string(magic_enum::enum_name(ExchangeId::BybitFutures)), &BybitFuturesExchangeConnector::createInstance);
        g_moduleFactory->registerClassByName<IExchangeConnector>(std::string(magic_enum::enum_name(ExchangeId::BybitSpot)), &BybitSpotExchangeConnector::createInstance);
    } else {
        return nullptr;
    }

    return g_moduleFactory;
}
} // namespace vk
#endif // BYBIT_API_BYBIT_MODULE_H
