/**
Bybit Futures REST Client

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@gmail.com>.
*/

#include "vk/bybit/v2/bybit_futures_rest_client_v2.h"
#include "vk/bybit/bybit_http_session.h"
#include "vk/bybit/bybit.h"
#include "vk/tools//utils.h"
#include <fmt/format.h>
#include <mutex>
#include <vk/bybit/bybit_enums.h>

namespace vk::bybit::v2::futures {
template <typename ValueType>
ValueType handleBybitResponse(const http::response<http::string_body>& response) {
    ValueType retVal;
    retVal.fromJson(nlohmann::json::parse(response.body()));

    if (retVal.m_retCode != 0) {
        throw std::runtime_error(
            fmt::format("Bybit API error, code: {}, msg: {}", retVal.m_retCode, retVal.m_retMsg).c_str());
    }

    return retVal;
}

struct RESTClient::P {
private:
    Symbols m_symbols;
    mutable std::recursive_mutex m_locker;

public:
    RESTClient* m_parent = nullptr;
    std::shared_ptr<HTTPSession> m_httpSession;

    explicit P(RESTClient* parent) {
        m_parent = parent;
    }

    [[nodiscard]] Symbols getSymbols() const {
        std::lock_guard lk(m_locker);
        return m_symbols;
    }

    void setSymbols(const Symbols& symbols) {
        std::lock_guard lk(m_locker);
        m_symbols = symbols;
    }

    void setSymbols(const std::vector<Symbol>& symbols) {
        std::lock_guard lk(m_locker);
        m_symbols.m_symbols = symbols;
    }

    bool findPricePrecisionsForSymbol(const std::string& symbol, int& priceScale, double& qtyStep) const {
        for (const auto symbols = m_parent->getSymbols(); const auto& symbolEl : symbols) {
            if (symbolEl.m_name == symbol) {
                priceScale = symbolEl.m_priceScale;
                qtyStep = symbolEl.m_lotSizeFilter.m_qtyStep;
                return true;
            }
        }
        return false;
    }

    static http::response<http::string_body> checkResponse(const http::response<http::string_body>& response) {
        if (response.result() != http::status::ok) {
            throw std::runtime_error(
                fmt::format("Bad response, code {}, msg: {}", response.result_int(), response.body()).c_str());
        }
        return response;
    }

