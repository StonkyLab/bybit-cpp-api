/**
Bybit Futures REST Client

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@gmail.com>.
*/

#ifndef INCLUDE_VK_BYBIT_FUTURES_REST_CLIENT_H
#define INCLUDE_VK_BYBIT_FUTURES_REST_CLIENT_H

#include "vk/bybit/bybit_models.h"
#include <string>
#include <memory>
#include <optional>

namespace vk::bybit {

class RESTClient {

    struct P;
    std::unique_ptr <P> m_p{};

public:

    RESTClient(const std::string &apiKey, const std::string &apiSecret);

	~RESTClient();

    /**
     * Set credentials to the RESTClient instance, it will reset the underlying HTTP Session
     * @param apiKey
     * @param apiSecret
     */
    void setCredentials(const std::string &apiKey, const std::string &apiSecret) const;

    /**
     * Download historical candles
     * @param category i.e. Spot, Linear...
     * @param symbol e.g. BTCUSDT
     * @param interval
     * @param from timestamp in ms, must be smaller than "to"
     * @param to timestamp in ms, must be bigger than "from"
     * @param limit maximum number of returned candles, maximum and also the default values is 200
     * @return vector of Candle structures
     * @throws nlohmann::json::exception, std::exception
     * @see https://bybit-exchange.github.io/docs/v5/market/kline
     */
    [[nodiscard]] std::vector<Candle>
    getHistoricalPrices(Category category, const std::string &symbol, CandleInterval interval, std::int64_t from, std::int64_t to,
                        std::int32_t limit = 200) const;

	/**
	 * Get wallet balance info
	 * @param coin e.g. USDT, returns all wallet balances if empty
	 * @param accountType Unified account: UNIFIED (trade spot/linear/options), CONTRACT(trade inverse), Classic account: CONTRACT, SPOT
	 * @return WalletBalance structure
	 * @throws nlohmann::json::exception, std::exception
	 * @see https://bybit-exchange.github.io/docs/v5/account/wallet-balance
	 */
    [[nodiscard]] WalletBalance getWalletBalance(AccountType accountType, const std::string &coin = "") const;

    /**
     * Returns server time in ms
     * @return timestamp in ms
     * @throws nlohmann::json::exception, std::exception
     * @see https://bybit-exchange.github.io/docs/v5/market/time
    */
    [[nodiscard]] std::int64_t getServerTime() const;

    /**
     * Get position info - if Hedge mode is enabled then there is more than one Position
     * @param category i.e. Spot, Linear...
     * @param symbol e.g. BTCUSDT or empty for all symbols
     * @return vector of Position structures
     * @throws nlohmann::json::exception, std::exception
     * @see https://bybit-exchange.github.io/docs/v5/position
     */
    [[nodiscard]] std::vector<Position> getPositionInfo(Category category, const std::string &symbol = "") const;

    /**
     * Get instruments info
     * @param category i.e. Spot, Linear...
     * @param symbol e.g. BTCUSDT or empty for all symbols
     * @param force Reload instruments info from server if true
     * @throws nlohmann::json::exception, std::exception
     * @return vector of Instrument structures
     * @see https://bybit-exchange.github.io/docs/v5/market/instrument
     */
    [[nodiscard]] std::vector<Instrument> getInstrumentsInfo(Category category, const std::string &symbol = "", bool force = false) const;

    /**
     * Switching between One-Way Mode and Hedge Mode
     * @param category i.e. Spot, Linear...
     * @param symbol e.g. BTCUSDT, Required if not passing coin
     * @param coin e.g. USDT - currency alias. Required if not passing symbol
     * @param positionMode MergedSingle: One-Way Mode; BothSides: Hedge Mode
     * @throws nlohmann::json::exception, std::exception
     * @return True if success
     * @see https://bybit-exchange.github.io/docs/v5/position/position-mode
     */
    bool setPositionMode(Category category, const std::string &symbol, const std::string &coin, PositionMode positionMode) const;

    /**
     * Place order
     * @param order Requested order
     * @return Filled OrderId structure
     * @throws nlohmann::json::exception, std::exception
     * @see https://bybit-exchange.github.io/docs/v5/order/create-order
    */
    [[nodiscard]] OrderId placeOrder(Order &order) const;

    /**
     * Get open orders list
     * @param category i.e. Spot, Linear...
     * @param symbol e.g. BTCUSDT
     * @throws nlohmann::json::exception, std::exception
     * @return vector of OrderResponse structures
     * @see https://bybit-exchange.github.io/docs/v5/order/open-order
     */
    [[nodiscard]] std::vector<OrderResponse> getOpenOrders(Category category, const std::string &symbol) const;

    /**
     * Get open order. Because order creation/cancellation is asynchronous, there can be a data delay in this
     * endpoint. You can get real-time order info with the Query Active Order (real-time) endpoint.
     * @param category i.e. Spot, Linear...
     * @param symbol e.g. BTCUSDT
     * @param orderId Order ID
     * @param orderLinkId Unique user-set order ID. Maximum length of 36 characters
     * @return Filled OrderResponse structure if success
     * @see https://bybit-exchange.github.io/docs/v5/order/open-order
     */
    [[nodiscard]] std::optional<OrderResponse>
    getOpenOrder(Category category, const std::string &symbol, const std::string &orderId, const std::string &orderLinkId) const;

    /**
     * Cancel all orders for a given symbol
     * @param category i.e. Spot, Linear...
     * @param symbol e.g. BTCUSDT
     * @return vector of OrderId of canceled orders
     * @see https://bybit-exchange.github.io/docs/v5/order/cancel-allctive
     */
    [[nodiscard]] std::vector<OrderId> cancelAllOrders(Category category, const std::string &symbol = "") const;

    /**
     * Cancel order
     * @param category i.e. Spot, Linear...
     * @param symbol e.g. BTCUSDT
     * @param orderId
     * @param orderLinkId
     * @return OrderId structure
     * @throws nlohmann::json::exception, std::exception
     * @see https://bybit-exchange.github.io/docs/v5/order/cancel-order
    */
    [[nodiscard]] OrderId
    cancelOrder(Category category, const std::string &symbol, const std::string &orderId = "", const std::string &orderLinkId = "") const;

	/**
	 * Set instruments
	 * @param instruments
	 */
	void setInstruments(const std::vector<Instrument> &instruments) const;

    /**
     * Close all open positions with market order
     * @param category i.e. Spot, Linear...
     */
    void closeAllPositions(Category category) const;
};
}

#endif //INCLUDE_VK_BYBIT_FUTURES_REST_CLIENT_H