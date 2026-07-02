#include "stonky/bybit/bybit.h"
#include "stonky/bybit/bybit_rest_client.h"
#include "stonky/bybit/bybit_ws_stream_manager.h"
#include "stonky/bybit/bybit_ws_private_stream_manager.h"
#include "stonky/utils/json_utils.h"
#include "stonky/utils/log_utils.h"
#include "stonky/utils/utils.h"
#include <memory>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <thread>
#include <spdlog/spdlog.h>
#include <future>
#include <boost/multiprecision/cpp_dec_float.hpp>

using namespace stonky::bybit;
using namespace std::chrono_literals;

constexpr int HISTORY_LENGTH_IN_S = 86400; // 1 day

void logFunction(const stonky::LogSeverity severity, const std::string& errmsg) {
    switch (severity) {
    case stonky::LogSeverity::Info:
        spdlog::info(errmsg);
        break;
    case stonky::LogSeverity::Warning:
        spdlog::warn(errmsg);
        break;
    case stonky::LogSeverity::Critical:
        spdlog::critical(errmsg);
        break;
    case stonky::LogSeverity::Error:
        spdlog::error(errmsg);
        break;
    case stonky::LogSeverity::Debug:
        spdlog::debug(errmsg);
        break;
    case stonky::LogSeverity::Trace:
        spdlog::trace(errmsg);
        break;
    }
}

/**
 * Parse a KEY=VALUE .env file. Skips blank lines and # comments, tolerates an
 * optional "export " prefix, trims whitespace and surrounding quotes.
 */
std::map<std::string, std::string> readEnvFile(const std::filesystem::path& path) {
    std::map<std::string, std::string> env;
    std::ifstream ifs(path.string());

    if (!ifs.is_open()) {
        std::cerr << "Couldn't open env file: " + path.string() << std::endl;
        return env;
    }

    const auto trim = [](std::string s) {
        s.erase(0, s.find_first_not_of(" \t\r\n"));
        s.erase(s.find_last_not_of(" \t\r\n") + 1);

        if (s.size() >= 2 && ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\''))) {
            s = s.substr(1, s.size() - 2);
        }

        return s;
    };

    std::string line;

    while (std::getline(ifs, line)) {
        line = trim(line);

        if (line.empty() || line.front() == '#') {
            continue;
        }

        if (line.starts_with("export ")) {
            line = line.substr(7);
        }

        const auto pos = line.find('=');

        if (pos == std::string::npos) {
            continue;
        }

        env[trim(line.substr(0, pos))] = trim(line.substr(pos + 1));
    }

    return env;
}

/**
 * Bybit Demo account credentials (~/.config/crypto-portfolio/bybit/demo.env,
 * keys API_KEY / API_SECRET). Pair with Environment::Demo endpoints.
 */
std::pair<std::string, std::string> readCredentials() {
    const auto* home = std::getenv("HOME");
    const std::filesystem::path envPath = std::filesystem::path(home ? home : "") / ".config/crypto-portfolio/bybit/demo.env";

    auto env = readEnvFile(envPath);
    return {env["API_KEY"], env["API_SECRET"]};
}

bool checkCandles(const std::vector<Candle>& candles, const CandleInterval interval) {
    const auto secs = Bybit::numberOfMsForCandleInterval(interval);

    if (candles.empty()) {
        return false;
    }

    for (auto i = 0; i < candles.size() - 1; i++) {
        if (const auto timeDiff = candles[i + 1].startTime - candles[i].startTime; timeDiff != secs) {
            return false;
        }
    }

    return true;
}

void testHistory() {
    try {
        const auto [fst, snd] = readCredentials();
        const auto restClient = std::make_unique<RESTClient>(
            fst, snd);

        const auto from = std::chrono::seconds(std::time(nullptr)).count() - HISTORY_LENGTH_IN_S;
        const auto to = from + 4 * 60 * 60;

        if (const auto candles = restClient->getHistoricalPrices(Category::linear, "BTCUSDT", CandleInterval::_1,
                                                                 from * 1000, to * 1000); checkCandles(
            candles, CandleInterval::_1)) {
            logFunction(stonky::LogSeverity::Info, "Candles OK");
        }
        else {
            logFunction(stonky::LogSeverity::Error, "Candles Not OK");
        }
    }
    catch (std::exception& e) {
        logFunction(stonky::LogSeverity::Critical, e.what());
    }
}

