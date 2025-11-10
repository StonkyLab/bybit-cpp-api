#include "vk/bybit/bybit_rest_client.h"
#include "vk/bybit/bybit.h"
#include <iostream>
#include <spdlog/spdlog.h>
#include <fstream>
#include "vk/utils/json_utils.h"
#include "vk/utils/log_utils.h"

using namespace vk::bybit;
using namespace std::chrono_literals;

void logFunction(const vk::LogSeverity severity, const std::string& errmsg) {
    switch (severity) {
    case vk::LogSeverity::Info:
        spdlog::info(errmsg);
        break;
    case vk::LogSeverity::Warning:
        spdlog::warn(errmsg);
        break;
    case vk::LogSeverity::Critical:
        spdlog::critical(errmsg);
        break;
    case vk::LogSeverity::Error:
        spdlog::error(errmsg);
        break;
    case vk::LogSeverity::Debug:
        spdlog::debug(errmsg);
        break;
    case vk::LogSeverity::Trace:
        spdlog::trace(errmsg);
        break;
    }
}

std::pair<std::string, std::string> readCredentials(const char* path) {
    std::filesystem::path pathToCfg{path};
    std::ifstream ifs(pathToCfg.string());

    if (!ifs.is_open()) {
        throw std::runtime_error("Couldn't open config file: " + pathToCfg.string());
    }

    try {
        std::string apiKey;
        std::string apiSecret;

        nlohmann::json json = nlohmann::json::parse(ifs);
        vk::readValue<std::string>(json, "ApiKey", apiKey);
        vk::readValue<std::string>(json, "ApiSecret", apiSecret);

        std::pair retVal(apiKey, apiSecret);
        return retVal;
    }
    catch (const std::exception& e) {
        std::cerr << e.what();
        ifs.close();
    }

    return {};
}

void measureRestResponses(const std::pair<std::string, std::string>& credentials) {
    const auto restClient = std::make_shared<RESTClient>(credentials.first, credentials.second);

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
            logFunction(vk::LogSeverity::Info,
                        fmt::format("Get Wallet Balance request time: {} ms", ms_double.count()));
            overallTime += ms_double.count();

            t1 = high_resolution_clock::now();
            auto ex = restClient->getInstrumentsInfo(Category::linear, "", true);
            t2 = high_resolution_clock::now();

            ms_double = t2 - t1;
            logFunction(vk::LogSeverity::Info, fmt::format("Get symbols request time: {} ms", ms_double.count()));
            overallTime += ms_double.count();

            t1 = high_resolution_clock::now();
            const auto account = restClient->getPositionInfo(Category::linear, "BTCUSDT");
            t2 = high_resolution_clock::now();

            ms_double = t2 - t1;
            logFunction(vk::LogSeverity::Info,
                        fmt::format("Get position info request time: {} ms\n", ms_double.count()));
            overallTime += ms_double.count();
            numPass++;

            double timePerResponse = overallTime / (numPass * 3);
            logFunction(vk::LogSeverity::Info, fmt::format("Average time per response: {} ms\n", timePerResponse));
        }
        catch (std::exception& e) {
            logFunction(vk::LogSeverity::Warning, fmt::format("Exception: {}", e.what()));
        }

        std::this_thread::sleep_for(2s);
    }
}

int main(const int argc, char** argv) {
    if (argc <= 1) {
        spdlog::error("No parameters!");
        return -1;
    }

    try {
        const auto credentials = readCredentials(argv[1]);
        measureRestResponses(credentials);
    }
    catch (const std::exception& e) {
        spdlog::error("Exception: {}", e.what());
    }
}