    [[nodiscard]] std::vector<Candle>
    getHistoricalPrices(const std::string& symbol, const CandleInterval interval, const std::int64_t startTime,
                        const std::int32_t limit) const {
        const std::string path = "/public/linear/kline";
        std::map<std::string, std::string> parameters;
        parameters.insert_or_assign("symbol", symbol);
        parameters.insert_or_assign("interval", magic_enum::enum_name(interval));
        parameters.insert_or_assign("from", std::to_string(startTime));

        if (limit != 200) {
            parameters.insert_or_assign("limit", std::to_string(limit));
        }

        const auto response = checkResponse(m_httpSession->get(path, parameters));
        return handleBybitResponse<Candles>(response).m_candles;
    }
};

RESTClient::RESTClient(const std::string& apiKey, const std::string& apiSecret) : m_p(
    std::make_unique<P>(this)) {
    m_p->m_httpSession = std::make_shared<HTTPSession>(apiKey, apiSecret);
}

RESTClient::~RESTClient() = default;

void RESTClient::setCredentials(const std::string& apiKey, const std::string& apiSecret) const {
    m_p->m_httpSession.reset();
    m_p->m_httpSession = std::make_shared<HTTPSession>(apiKey, apiSecret);
}

std::vector<Candle>
RESTClient::getHistoricalPrices(const std::string& symbol, const CandleInterval interval, std::int64_t from,
                                const std::int64_t to,
                                const std::int32_t limit) const {
    std::vector<Candle> retVal;

    std::vector<Candle> candles = m_p->getHistoricalPrices(symbol, interval, from, limit);

    while (!candles.empty()) {
        const auto first = candles.front();
        const auto last = candles.back();

        if (to < last.m_openTime) {
            for (const auto& candle : candles) {
                if (candle.m_openTime <= to) {
                    retVal.push_back(candle);
                }
            }

            break;
        }

        retVal.insert(retVal.end(), candles.begin(), candles.end());
        from = last.m_openTime + Bybit::numberOfMsForCandleInterval(interval) / 1000;

        candles.clear();
        candles = m_p->getHistoricalPrices(symbol, interval, from, limit);
    }

    return retVal;
}

WalletBalance RESTClient::getWalletBalance(const std::string& coin) const {
    const std::string path = "/v2/private/wallet/balance";
    std::map<std::string, std::string> parameters;

    if (!coin.empty()) {
        parameters.insert_or_assign("coin", coin);
    }

    const auto response = P::checkResponse(m_p->m_httpSession->get(path, parameters));
    return handleBybitResponse<WalletBalance>(response);
}

std::int64_t RESTClient::getServerTime() const {
    const std::string path = "/v2/public/time";
    const std::map<std::string, std::string> parameters;

    const auto response = P::checkResponse(m_p->m_httpSession->get(path, parameters));
    const auto timeString = handleBybitResponse<Response>(response).m_timeNow;
    const auto parts = splitString(timeString, '.');

    const std::int64_t seconds = std::stoll(parts[0]);
    const std::int64_t mSeconds = static_cast<std::int64_t>(std::stod("." + parts[1]) * 1000);

    return seconds * 1000 + mSeconds;
}

OrderResponse RESTClient::sendOrder(Order& order) const {
    const std::string path = "/private/linear/order/create";

    int priceScale = 2;
    double qtyStep = 0.01;

    m_p->findPricePrecisionsForSymbol(order.m_symbol, priceScale, qtyStep);

    order.m_priceScale = priceScale;
    order.m_qtyStep = qtyStep;

    const auto response = P::checkResponse(m_p->m_httpSession->post(path, order.toJson()));
    return handleBybitResponse<OrderResponse>(response);
}

std::vector<std::string> RESTClient::cancelAllOrders(const std::string& symbol) const {
    std::vector<std::string> retVal;
    const std::string path = "/private/linear/order/cancel-all";
    std::map<std::string, std::string> parameters;
    parameters.insert_or_assign("symbol", symbol);

    const auto response = P::checkResponse(m_p->m_httpSession->post(path, nlohmann::json(parameters)));

    for (const auto result = handleBybitResponse<Response>(response).m_result; const auto& el : result.items()) {
        retVal.push_back(el.value());
    }

    return retVal;
}

std::string
RESTClient::cancelOrder(const std::string& symbol, const std::string& orderId, const std::string& orderLinkId) const {
    const std::string path = "/private/linear/order/cancel";
    std::map<std::string, std::string> parameters;
    parameters.insert_or_assign("symbol", symbol);

    if (!orderId.empty()) {
        parameters.insert_or_assign("order_id", orderId);
    }

    if (!orderLinkId.empty()) {
        parameters.insert_or_assign("order_link_id", orderId);
    }

    const auto response = P::checkResponse(m_p->m_httpSession->post(path, nlohmann::json(parameters).dump()));
    return handleBybitResponse<Response>(response).m_result["order_id"];
}

std::vector<Position> RESTClient::getPositionInfo(const std::string& symbol) const {
    const std::string path = "/private/linear/position/list";
    std::map<std::string, std::string> parameters;

    if (!symbol.empty()) {
        parameters.insert_or_assign("symbol", symbol);
    }

    const auto response = P::checkResponse(m_p->m_httpSession->get(path, parameters));
    return handleBybitResponse<Positions>(response).m_positions;
}

void RESTClient::setPositionMode(const std::string& symbol, const std::string& coin, PositionMode positionMode) const {
    if (symbol.empty() && coin.empty()) {
        throw std::invalid_argument("Invalid parameters symbol/coin");
    }

    const std::string path = "/private/linear/position/switch-mode";
    std::map<std::string, std::string> parameters;

    if (!symbol.empty()) {
        parameters.insert_or_assign("symbol", symbol);
    }
    if (!coin.empty()) {
        parameters.insert_or_assign("coin", coin);
    }

    parameters.insert_or_assign("mode", magic_enum::enum_name(positionMode));

    const auto response = P::checkResponse(m_p->m_httpSession->post(path, nlohmann::json(parameters)));
    handleBybitResponse<Response>(response);
}

std::vector<Symbol> RESTClient::getSymbols(const bool force) const {
    if (m_p->getSymbols().m_symbols.empty() || force) {
        const std::string path = "/v2/public/symbols";
        const auto response = P::checkResponse(m_p->m_httpSession->get(path, {}));
        m_p->setSymbols(handleBybitResponse<Symbols>(response));
    }

    return m_p->getSymbols().m_symbols;
}

std::vector<OrderResponse> RESTClient::getActiveOrders(const std::string& symbol) const {
    const std::string path = "/private/linear/order/list";
    std::map<std::string, std::string> parameters;
    parameters.insert_or_assign("symbol", symbol);

    const auto response = P::checkResponse(m_p->m_httpSession->get(path, parameters));
    return handleBybitResponse<OrdersResponse>(response).m_orders;
}

std::optional<OrderResponse> RESTClient::getActiveOrder(const std::string& symbol, const std::string& orderId,
                                                        const std::string& orderLinkId) const {
    const std::string path = "/private/linear/order/list";
    std::map<std::string, std::string> parameters;
    parameters.insert_or_assign("symbol", symbol);
    parameters.insert_or_assign("order_id", orderId);
    parameters.insert_or_assign("order_link_id", orderLinkId);

    if (const auto response = P::checkResponse(m_p->m_httpSession->get(path, parameters)); !handleBybitResponse<
        OrdersResponse>(response).m_orders.empty()) {
        return handleBybitResponse<OrdersResponse>(response).m_orders.front();
    }

    return {};
}

void RESTClient::setSymbols(const std::vector<Symbol>& symbols) const {
    m_p->setSymbols(symbols);
}

void RESTClient::closeAllPositions() const {
    for (const auto positions = getPositionInfo(); const auto& position : positions) {
        if (position.m_size != 0) {
            Order order;
            order.m_symbol = position.m_symbol;

            if (position.m_side == Side::Buy) {
                order.m_side = Side::Sell;
            }
            else {
                order.m_side = Side::Buy;
            }

            order.m_orderType = OrderType::Market;
            order.m_qty = position.m_size;
            order.m_timeInForce = TimeInForce::GoodTillCancel;
            auto orderResponse = sendOrder(order);
        }
    }
}

FundingRate RESTClient::getLastFundingRate(const std::string& symbol) const {
    const std::string path = "/public/linear/funding/prev-funding-rate";
    std::map<std::string, std::string> parameters;
    parameters.insert_or_assign("symbol", symbol);

    const auto response = P::checkResponse(m_p->m_httpSession->get(path, parameters));
    return handleBybitResponse<FundingRate>(response);
}

std::optional<OrderResponse> RESTClient::queryActiveOrder(const std::string& symbol, const std::string& orderId,
                                                          const std::string& orderLinkId) const {
    const std::string path = "/private/linear/order/search";
    std::map<std::string, std::string> parameters;
    parameters.insert_or_assign("symbol", symbol);
    parameters.insert_or_assign("order_id", orderId);
    parameters.insert_or_assign("order_link_id", orderLinkId);

    const auto response = P::checkResponse(m_p->m_httpSession->get(path, parameters));
    return handleBybitResponse<OrderResponse>(response);
}
}
