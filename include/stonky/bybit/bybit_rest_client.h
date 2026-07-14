/**
Bybit Futures REST Client

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.
*/

#ifndef INCLUDE_STONKY_BYBIT_FUTURES_REST_CLIENT_H
#define INCLUDE_STONKY_BYBIT_FUTURES_REST_CLIENT_H

#include "stonky/bybit/bybit_models.h"
#include "stonky/bybit/bybit_event_models.h"
#include <string>
#include <memory>
#include <optional>

namespace stonky::bybit {

using onCandlesDownloaded = std::function<void(const std::vector<Candle>&)>;

class RESTClient {
    struct P;
    std::unique_ptr<P> m_p{};

public:
    /**
     * @param apiKey
     * @param apiSecret
     * @param env Bybit environment — Mainnet (default), Testnet or Demo trading.
     *            Selects the REST host (api / api-testnet / api-demo .bybit.com).
     */
    RESTClient(const std::string& apiKey, const std::string& apiSecret, Environment env = Environment::Mainnet);

    ~RESTClient();

    /**
     * Set credentials to the RESTClient instance, it will reset the underlying HTTP Session
     * @param apiKey
     * @param apiSecret
     */
    void setCredentials(const std::string& apiKey, const std::string& apiSecret) const;

    /**
     * Download historical candles
     * @param category i.e. Spot, Linear...
     * @param symbol e.g. BTCUSDT
     * @param interval
     * @param from timestamp in ms, must be smaller than "to"
     * @param to timestamp in ms, must be bigger than "from"
     * @param limit maximum number of returned candles, maximum and also the default values is 200
     * @param writer
     * @return vector of Candle structures
     * @throws nlohmann::json::exception, std::exception
     * @see https://bybit-exchange.github.io/docs/v5/market/kline
     */
    [[nodiscard]] std::vector<Candle>
    getHistoricalPrices(Category category, const std::string& symbol, CandleInterval interval, std::int64_t from,
                        std::int64_t to,
                        std::int32_t limit = 200, const onCandlesDownloaded &writer = {}) const;

    /**
     * Get wallet balance info
     * @param coin e.g. USDT, returns all wallet balances if empty
     * @param accountType Unified account: UNIFIED (trade spot/linear/options), CONTRACT(trade inverse), Classic account: CONTRACT, SPOT
     * @return WalletBalance structure
     * @throws nlohmann::json::exception, std::exception
     * @see https://bybit-exchange.github.io/docs/v5/account/wallet-balance
     */
    [[nodiscard]] WalletBalance getWalletBalance(AccountType accountType, const std::string& coin = "") const;

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
     * @param symbol e.g. BTCUSDT; for linear/inverse leave empty and pass settleCoin instead
     * @param settleCoin e.g. USDT — linear/inverse require symbol OR settleCoin when
     *        listing all positions (Bybit rejects category=linear with neither: retCode 10001)
     * @return vector of Position structures
     * @throws nlohmann::json::exception, std::exception
     * @see https://bybit-exchange.github.io/docs/v5/position
     */
    [[nodiscard]] std::vector<Position> getPositionInfo(Category category, const std::string& symbol = "", const std::string& settleCoin = "") const;

    /**
     * Get instruments info
     * @param category i.e. Spot, Linear...
     * @param symbol e.g. BTCUSDT or empty for all symbols
     * @param force Reload instruments info from server if true
     * @throws nlohmann::json::exception, std::exception
     * @return vector of Instrument structures
     * @see https://bybit-exchange.github.io/docs/v5/market/instrument
     */
    [[nodiscard]] std::vector<Instrument> getInstrumentsInfo(Category category, const std::string& symbol = "",
                                                             bool force = false,
                                                             const std::string& status = "") const;

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
    [[nodiscard]] bool setPositionMode(Category category, const std::string& symbol, const std::string& coin,
                                       PositionMode positionMode) const;

    /**
     * Place order
     * @param order Requested order
     * @return Filled OrderId structure
     * @throws nlohmann::json::exception, std::exception
     * @see https://bybit-exchange.github.io/docs/v5/order/create-order
    */
    [[nodiscard]] OrderId placeOrder(Order& order) const;

