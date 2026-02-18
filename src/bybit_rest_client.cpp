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
#include <spdlog/spdlog.h>
#include <deque>

namespace vk::bybit {
template<typename ValueType>
ValueType handleBybitResponse(const http::response<http::string_body> &response) {
	ValueType retVal;
	retVal.fromJson(nlohmann::json::parse(response.body()));

	if (retVal.retCode != 0) {
		throw std::runtime_error(
			fmt::format("Bybit API error, code: {}, msg: {}", retVal.retCode, retVal.retMsg).c_str());
	}

	return retVal;
}

struct RateLimiter {
	std::mutex mutex;
	int remaining = 50;
	std::int64_t resetTime = 0;
    
    // Local fallback mechanism
    bool serverHeadersFound = false;
    std::deque<std::int64_t> requestTimes; // For local sliding window
    const size_t localLimit = 10;           // 10 requests per second (conservative, Bybit doesn't send rate headers for public endpoints)
    const std::int64_t windowSizeMs = 1000;

	void update(const http::response<http::string_body> &response) {
		std::lock_guard lock(mutex);
		try {
			// Headers are case-insensitive in Boost.Beast
			const auto itStatus = response.find("X-Bapi-Limit-Status");

			if (const auto itReset = response.find("X-Bapi-Limit-Reset"); itStatus != response.end() && itReset != response.end()) {
                remaining = std::stoi(std::string(itStatus->value()));
                resetTime = std::stoll(std::string(itReset->value()));
                serverHeadersFound = true; // Switch to server-side mode
            }
			
			// Log for debugging
#ifdef VERBOSE_LOG
			spdlog::debug(fmt::format("RateLimit: Remaining={}, ResetTime={}, LocalMode={}", remaining, resetTime, !serverHeadersFound));
#endif
		} catch (const std::exception& e) {
			spdlog::warn(fmt::format("Failed to parse rate limit headers: {}", e.what()));
		}
	}

	void wait() {
		std::unique_lock lock(mutex);
        const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        if (serverHeadersFound) {
            // Server-side logic
            if (remaining <= 2) {
                if (resetTime > now) {
                    const auto waitTime = resetTime - now + 50; // +50ms buffer
#ifdef VERBOSE_LOG
                    spdlog::info(fmt::format("Rate limit reached (Server). Waiting for {} ms", waitTime));
#endif
                    std::this_thread::sleep_for(std::chrono::milliseconds(waitTime));
                }
            }
        } else {
            // Local fallback logic (Sliding Window)
            // Remove old requests
            while (!requestTimes.empty() && now - requestTimes.front() > windowSizeMs) {
                requestTimes.pop_front();
            }

            if (requestTimes.size() >= localLimit) {
                // Wait until the oldest request expires
                const auto oldest = requestTimes.front();

                if (const auto waitTime = (oldest + windowSizeMs) - now + 10; waitTime > 0) {
#ifdef VERBOSE_LOG
                    spdlog::info(fmt::format("Rate limit reached (Local). Waiting for {} ms", waitTime));
#endif
                    std::this_thread::sleep_for(std::chrono::milliseconds(waitTime));
                    
                    // After sleeping, we must remove the expired request and add current one
                    // Update now after sleep
                     const auto nowAfterWait = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count();
                     while (!requestTimes.empty() && nowAfterWait - requestTimes.front() > windowSizeMs) {
                        requestTimes.pop_front();
                    }
                }
            }
            requestTimes.push_back(now);
        }
	}
};

struct RESTClient::P {
private:
	Instruments m_instruments;
	mutable std::recursive_mutex m_locker;
    
public:
	RESTClient *parent = nullptr;
	std::shared_ptr<HTTPSession> httpSession;
	mutable RateLimiter rateLimiter; // Add RateLimiter

	explicit P(RESTClient *parent) {
		this->parent = parent;
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
		m_instruments.instruments = instruments;
	}

