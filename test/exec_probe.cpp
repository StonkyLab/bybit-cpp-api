/**
Live probe of RESTClient::getExecutions against the Bybit Demo account —
validates the new /v5/execution/list path (auth, params, EventExecution parse)
used by the gateway's WS-gap fill reconcile. Read-only.
*/

#include "stonky/bybit/bybit_rest_client.h"
#include <spdlog/spdlog.h>
#include <filesystem>
#include <fstream>
#include <map>

using namespace stonky::bybit;

namespace {
std::map<std::string, std::string> readEnvFile(const std::filesystem::path& path) {
    std::map<std::string, std::string> env;
    std::ifstream ifs(path.string());
    std::string line;
    const auto trim = [](std::string s) {
        s.erase(0, s.find_first_not_of(" \t\r\n"));
        s.erase(s.find_last_not_of(" \t\r\n") + 1);
        return s;
    };
    while (std::getline(ifs, line)) {
        line = trim(line);
        if (line.empty() || line.front() == '#') continue;
        if (const auto pos = line.find('='); pos != std::string::npos) env[trim(line.substr(0, pos))] = trim(line.substr(pos + 1));
    }
    return env;
}
} // namespace

int main(int argc, char** argv) {
    spdlog::set_level(spdlog::level::info);
    const auto* home = std::getenv("HOME");
    const auto env = readEnvFile(std::filesystem::path(home ? home : "") / ".config/crypto-portfolio/bybit/demo.env");
    const std::string symbol = argc > 1 ? argv[1] : "XRPUSDT";

    if (!env.contains("API_KEY") || !env.contains("API_SECRET")) {
        spdlog::critical("Missing demo creds");
        return 1;
    }

    try {
        const RESTClient rest(env.at("API_KEY"), env.at("API_SECRET"), Environment::Demo);
        const auto executions = rest.getExecutions(Category::linear, symbol, "");
        spdlog::info("getExecutions({}) returned {} execution(s)", symbol, executions.size());

        int shown = 0;
        for (const auto& ex : executions) {
            if (shown++ >= 5) break;
            spdlog::info("  execId={} linkId={} type={} qty={} price={} maker={}", ex.execId, ex.orderLinkId, magic_enum::enum_name(ex.execType), ex.execQty, ex.execPrice,
                         ex.isMaker);
        }
    } catch (std::exception& e) {
        spdlog::critical("getExecutions failed: {}", e.what());
        return 1;
    }

    return 0;
}