void measureRestResponses() {
    const auto [fst, snd] = readCredentials();
    const auto restClient = std::make_shared<RESTClient>(fst, snd);

    using std::chrono::high_resolution_clock;
    using std::chrono::duration_cast;
    using std::chrono::duration;
    using std::chrono::milliseconds;

    double overallTime = 0.0;
    int numPass = 0;

    while (true) {
        try {
            auto t1 = high_resolution_clock::now();
            auto pr = restClient->getWalletBalance(AccountType::UNIFIED, "USDT");
            auto t2 = high_resolution_clock::now();

            duration<double, std::milli> ms_double = t2 - t1;
            logFunction(stonky::LogSeverity::Info,
                        fmt::format("Get Wallet Balance request time: {} ms", ms_double.count()));
            overallTime += ms_double.count();

            t1 = high_resolution_clock::now();
            auto ex = restClient->getInstrumentsInfo(Category::linear, "", true);
            t2 = high_resolution_clock::now();

            ms_double = t2 - t1;
            logFunction(stonky::LogSeverity::Info, fmt::format("Get symbols request time: {} ms", ms_double.count()));
            overallTime += ms_double.count();

            t1 = high_resolution_clock::now();
            const auto account = restClient->getPositionInfo(Category::linear, "BTCUSDT");
            t2 = high_resolution_clock::now();

            ms_double = t2 - t1;
            logFunction(stonky::LogSeverity::Info,
                        fmt::format("Get position info request time: {} ms\n", ms_double.count()));
            overallTime += ms_double.count();
            numPass++;

            double timePerResponse = overallTime / (numPass * 3);
            logFunction(stonky::LogSeverity::Info, fmt::format("Average time per response: {} ms\n", timePerResponse));
        }
        catch (std::exception& e) {
            logFunction(stonky::LogSeverity::Warning, fmt::format("Exception: {}", e.what()));
        }

        std::this_thread::sleep_for(2s);
    }
}

double round_to(const double value, const double precision = 1.0) {
    return std::round(value / precision) * precision;
}

void replaceAll(std::string& s, const std::string& search, const std::string& replace) {
    for (size_t pos = 0;; pos += replace.length()) {
        pos = s.find(search, pos);

        if (pos == std::string::npos)
            break;

        s.erase(pos, search.length());
        s.insert(pos, replace);
    }
}

void positions() {
    const auto [fst, snd] = readCredentials();
    const auto restClient = std::make_shared<RESTClient>(fst, snd);

    try {
        for (const auto& position : restClient->getPositionInfo(Category::linear, "BTCUSDT")) {
            if (position.size != 0) {
                const auto ts = stonky::getMsTimestamp(stonky::currentTime()).count();
                Order order;
                order.symbol = position.symbol;

                if (position.side == Side::Buy) {
                    order.side = Side::Sell;
                }
                else {
                    order.side = Side::Buy;
                }

                order.orderType = OrderType::Market;
                order.qty = position.size;
                order.timeInForce = TimeInForce::GTC;
                order.orderLinkId = std::to_string(ts);
                order.positionIdx = position.positionIdx;
                const auto id = restClient->placeOrder(order);
                logFunction(stonky::LogSeverity::Info, fmt::format("Order placed, id: {}", id.orderId));
            }
        }
    }
    catch (std::exception& e) {
        logFunction(stonky::LogSeverity::Warning, fmt::format("Exception: {}", e.what()));
    }
}

void testOrders() {
    const auto [fst, snd] = readCredentials();
    auto restClient = std::make_shared<RESTClient>(fst, snd);

    try {
        const auto ts = stonky::getMsTimestamp(stonky::currentTime()).count();

        double lotAmount = 0.1;
        constexpr int amount = -25;

        Order order;
        order.symbol = "DOTUSDT";
        order.side = Side::Buy;
        order.orderType = OrderType::Market;
        order.qty = lotAmount * std::abs(amount);
        order.timeInForce = TimeInForce::GTC;
        order.orderLinkId = std::to_string(ts);

        auto orderResponse = restClient->placeOrder(order);
        logFunction(stonky::LogSeverity::Info, fmt::format("Order Id: {}", orderResponse.orderId));
    }
    catch (std::exception& e) {
        logFunction(stonky::LogSeverity::Warning, fmt::format("Exception: {}", e.what()));
    }
}

void setPositionMode() {
    const auto [fst, snd] = readCredentials();

    if (const auto restClient = std::make_shared<RESTClient>(fst, snd); restClient->setPositionMode(Category::linear, "", "USDT", PositionMode::MergedSingle)) {
        logFunction(stonky::LogSeverity::Info, "Position mode set successfully");
    }
    else {
        logFunction(stonky::LogSeverity::Info, "Failed to set position mode");
    }
}

void testWebsockets() {
    const std::shared_ptr wsManager = std::make_unique<WSStreamManager>();
    wsManager->setLoggerCallback(&logFunction);

    wsManager->subscribeTickerStream("BTCUSDT");
    wsManager->subscribeCandlestickStream("BTCUSDT", CandleInterval::_1);

    while (true) {
        {
            if (const auto ret = wsManager->readEventTicker("BTCUSDT")) {
                std::cout << "BTC price: " << ret->lastPrice << std::endl;
            }
            else {
                std::cout << "Error" << std::endl;
            }
        }

        {
            if (const auto ret = wsManager->readEventCandlestick("BTCUSDT", CandleInterval::_1)) {
                std::cout << "BTC open price: " << ret->open << std::endl;
            }
            else {
                std::cout << "Error" << std::endl;
            }
        }
        std::this_thread::sleep_for(1000ms);
    }
}

