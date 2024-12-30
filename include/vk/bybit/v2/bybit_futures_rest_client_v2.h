/**
Bybit Futures REST Client v2

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@gmail.com>.
*/

#ifndef INCLUDE_VK_BYBIT_FUTURES_REST_CLIENT_V2_H
#define INCLUDE_VK_BYBIT_FUTURES_REST_CLIENT_V2_H

#include "bybit_models_v2.h"
#include <string>
#include <memory>
#include <optional>
#include <vk/bybit/bybit_enums.h>

namespace vk::bybit::v2::futures {
class RESTClient {
    struct P;
    std::unique_ptr<P> m_p{};

public:
    RESTClient(const std::string& apiKey, const std::string& apiSecret);

    ~RESTClient();

    /**
     * Set credentials to the RESTClient instance, it will reset the underlying HTTP Session
     * @param apiKey
     * @param apiSecret
     */
    void setCredentials(const std::string& apiKey, const std::string& apiSecret) const;

    /**
     * Download historical candles
     * @param symbol e,g BTCUSDT
     * @param interval
     * @param from timestamp in ms, must be smaller than "to"
     * @param to
     * @param limit maximum number of returned candles, maximum and also the default values is 200
     * @return vector of Candle structures
     * @throws nlohmann::json::exception, std::exception
     * @see https://bybit-exchange.github.io/docs/futuresV2/linear/#t-querykline
     */
    [[nodiscard]] std::vector<Candle>
    getHistoricalPrices(const std::string& symbol, CandleInterval interval, std::int64_t from, std::int64_t to,
                        std::int32_t limit = 200) const;

    /**
     * Get wallet balance info
     * @param coin e.g. USDT, returns all wallet balances if empty
     * @return WalletBalance structure
     * @throws nlohmann::json::exception, std::exception
     * @see https://bybit-exchange.github.io/docs/futuresV2/linear/#t-balance
     */
    [[nodiscard]] WalletBalance getWalletBalance(const std::string& coin = "") const;

    /**
     * Returns server time in ms
     * @return timestamp in ms
     * @throws nlohmann::json::exception, std::exception
     * @see https://bybit-exchange.github.io/docs/futuresV2/linear/#t-servertime
    */
    [[nodiscard]] std::int64_t getServerTime() const;

    /**
     * Send order
     * @param order
     * @return Filled OrderResponse structure
     * @throws nlohmann::json::exception, std::exception
     * @see https://bybit-exchange.github.io/docs/futuresV2/linear/#t-placeactive
    */
    [[nodiscard]] OrderResponse sendOrder(Order& order) const;

    /**
     * Cancel order
     * @param symbol e.g. BTCUSDT
     * @param orderId
     * @param orderLinkId
     * @return OrderId as a string
     * @throws nlohmann::json::exception, std::exception
     * @see https://bybit-exchange.github.io/docs/futuresV2/linear/#t-cancelactive
    */
    [[nodiscard]] std::string
    cancelOrder(const std::string& symbol, const std::string& orderId = "", const std::string& orderLinkId = "") const;

    /**
     * Cancel all orders for a give symbol
     * @param symbol e.g. BTCUSDT
     * @return vector of ids of canceled orders
     * @see https://bybit-exchange.github.io/docs/futuresV2/linear/#t-cancelallactive
     */
    [[nodiscard]] std::vector<std::string> cancelAllOrders(const std::string& symbol) const;

    /**
     * Get position info - if Hedge mode is enabled then there is more than one Position
     * @param symbol e.g. BTCUSDT or empty for all symbols
     * @return vector of Position structures
     * @throws nlohmann::json::exception, std::exception
     * @see https://bybit-exchange.github.io/docs/futuresV2/linear/#t-myposition
     */
    [[nodiscard]] std::vector<Position> getPositionInfo(const std::string& symbol = "") const;

    /**
     * Switching between One-Way Mode and Hedge Mode
     * @param symbol e.g. BTCUSDT, Required if not passing coin
     * @param coin e.g. USDT - currency alias. Required if not passing symbol
     * @param positionMode MergedSingle: One-Way Mode; BothSide: Hedge Mode
     * @throws nlohmann::json::exception, std::exception
     * @return True if success
     * @see https://bybit-exchange.github.io/docs/futuresV2/linear/#t-switchpositionmode
     */
    void setPositionMode(const std::string& symbol, const std::string& coin, PositionMode positionMode) const;

    /**
     * Get symbols info
     * @@param force Reload symbols info from server if true
     * @throws nlohmann::json::exception, std::exception
     * @return vector of Symbol structures
     * @see https://bybit-exchange.github.io/docs/futuresV2/linear/#t-querysymbol
     */
    [[nodiscard]] std::vector<Symbol> getSymbols(bool force = false) const;

    /**
     * Get active order list
     * @param symbol e.g. BTCUSDT
     * @throws nlohmann::json::exception, std::exception
     * @return vector of OrderResponse structures
     * @see https://bybit-exchange.github.io/docs/futuresV2/linear/#t-getactive
     */
    [[nodiscard]] std::vector<OrderResponse> getActiveOrders(const std::string& symbol) const;

    /**
     * Get active order. Because order creation/cancellation is asynchronous, there can be a data delay in this
     * endpoint. You can get real-time order info with the Query Active Order (real-time) endpoint.
     * @param symbol e.g. BTCUSDT
     * @param orderId Order ID
     * @param orderLinkId Unique user-set order ID. Maximum length of 36 characters
     * @return Filled OrderResponse structure if success
     * @see https://bybit-exchange.github.io/docs/futuresV2/linear/#t-getactive
     */
    [[nodiscard]] std::optional<OrderResponse>
    getActiveOrder(const std::string& symbol, const std::string& orderId, const std::string& orderLinkId) const;

    /**
     * Set symbols
     * @param symbols
     */
    void setSymbols(const std::vector<Symbol>& symbols) const;

    /**
     * Close all open positions with market order
     */
    void closeAllPositions() const;

    /**
     * Returns the most recent funding rate for a specified symbol.
     * @param symbol e.g. BTCUSDT, must not be empty
     * @return Filled FundingRate structure
     */
    [[nodiscard]] FundingRate getLastFundingRate(const std::string& symbol) const;

    /**
     * Query real-time active order information.
     * @param symbol e.g. BTCUSDT
     * @param orderId Order ID
     * @param orderLinkId Unique user-set order ID. Maximum length of 36 characters
     * @return Filled OrderResponse structure if success
     * @see https://bybit-exchange.github.io/docs/futuresV2/linear/#t-queryactive
     */
    [[nodiscard]] std::optional<OrderResponse>
    queryActiveOrder(const std::string& symbol, const std::string& orderId, const std::string& orderLinkId) const;
};
}

#endif //INCLUDE_VK_BYBIT_FUTURES_REST_CLIENT_V2_H