	bool findPricePrecisionsForInstrument(const Category category,
	                                      const std::string &symbol,
	                                      double &priceStep,
	                                      double &qtyStep) const {
		for (const auto symbols = parent->getInstrumentsInfo(category); const auto &symbolEl: symbols) {
			if (symbolEl.symbol == symbol) {
				priceStep = symbolEl.priceFilter.tickSize;
				qtyStep = symbolEl.lotSizeFilter.qtyStep;
				return true;
			}
		}
		return false;
	}

	http::response<http::string_body> checkResponse(const http::response<http::string_body> &response) const {
        // Update rate limiter with headers from response
        rateLimiter.update(response);

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
        
        // Wait if rate limited
        rateLimiter.wait();

		const auto response = checkResponse(httpSession->get(path, parameters));
		return handleBybitResponse<Candles>(response).candles;
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

        // Wait if rate limited
        rateLimiter.wait();

		const auto response = checkResponse(httpSession->get(path, parameters));
		return handleBybitResponse<FundingRates>(response).fundingRates;
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
        
        // Wait if rate limited
        rateLimiter.wait();

		const auto response = checkResponse(httpSession->get(path, parameters));
		return handleBybitResponse<Instruments>(response);
	}
};

RESTClient::RESTClient(const std::string &apiKey, const std::string &apiSecret) : m_p(
	std::make_unique<P>(this)) {
	m_p->httpSession = std::make_shared<HTTPSession>(apiKey, apiSecret);
}

RESTClient::~RESTClient() = default;

void RESTClient::setCredentials(const std::string &apiKey, const std::string &apiSecret) const {
	m_p->httpSession.reset();
	m_p->httpSession = std::make_shared<HTTPSession>(apiKey, apiSecret);
}

std::vector<Candle>
RESTClient::getHistoricalPrices(const Category category,
                                const std::string &symbol,
                                const CandleInterval interval,
                                std::int64_t from,
                                const std::int64_t to,
                                const std::int32_t limit,
                                const onCandlesDownloaded &writer) const {
	std::vector<Candle> retVal;
	std::vector<Candle> candles = m_p->getHistoricalPrices(category, symbol, interval, from, limit);

	while (!candles.empty()) {

		std::ranges::reverse(candles);

		if ((candles.back().startTime - to) < Bybit::numberOfMsForCandleInterval(interval)) {
			candles.pop_back();
		}

		if (candles.empty()) {
			break;
		}

		const auto first = candles.front();
		const auto last = candles.back();


		if (to < last.startTime) {
			for (const auto &candle: candles) {
				if (candle.startTime <= to) {
					retVal.push_back(candle);
				}
			}
			if (writer) {
				writer(candles);
			}
			break;
		}

		retVal.insert(retVal.end(), candles.begin(), candles.end());
		from = last.startTime + Bybit::numberOfMsForCandleInterval(interval);

		if (writer) {
			writer(candles);
		}
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

    m_p->rateLimiter.wait();
	const auto response = m_p->checkResponse(m_p->httpSession->get(path, parameters));
	return handleBybitResponse<WalletBalance>(response);
}

std::int64_t RESTClient::getServerTime() const {
	const std::string path = "/v5/market/time";
	const std::map<std::string, std::string> parameters;

    m_p->rateLimiter.wait();
	const auto response = m_p->checkResponse(m_p->httpSession->get(path, parameters));
	const auto timeResponse = handleBybitResponse<ServerTime>(response);

	return timeResponse.timeNano / 1000000;
}

std::vector<Position> RESTClient::getPositionInfo(const Category category, const std::string &symbol) const {
	const std::string path = "/v5/position/list";
	std::map<std::string, std::string> parameters;

	parameters.insert_or_assign("category", magic_enum::enum_name(category));

	if (!symbol.empty()) {
		parameters.insert_or_assign("symbol", symbol);
	}

    m_p->rateLimiter.wait();
	const auto response = m_p->checkResponse(m_p->httpSession->get(path, parameters));
	return handleBybitResponse<Positions>(response).positions;
}

std::vector<Instrument>
RESTClient::getInstrumentsInfo(const Category category, const std::string &symbol, const bool force) const {
	if (m_p->getInstruments().instruments.empty() || force) {
		Instruments instr;
		std::vector<Instrument> temp;

		do {
			instr = m_p->getInstrumentsInfo(category, symbol, instr.nextPageCursor);

			for (const auto &instrument: instr.instruments) {
				temp.push_back(instrument);
			}
		} while (!instr.nextPageCursor.empty());

		m_p->setInstruments(temp);
	}

	return m_p->getInstruments().instruments;
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

    m_p->rateLimiter.wait();
	const auto response = m_p->checkResponse(m_p->httpSession->post(path, payload));

	try {
		return handleBybitResponse<Response>(response).retMsg == "OK";
	} catch (std::exception &) {
		Response resp;
		resp.fromJson(nlohmann::json::parse(response.body()));

		if (resp.retMsg == "Position mode is not modified") {
			return true;
		}
	}

	return false;
}

OrderId RESTClient::placeOrder(Order &order) const {
	const std::string path = "/v5/order/create";

	double priceStep = 0.01;
	double qtyStep = 0.01;

	m_p->findPricePrecisionsForInstrument(order.category, order.symbol, priceStep, qtyStep);

	order.priceStep = priceStep;
	order.qtyStep = qtyStep;

    m_p->rateLimiter.wait();
	const auto response = m_p->checkResponse(m_p->httpSession->post(path, order.toJson()));
	return handleBybitResponse<OrderId>(response);
}

std::vector<OrderResponse> RESTClient::getOpenOrders(const Category category, const std::string &symbol) const {
	const std::string path = "/v5/order/realtime";
	std::map<std::string, std::string> parameters;
	parameters.insert_or_assign("category", magic_enum::enum_name(category));
	parameters.insert_or_assign("symbol", symbol);

    m_p->rateLimiter.wait();
	const auto response = m_p->checkResponse(m_p->httpSession->get(path, parameters));
	return handleBybitResponse<OrdersResponse>(response).orders;
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

    m_p->rateLimiter.wait();
	if (const auto response = m_p->checkResponse(m_p->httpSession->get(path, parameters)); !handleBybitResponse<
		OrdersResponse>(response).orders.empty()) {
		return handleBybitResponse<OrdersResponse>(response).orders.front();
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

    m_p->rateLimiter.wait();
	const auto response = m_p->checkResponse(m_p->httpSession->post(path, nlohmann::json(parameters)));

	for (const auto res = handleBybitResponse<Response>(response).result; const auto &el: res["list"].
	     items()) {
		OrderId oid;
		oid.result = el.value();
		oid.fromJson({});
		retVal.push_back(oid);
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

    m_p->rateLimiter.wait();
	const auto response = m_p->checkResponse(m_p->httpSession->post(path, nlohmann::json(parameters)));
	return handleBybitResponse<OrderId>(response);
}

void RESTClient::setInstruments(const std::vector<Instrument> &instruments) const {
	m_p->setInstruments(instruments);
}

void RESTClient::closeAllPositions(const Category category) const {
	for (const auto positionList = getPositionInfo(category); const auto &pos: positionList) {
		if (!pos.zeroSize) {
			Order ord;
			ord.symbol = pos.symbol;

			if (pos.side == Side::Buy) {
				ord.side = Side::Sell;
			} else {
				ord.side = Side::Buy;
			}

			ord.orderType = OrderType::Market;
			ord.qty = pos.size;
			ord.timeInForce = TimeInForce::GTC;
			auto orderResponse = placeOrder(ord);
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
		endTime = fr.back().fundingRateTimestamp - 1;
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

    m_p->rateLimiter.wait();
	const auto response = m_p->checkResponse(m_p->httpSession->get(path, parameters));
	return handleBybitResponse<Tickers>(response);
}
}
