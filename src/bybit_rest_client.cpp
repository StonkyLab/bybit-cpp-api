/**
Bybit Futures REST Client

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@gmail.com>.
*/

#include "vk/bybit/bybit_rest_client.h"
#include "vk/bybit/bybit_http_session.h"
#include "vk/bybit/bybit.h"
#include "vk/utils/utils.h"
#include <mutex>

namespace vk::bybit {
template<typename ValueType>
ValueType handleBybitResponse(const http::response<http::string_body> &response) {
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
	Instruments m_instruments;
	mutable std::recursive_mutex m_locker;

public:
	RESTClient *m_parent = nullptr;
	std::shared_ptr<HTTPSession> m_httpSession;

	explicit P(RESTClient *parent) {
		m_parent = parent;
	}

	[[nodiscard]] Instruments getInstruments() const {
		std::lock_guard lk(m_locker);
		return m_instruments;
	}

	void setInstruments(const Instruments &instruments) {
		std::lock_guard lk(m_locker);
		m_instruments = instruments;
	}

	void setInstruments(const std::vector<Instrument> &instruments) {
		std::lock_guard lk(m_locker);
		m_instruments.m_instruments = instruments;
	}

	bool findPricePrecisionsForInstrument(const Category category,
	                                      const std::string &symbol,
	                                      double &priceStep,
	                                      double &qtyStep) const {
		for (const auto symbols = m_parent->getInstrumentsInfo(category); const auto &symbolEl: symbols) {
			if (symbolEl.m_symbol == symbol) {
				priceStep = symbolEl.m_priceFilter.m_tickSize;
				qtyStep = symbolEl.m_lotSizeFilter.m_qtyStep;
				return true;
			}
		}
		return false;
	}

	static http::response<http::string_body> checkResponse(const http::response<http::string_body> &response) {
		if (response.result() != http::status::ok) {
			throw std::runtime_error(
				fmt::format("Bad response, code {}, msg: {}", response.result_int(), response.body()).c_str());
		}
		return response;
	}

	[[nodiscard]] std::vector<Candle>
	getHistoricalPrices(const Category category,
	                    const std::string &symbol,
	                    const CandleInterval interval,
	                    const std::int64_t startTime,
	                    const std::int32_t limit) const {
		const std::string path = "/v5/market/kline";
		std::map<std::string, std::string> parameters;
		parameters.insert_or_assign("category", magic_enum::enum_name(category));
		parameters.insert_or_assign("symbol", symbol);
		parameters.insert_or_assign("interval", magic_enum::enum_name(interval));
		parameters.insert_or_assign("start", std::to_string(startTime));

		if (limit != 200) {
			parameters.insert_or_assign("limit", std::to_string(limit));
		}

		const auto response = checkResponse(m_httpSession->get(path, parameters));
		return handleBybitResponse<Candles>(response).m_candles;
	}

	[[nodiscard]] std::vector<FundingRate> getFundingRates(const Category category,
	                                                       const std::string &symbol,
	                                                       const std::int64_t startTime,
	                                                       const int64_t endTime,
	                                                       const std::int32_t limit) const {
		const std::string path = "/v5/market/funding/history";
		std::map<std::string, std::string> parameters;
		parameters.insert_or_assign("category", magic_enum::enum_name(category));
		parameters.insert_or_assign("symbol", symbol);
		parameters.insert_or_assign("startTime", std::to_string(startTime));
		parameters.insert_or_assign("endTime", std::to_string(endTime));

		if (limit != 200) {
			parameters.insert_or_assign("limit", std::to_string(limit));
		}

		const auto response = checkResponse(m_httpSession->get(path, parameters));
		return handleBybitResponse<FundingRates>(response).m_fundingRates;
	}

	Instruments getInstrumentsInfo(const Category category, const std::string &symbol,
	                               const std::string &cursor) const {
		const std::string path = "/v5/market/instruments-info";
		std::map<std::string, std::string> parameters;
		parameters.insert_or_assign("category", magic_enum::enum_name(category));

		if (!symbol.empty()) {
			parameters.insert_or_assign("symbol", symbol);
		}

		if (!cursor.empty()) {
			parameters.insert_or_assign("cursor", cursor);
		}

		const auto response = checkResponse(m_httpSession->get(path, parameters));
		return handleBybitResponse<Instruments>(response);
	}
};

RESTClient::RESTClient(const std::string &apiKey, const std::string &apiSecret) : m_p(
	std::make_unique<P>(this)) {
	m_p->m_httpSession = std::make_shared<HTTPSession>(apiKey, apiSecret);
}

RESTClient::~RESTClient() = default;

void RESTClient::setCredentials(const std::string &apiKey, const std::string &apiSecret) const {
	m_p->m_httpSession.reset();
	m_p->m_httpSession = std::make_shared<HTTPSession>(apiKey, apiSecret);
}

std::vector<Candle>
RESTClient::getHistoricalPrices(const Category category,
                                const std::string &symbol,
                                const CandleInterval interval,
                                std::int64_t from,
                                const std::int64_t to,
                                const std::int32_t limit) const {
	std::vector<Candle> retVal;
	std::vector<Candle> candles = m_p->getHistoricalPrices(category, symbol, interval, from, limit);

	while (!candles.empty()) {
		std::ranges::reverse(candles);

		const auto first = candles.front();
		const auto last = candles.back();

		if (to < last.m_startTime) {
			for (const auto &candle: candles) {
				if (candle.m_startTime <= to) {
					retVal.push_back(candle);
				}
			}

			break;
		}

		retVal.insert(retVal.end(), candles.begin(), candles.end());
		from = last.m_startTime + Bybit::numberOfMsForCandleInterval(interval);

		candles.clear();
		candles = m_p->getHistoricalPrices(category, symbol, interval, from, limit);
	}

	return retVal;
}

WalletBalance RESTClient::getWalletBalance(const AccountType accountType, const std::string &coin) const {
	const std::string path = "/v5/account/wallet-balance";
	std::map<std::string, std::string> parameters;

	parameters.insert_or_assign("accountType", magic_enum::enum_name(accountType));

	if (!coin.empty()) {
		parameters.insert_or_assign("coin", coin);
	}

	const auto response = P::checkResponse(m_p->m_httpSession->get(path, parameters));
	return handleBybitResponse<WalletBalance>(response);
}

std::int64_t RESTClient::getServerTime() const {
	const std::string path = "/v5/market/time";
	const std::map<std::string, std::string> parameters;

	const auto response = P::checkResponse(m_p->m_httpSession->get(path, parameters));
	const auto timeResponse = handleBybitResponse<ServerTime>(response);

	return timeResponse.m_timeNano / 1000000;
}

std::vector<Position> RESTClient::getPositionInfo(const Category category, const std::string &symbol) const {
	const std::string path = "/v5/position/list";
	std::map<std::string, std::string> parameters;

	parameters.insert_or_assign("category", magic_enum::enum_name(category));

	if (!symbol.empty()) {
		parameters.insert_or_assign("symbol", symbol);
	}

	const auto response = P::checkResponse(m_p->m_httpSession->get(path, parameters));
	return handleBybitResponse<Positions>(response).m_positions;
}

std::vector<Instrument>
RESTClient::getInstrumentsInfo(const Category category, const std::string &symbol, const bool force) const {
	if (m_p->getInstruments().m_instruments.empty() || force) {
		Instruments instruments;
		std::vector<Instrument> temp;

		do {
			instruments = m_p->getInstrumentsInfo(category, symbol, instruments.m_nextPageCursor);

			for (const auto &instrument: instruments.m_instruments) {
				temp.push_back(instrument);
			}
		} while (!instruments.m_nextPageCursor.empty());

		m_p->setInstruments(temp);
	}

	return m_p->getInstruments().m_instruments;
}

bool RESTClient::setPositionMode(Category category,
                                 const std::string &symbol,
                                 const std::string &coin,
                                 PositionMode positionMode) const {
	if (symbol.empty() && coin.empty()) {
		throw std::invalid_argument("Invalid parameters symbol/coin");
	}

	std::string path = "/v5/position/switch-mode";
	std::map<std::string, std::string> parameters;
	parameters.insert_or_assign("category", magic_enum::enum_name(category));

	if (!symbol.empty()) {
		parameters.insert_or_assign("symbol", symbol);
	}
	if (!coin.empty()) {
		parameters.insert_or_assign("coin", coin);
	}

	auto payload = nlohmann::json(parameters);
	payload["mode"] = positionMode;

	const auto response = P::checkResponse(m_p->m_httpSession->post(path, payload));

	try {
		return handleBybitResponse<Response>(response).m_retMsg == "OK";
	} catch (std::exception &) {
		Response resp;
		resp.fromJson(nlohmann::json::parse(response.body()));

		if (resp.m_retMsg == "Position mode is not modified") {
			return true;
		}
	}

	return false;
}

OrderId RESTClient::placeOrder(Order &order) const {
	const std::string path = "/v5/order/create";

	double priceStep = 0.01;
	double qtyStep = 0.01;

	m_p->findPricePrecisionsForInstrument(order.m_category, order.m_symbol, priceStep, qtyStep);

	order.m_priceStep = priceStep;
	order.m_qtyStep = qtyStep;

	const auto response = P::checkResponse(m_p->m_httpSession->post(path, order.toJson()));
	return handleBybitResponse<OrderId>(response);
}

std::vector<OrderResponse> RESTClient::getOpenOrders(const Category category, const std::string &symbol) const {
	const std::string path = "/v5/order/realtime";
	std::map<std::string, std::string> parameters;
	parameters.insert_or_assign("category", magic_enum::enum_name(category));
	parameters.insert_or_assign("symbol", symbol);

	const auto response = P::checkResponse(m_p->m_httpSession->get(path, parameters));
	return handleBybitResponse<OrdersResponse>(response).m_orders;
}

std::optional<OrderResponse>
RESTClient::getOpenOrder(const Category category,
                         const std::string &symbol,
                         const std::string &orderId,
                         const std::string &orderLinkId) const {
	const std::string path = "/v5/order/realtime";
	std::map<std::string, std::string> parameters;
	parameters.insert_or_assign("category", magic_enum::enum_name(category));
	parameters.insert_or_assign("symbol", symbol);
	parameters.insert_or_assign("orderId", orderId);
	parameters.insert_or_assign("orderLinkId", orderLinkId);

	if (const auto response = P::checkResponse(m_p->m_httpSession->get(path, parameters)); !handleBybitResponse<
		OrdersResponse>(response).m_orders.empty()) {
		return handleBybitResponse<OrdersResponse>(response).m_orders.front();
	}

	return {};
}

std::vector<OrderId> RESTClient::cancelAllOrders(Category category, const std::string &symbol) const {
	std::vector<OrderId> retVal;
	std::string path = "/v5/order/cancel-all";
	std::map<std::string, std::string> parameters;

	parameters.insert_or_assign("category", magic_enum::enum_name(category));

	if (!symbol.empty()) {
		parameters.insert_or_assign("symbol", symbol);
	}

	const auto response = P::checkResponse(m_p->m_httpSession->post(path, nlohmann::json(parameters)));

	for (const auto result = handleBybitResponse<Response>(response).m_result; const auto &el: result["list"].
	     items()) {
		OrderId orderId;
		orderId.m_result = el.value();
		orderId.fromJson({});
		retVal.push_back(orderId);
	}

	return retVal;
}

OrderId RESTClient::cancelOrder(const Category category,
                                const std::string &symbol,
                                const std::string &orderId,
                                const std::string &orderLinkId) const {
	const std::string path = "/v5/order/cancel";
	std::map<std::string, std::string> parameters;
	parameters.insert_or_assign("symbol", symbol);
	parameters.insert_or_assign("category", magic_enum::enum_name(category));

	if (!orderId.empty()) {
		parameters.insert_or_assign("orderId", orderId);
	}

	if (!orderLinkId.empty()) {
		parameters.insert_or_assign("orderLinkId", orderId);
	}

	const auto response = P::checkResponse(m_p->m_httpSession->post(path, nlohmann::json(parameters)));
	return handleBybitResponse<OrderId>(response);
}

void RESTClient::setInstruments(const std::vector<Instrument> &instruments) const {
	m_p->setInstruments(instruments);
}

void RESTClient::closeAllPositions(const Category category) const {
	for (const auto positions = getPositionInfo(category); const auto &position: positions) {
		if (!position.m_zeroSize) {
			Order order;
			order.m_symbol = position.m_symbol;

			if (position.m_side == Side::Buy) {
				order.m_side = Side::Sell;
			} else {
				order.m_side = Side::Buy;
			}

			order.m_orderType = OrderType::Market;
			order.m_qty = position.m_size;
			order.m_timeInForce = TimeInForce::GTC;
			auto orderResponse = placeOrder(order);
		}
	}
}

std::vector<FundingRate>
RESTClient::getFundingRates(const Category category,
                            const std::string &symbol,
                            const int64_t startTime,
                            int64_t endTime,
                            const std::int32_t limit) const {
	std::vector<FundingRate> retVal;
	std::vector<FundingRate> fr;

	if (startTime < endTime) {
		fr = m_p->getFundingRates(category, symbol, startTime, endTime, limit);
	}

	while (!fr.empty()) {
		retVal.insert(retVal.end(), fr.begin(), fr.end());
		endTime = fr.back().m_fundingRateTimestamp - 1;
		fr.clear();

		if (startTime < endTime) {
			fr = m_p->getFundingRates(category, symbol, startTime, endTime, limit);
		}
	}

	std::ranges::reverse(retVal);
	return retVal;
}

Tickers RESTClient::getTickers(const Category category, const std::string &symbol) const {
	const std::string path = "/v5/market/tickers";

	std::map<std::string, std::string> parameters;
	parameters.insert_or_assign("category", magic_enum::enum_name(category));
	parameters.insert_or_assign("symbol", symbol);

	const auto response = P::checkResponse(m_p->m_httpSession->get(path, parameters));
	return handleBybitResponse<Tickers>(response);
}
}
