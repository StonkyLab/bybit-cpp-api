/**
Bybit Futures REST Client

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.
*/

#include "vk/bybit/bybit_rest_client.h"
#include "vk/bybit/bybit_http_session.h"
#include "vk/bybit/bybit.h"
#include "vk/utils/utils.h"
#include <mutex>
#include <thread>
#include <chrono>
#include <spdlog/spdlog.h>
#include <deque>
#include <regex>
#include <set>
#include <zlib.h>

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
	mutable std::shared_ptr<HTTPSession> publicHttpSession;
	mutable RateLimiter rateLimiter;

	explicit P(RESTClient *parent) {
		this->parent = parent;
	}

	// Decompress a gzip-compressed byte string using zlib.
	static std::string decompressGzip(const std::string &compressed) {
		z_stream zs{};
		if (inflateInit2(&zs, 16 + MAX_WBITS) != Z_OK) {
			throw std::runtime_error("inflateInit2 failed");
		}
		zs.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(compressed.data()));
		zs.avail_in = static_cast<uInt>(compressed.size());

		std::string result;
		char outbuf[32768];
		int ret;
		do {
			zs.next_out = reinterpret_cast<Bytef *>(outbuf);
			zs.avail_out = sizeof(outbuf);
			ret = inflate(&zs, Z_NO_FLUSH);
			if (ret == Z_STREAM_ERROR || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
				inflateEnd(&zs);
				throw std::runtime_error("inflate failed: " + std::to_string(ret));
			}
			result.append(outbuf, sizeof(outbuf) - zs.avail_out);
		} while (ret != Z_STREAM_END);
		inflateEnd(&zs);
		return result;
	}

	// Returns the timestamp (ms) from the last non-empty line of a Bybit trade CSV.
	// Format: id,timestamp_ms,price,volume,side,flag
	// The timestamp is the SECOND comma-separated field (index 1).
	static int64_t lastCsvTimestamp(const std::string &csv) {
		size_t end = csv.size();
		while (end > 0 && (csv[end - 1] == '\n' || csv[end - 1] == '\r')) {
			--end;
		}
		if (end == 0) {
			throw std::runtime_error("CSV data is empty");
		}
		const size_t lineStart = csv.rfind('\n', end - 1);
		const size_t start = (lineStart == std::string::npos) ? 0 : lineStart + 1;
		const std::string lastLine = csv.substr(start, end - start);
		// Skip the first field (trade ID) and parse the second field (timestamp ms)
		const size_t comma1 = lastLine.find(',');
		if (comma1 == std::string::npos) {
			throw std::runtime_error("Unexpected CSV format: no comma in last line");
		}
		const size_t comma2 = lastLine.find(',', comma1 + 1);
		const std::string tsStr = lastLine.substr(comma1 + 1,
			comma2 == std::string::npos ? std::string::npos : comma2 - comma1 - 1);
		return std::stoll(tsStr);
	}

	// Fetches the actual last candle timestamp for a delisted spot symbol by downloading and
	// decompressing the latest daily gz file from public.bybit.com/spot/SYMBOL/.
	// Returns the timestamp (ms) of the last record, or 0 on failure.
	[[nodiscard]] int64_t fetchLastTimestampForDelistedSymbol(const std::string &symbol) const {
		constexpr int maxRetries = 3;
		for (int attempt = 0; attempt < maxRetries; ++attempt) {
			try {
				const auto dirResponse = publicHttpSession->get("/spot/" + symbol + "/", {});
				const std::string &body = dirResponse.body();
				// Files are named e.g. VRAUSDT_2026-02-18.csv.gz
				const std::regex fileRegex(symbol + R"re(_(\d{4}-\d{2}-\d{2})\.csv\.gz)re");
				std::string latestDate;
				std::string latestFilename;
				auto it = std::sregex_iterator(body.begin(), body.end(), fileRegex);
				for (; it != std::sregex_iterator(); ++it) {
					const std::string date = (*it)[1].str();
					if (date > latestDate) {
						latestDate = date;
						latestFilename = (*it)[0].str();
					}
				}
				if (latestFilename.empty()) {
					return 0;
				}
				const auto fileResponse = publicHttpSession->get(
					"/spot/" + symbol + "/" + latestFilename, {});
				const std::string decompressed = decompressGzip(fileResponse.body());
				return lastCsvTimestamp(decompressed);
			} catch (const std::exception &e) {
				if (attempt < maxRetries - 1) {
					spdlog::warn(fmt::format(
						"Failed to fetch last timestamp for delisted symbol {} (attempt {}/{}): {}",
						symbol, attempt + 1, maxRetries, e.what()));
					publicHttpSession = std::make_shared<HTTPSession>("", "", "public.bybit.com");
					std::this_thread::sleep_for(std::chrono::seconds(1 << attempt));
				} else {
					spdlog::warn(fmt::format("Failed to fetch last timestamp for delisted symbol {}: {}",
					                        symbol, e.what()));
				}
			}
		}
		return 0;
	}

	[[nodiscard]] std::vector<std::string> fetchPublicSpotSymbols() const {
		constexpr int maxRetries = 3;
		for (int attempt = 0; attempt < maxRetries; ++attempt) {
			try {
				const auto response = publicHttpSession->get("/spot/", {});
				const std::string &body = response.body();
				std::vector<std::string> symbols;
				const std::regex linkRegex(R"re(href="([A-Z0-9]+)")re");
				auto it = std::sregex_iterator(body.begin(), body.end(), linkRegex);
				for (; it != std::sregex_iterator(); ++it) {
					symbols.push_back((*it)[1].str());
				}
				return symbols;
			} catch (const std::exception &e) {
				if (attempt < maxRetries - 1) {
					spdlog::warn(fmt::format("Failed to fetch public spot symbols (attempt {}/{}): {}, retrying...",
					                        attempt + 1, maxRetries, e.what()));
					publicHttpSession = std::make_shared<HTTPSession>("", "", "public.bybit.com");
					std::this_thread::sleep_for(std::chrono::seconds(1 << attempt));
				} else {
					spdlog::warn(fmt::format("Failed to fetch public spot symbols: {}", e.what()));
				}
			}
		}
		return {};
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
	                               const std::string &cursor,
	                               const std::string &status = "") const {
		const std::string path = "/v5/market/instruments-info";
		std::map<std::string, std::string> parameters;
		parameters.insert_or_assign("category", magic_enum::enum_name(category));

		if (!symbol.empty()) {
			parameters.insert_or_assign("symbol", symbol);
		}

		if (!cursor.empty()) {
			parameters.insert_or_assign("cursor", cursor);
		}

		if (!status.empty()) {
			parameters.insert_or_assign("status", status);
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
	m_p->publicHttpSession = std::make_shared<HTTPSession>("", "", "public.bybit.com");
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

		// For delisted spot symbols, Bybit ignores the 'start' parameter and always
		// returns its fixed last-N-candles window.  When the batch therefore starts
		// before 'from', we have two cases:
		//  a) The batch is entirely before 'from' — no new data, stop.
		//  b) The batch straddles 'from' — discard the stale prefix and continue.
		if (candles.front().startTime < from) {
			if (candles.back().startTime < from) {
				break; // Entire batch predates 'from'; no new data available.
			}
			// Trim candles that predate 'from'.
			const auto firstValid = std::lower_bound(candles.begin(), candles.end(), from,
				[](const Candle &c, std::int64_t ts) { return c.startTime < ts; });
			candles.erase(candles.begin(), firstValid);
		}

		// Pop the last candle only if its interval overlaps with 'to' — i.e. it is the
		// current in-progress candle for active symbols.  For delisted symbols whose
		// last candle lies far in the past this condition is false, so T_last is kept.
		if (candles.back().startTime + Bybit::numberOfMsForCandleInterval(interval) > to) {
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
RESTClient::getInstrumentsInfo(const Category category, const std::string &symbol, const bool force,
                               const std::string &status) const {
	// Status-filtered requests always fetch fresh and do not touch the cache
	if (!status.empty()) {
		if (category == Category::spot) {
			// For spot: derive delisted symbols as the diff between public.bybit.com and active instruments
			if (m_p->getInstruments().instruments.empty()) {
				Instruments instr;
				std::vector<Instrument> temp;
				do {
					instr = m_p->getInstrumentsInfo(category, symbol, instr.nextPageCursor);
					for (const auto &i: instr.instruments) temp.push_back(i);
				} while (!instr.nextPageCursor.empty());
				m_p->setInstruments(temp);
			}

			std::set<std::string> activeSymbolSet;
			for (const auto &instr: m_p->getInstruments().instruments) {
				activeSymbolSet.insert(instr.symbol);
			}

			std::vector<Instrument> delisted;
			for (const auto &sym: m_p->fetchPublicSpotSymbols()) {
				if (sym.ends_with("USDT") && !activeSymbolSet.contains(sym)) {
					Instrument delistedInstr;
					delistedInstr.symbol = sym;
					delistedInstr.quoteCoin = "USDT";
					delistedInstr.contractStatus = ContractStatus::Closed;
					delisted.push_back(delistedInstr);
				}
			}
			return delisted;
		}

		Instruments instr;
		std::vector<Instrument> temp;

		do {
			instr = m_p->getInstrumentsInfo(category, symbol, instr.nextPageCursor, status);

			for (const auto &instrument: instr.instruments) {
				temp.push_back(instrument);
			}
		} while (!instr.nextPageCursor.empty());

		return temp;
	}

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

std::int64_t RESTClient::fetchLastTimestampForDelistedSpotSymbol(const std::string &symbol) const {
	return m_p->fetchLastTimestampForDelistedSymbol(symbol);
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