    /**
     * Amend an open order in place (price and/or quantity). The order keeps its
     * orderId/orderLinkId; Bybit re-evaluates queue priority at the new price
     * level in a single venue transaction (one round-trip vs cancel + re-create).
     * @param category i.e. Spot, Linear...
     * @param symbol e.g. BTCUSDT
     * @param orderId Venue order id; required if orderLinkId is empty
     * @param orderLinkId User-set order id; required if orderId is empty
     * @param price New limit price; pass 0 to leave the price unchanged
     * @param qty New order quantity; pass 0 to leave the quantity unchanged
     * @return OrderId structure of the amended order
     * @throws nlohmann::json::exception, std::exception
     * @see https://bybit-exchange.github.io/docs/v5/order/amend-order
     */
    [[nodiscard]] OrderId amendOrder(Category category, const std::string& symbol,
                                     const std::string& orderId, const std::string& orderLinkId,
                                     double price, double qty = 0.0) const;

    /**
     * Get open orders list
     * @param category i.e. Spot, Linear...
     * @param symbol e.g. BTCUSDT
     * @throws nlohmann::json::exception, std::exception
     * @return vector of OrderResponse structures
     * @see https://bybit-exchange.github.io/docs/v5/order/open-order
     */
    [[nodiscard]] std::vector<OrderResponse> getOpenOrders(Category category, const std::string& symbol) const;

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
    getOpenOrder(Category category, const std::string& symbol, const std::string& orderId,
                 const std::string& orderLinkId) const;

    /**
     * Get an order's executions (trade history) — used to reconcile fills the
     * private WS may have dropped during a reconnect gap. Query by orderLinkId
     * to fetch every fill of one order; each carries a stable execId that the
     * chase core dedups against the WS execution feed (re-crediting is harmless).
     * @param category i.e. Spot, Linear...
     * @param symbol e.g. BTCUSDT
     * @param orderLinkId user-set order id (our clientOrderId)
     * @return executions for the order (execType == Trade are the real fills)
     * @see https://bybit-exchange.github.io/docs/v5/order/execution
     */
    [[nodiscard]] std::vector<EventExecution>
    getExecutions(Category category, const std::string& symbol, const std::string& orderLinkId) const;

    /**
     * Cancel all orders for a given symbol
     * @param category i.e. Spot, Linear...
     * @param symbol e.g. BTCUSDT
     * @return vector of OrderId of canceled orders
     * @see https://bybit-exchange.github.io/docs/v5/order/cancel-allctive
     */
    [[nodiscard]] std::vector<OrderId> cancelAllOrders(Category category, const std::string& symbol = "") const;

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
    cancelOrder(Category category, const std::string& symbol, const std::string& orderId = "",
                const std::string& orderLinkId = "") const;

    /**
     * Set instruments
     * @param instruments
     */
    void setInstruments(const std::vector<Instrument>& instruments) const;

    /**
     * Close all open positions with market order
     * @param category i.e. Spot, Linear...
     * @param settleCoin e.g. USDT — required for linear/inverse (see getPositionInfo)
     */
    void closeAllPositions(Category category, const std::string& settleCoin = "") const;

    /**
     * Returns a vector of funding rates for a given category and symbol.
     * @param category i.e. Spot, Linear...
     * @param symbol e.g. BTCUSDT
     * @param startTime timestamp in ms to get funding rate from INCLUSIVE.
     * @param endTime timestamp in ms to get funding rate until INCLUSIVE.
     * @return vector of FundingRate structures
     * @throws nlohmann::json::exception, std::exception
     * @see https://bybit-exchange.github.io/docs/v5/market/history-fund-rate
     */
    [[nodiscard]] std::vector<FundingRate>
    getFundingRates(Category category, const std::string& symbol, int64_t startTime, int64_t endTime,
                    std::int32_t = 200) const;

    /**
     * Query for the latest price snapshot, best bid/ask price, and trading volume in the last 24 hours.
     * @param category  i.e. Spot, Linear...
     * @param symbol e.g. BTCUSDT, if empty then all available tickers are returned
     * @return vector of Ticker structures
     * @see https://bybit-exchange.github.io/docs/v5/market/tickers
     */
    [[nodiscard]] Tickers getTickers(Category category, const std::string& symbol) const;

    /**
     * Finds the last available trading day for a delisted spot symbol by inspecting the
     * public.bybit.com/spot/SYMBOL/ directory listing and returns the end-of-day UTC
     * timestamp (ms) of the newest CSV file found there.
     * Returns 0 if the symbol directory cannot be fetched or contains no dated files.
     * @param symbol e.g. VRAUSDT
     * @return end-of-day UTC timestamp in milliseconds, or 0 on failure
     */
    [[nodiscard]] std::int64_t fetchLastTimestampForDelistedSpotSymbol(const std::string& symbol) const;
};
}

#endif //INCLUDE_STONKY_BYBIT_FUTURES_REST_CLIENT_H