/**
 * Live smoke test of the private WS stream against the DEMO environment
 * (credentials from ~/.config/crypto-portfolio/bybit/demo.env). Places a
 * post_only BTCUSDT limit buy ~20 % below the market (never fills), amends
 * its price, cancels it, and prints every order-topic event observed along
 * the way. Expected sequence: New → New (amended price, new updatedTime)
 * → Cancelled.
 */
void testPrivateWebsockets() {
    const auto [fst, snd] = readCredentials();

    const std::shared_ptr wsPrivateManager = std::make_shared<WSPrivateStreamManager>(fst, snd, Environment::Demo);
    wsPrivateManager->setLoggerCallback(&logFunction);

    wsPrivateManager->setOrderUpdateCallback([](const EventOrderUpdate& e) {
        std::cout << "ORDER: " << e.symbol << " " << magic_enum::enum_name(e.orderStatus) << " px=" << e.price << " cumExecQty=" << e.cumExecQty << " reject="
                  << e.rejectReason << " updated=" << e.updatedTime << std::endl;
    });

    wsPrivateManager->setExecutionCallback([](const EventExecution& e) {
        std::cout << "EXEC: " << e.symbol << " " << magic_enum::enum_name(e.execType) << " qty=" << e.execQty << " px=" << e.execPrice << " maker=" << e.isMaker
                  << " execId=" << e.execId << std::endl;
    });

    wsPrivateManager->connect();
    std::this_thread::sleep_for(3000ms);

    try {
        const auto restClient = std::make_shared<RESTClient>(fst, snd, Environment::Demo);

        /// Adapt to the account's CURRENT position mode instead of switching it
        /// (Bybit refuses switch-mode while any position/order is open). Hedge
        /// mode lists idx=1/2 entries; one-way lists a single idx=0 entry. The
        /// production executor will require one-way (net positions).
        std::int64_t positionIdx = 0;

        for (const auto positions = restClient->getPositionInfo(Category::linear, "BTCUSDT"); const auto& pos: positions) {
            logFunction(stonky::LogSeverity::Info, fmt::format("position: {} idx={} size={}", pos.symbol, pos.positionIdx, pos.size));

            if (pos.positionIdx != 0) {
                positionIdx = 1; /// hedge mode: Buy leg = idx 1
            }
        }

        logFunction(stonky::LogSeverity::Info, fmt::format("account mode: {}, test order positionIdx={}", positionIdx == 0 ? "one-way" : "hedge", positionIdx));

        const auto tickers = restClient->getTickers(Category::linear, "BTCUSDT");
        logFunction(stonky::LogSeverity::Info,
                    fmt::format("tickers: {}, bid1={}, last={}", tickers.tickers.size(), tickers.tickers.empty() ? 0.0 : tickers.tickers[0].bid1Price,
                                tickers.tickers.empty() ? 0.0 : tickers.tickers[0].lastPrice));
        const auto limitPrice = tickers.tickers[0].bid1Price * 0.8;

        Order order;
        order.category = Category::linear;
        order.symbol = "BTCUSDT";
        order.side = Side::Buy;
        order.orderType = OrderType::Limit;
        order.timeInForce = TimeInForce::PostOnly;
        order.qty = 0.001;
        order.price = limitPrice;
        order.positionIdx = positionIdx;

        const auto orderId = restClient->placeOrder(order);
        logFunction(stonky::LogSeverity::Info, fmt::format("placed: {}", orderId.orderId));
        std::this_thread::sleep_for(2000ms);

        const auto amendId = restClient->amendOrder(Category::linear, "BTCUSDT", orderId.orderId, "", limitPrice * 1.01);
        logFunction(stonky::LogSeverity::Info, fmt::format("amended: {}", amendId.orderId));
        std::this_thread::sleep_for(2000ms);

        const auto cancelId = restClient->cancelOrder(Category::linear, "BTCUSDT", orderId.orderId);
        logFunction(stonky::LogSeverity::Info, fmt::format("cancelled: {}", cancelId.orderId));
    }
    catch (std::exception& e) {
        logFunction(stonky::LogSeverity::Critical, e.what());
    }

    /// Keep the stream open to observe the trailing events (and reconnect
    /// behavior — try dropping the network here).
    std::this_thread::sleep_for(30000ms);
}

void testTickers() {
    const auto [fst, snd] = readCredentials();
    const auto restClient = std::make_shared<RESTClient>(fst, snd);

    try {
        const auto tme = restClient->getServerTime();
        auto response = restClient->getTickers(Category::linear, "BTCUSDT");
        logFunction(stonky::LogSeverity::Info, fmt::format("Ticker: {}, fr: {}", response.tickers[0].symbol, response.tickers[0].fundingRate));
    }
    catch (std::exception& e) {
        logFunction(stonky::LogSeverity::Warning, fmt::format("Exception: {}", e.what()));
    }
}

int main() {
    // measureRestResponses();
    // testWebsockets();
    // setPositionMode();
    // positions();
    // testOrders();
    // testTickers();
    // testHistory();
    testPrivateWebsockets();
    return getchar();
}
