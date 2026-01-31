# bybit_cpp_api

C++ connector library for Bybit cryptocurrency exchange API v5.

## Features

- REST API client for futures trading (Linear Perpetuals)
- WebSocket client for real-time market data
- Historical candlestick and funding rate data download
- Full trading support (orders, positions, account management)
- Support for both One-Way and Hedge position modes

## Requirements

- C++20 compiler
- CMake 3.20+
- Boost 1.83+ (ASIO, Beast)
- OpenSSL
- nlohmann_json
- spdlog
- magic_enum

## Building

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## Usage Examples

### REST Client - Market Data

```cpp
#include "vk/bybit/bybit_rest_client.h"

using namespace vk::bybit;

// Create client (empty credentials for public endpoints)
RESTClient client("", "");

// Get all instruments info
auto instruments = client.getInstrumentsInfo(Category::Linear);

// Get tickers
auto tickers = client.getTickers(Category::Linear, "BTCUSDT");

// Get server time
auto serverTime = client.getServerTime();
```

### Historical Candlestick Data

```cpp
#include "vk/bybit/bybit_rest_client.h"

using namespace vk::bybit;

RESTClient client("", "");

// Download historical candles with automatic pagination
auto candles = client.getHistoricalPrices(
    Category::Linear,
    "BTCUSDT",
    CandleInterval::_1h,
    fromTimestamp,    // ms
    toTimestamp,      // ms
    200);             // limit per request

std::cout << "Downloaded " << candles.size() << " candles" << std::endl;

// With callback for streaming large datasets
client.getHistoricalPrices(
    Category::Linear,
    "BTCUSDT", 
    CandleInterval::_1m,
    fromTimestamp,
    toTimestamp,
    200,
    [](const std::vector<Candle>& batch) {
        // Process each batch as it arrives
        for (const auto& candle : batch) {
            std::cout << "Time: " << candle.openTime << std::endl;
        }
    });
```

### Funding Rate History

```cpp
auto fundingRates = client.getFundingRates(
    Category::Linear,
    "BTCUSDT",
    startTimestamp,
    endTimestamp);

for (const auto& fr : fundingRates) {
    std::cout << "Time: " << fr.fundingRateTimestamp 
              << " Rate: " << fr.fundingRate << std::endl;
}
```

### Trading Operations (Requires API Keys)

```cpp
#include "vk/bybit/bybit_rest_client.h"

using namespace vk::bybit;

RESTClient client("your_api_key", "your_api_secret");

// Get wallet balance
auto balance = client.getWalletBalance(AccountType::Unified, "USDT");

// Get current positions
auto positions = client.getPositionInfo(Category::Linear, "BTCUSDT");

// Place a market order
Order order;
order.category = Category::Linear;
order.symbol = "BTCUSDT";
order.side = Side::Buy;
order.orderType = OrderType::Market;
order.qty = "0.001";
order.positionIdx = PositionIdx::OneWay;

auto orderId = client.placeOrder(order);

// Cancel order
client.cancelOrder(Category::Linear, "BTCUSDT", orderId.orderId);

// Cancel all orders for symbol
client.cancelAllOrders(Category::Linear, "BTCUSDT");

// Close all positions
client.closeAllPositions(Category::Linear);
```

### Position Mode

```cpp
// Switch to Hedge Mode
client.setPositionMode(
    Category::Linear,
    "BTCUSDT",
    "",                     // coin (use symbol instead)
    PositionMode::BothSide  // Hedge Mode
);

// Switch to One-Way Mode  
client.setPositionMode(
    Category::Linear,
    "BTCUSDT",
    "",
    PositionMode::MergedSingle  // One-Way Mode
);
```

### WebSocket - Real-time Data

```cpp
#include "vk/bybit/bybit_ws_client.h"

using namespace vk::bybit;

WebSocketClient wsClient;

// Subscribe to ticker stream
wsClient.subscribe("tickers.BTCUSDT", [](const Event& event) {
    // Handle real-time ticker updates
    std::cout << "Ticker update received" << std::endl;
});

// Subscribe to kline stream
wsClient.subscribe("kline.1.BTCUSDT", [](const Event& event) {
    // Handle real-time candlestick updates
});
```

## Available Categories

| Category | Description |
|----------|-------------|
| `Category::Spot` | Spot trading |
| `Category::Linear` | USDT perpetual |
| `Category::Inverse` | Inverse perpetual |
| `Category::Option` | Options |

## Candle Intervals

| Interval | Enum Value |
|----------|------------|
| 1 minute | `CandleInterval::_1m` |
| 3 minutes | `CandleInterval::_3m` |
| 5 minutes | `CandleInterval::_5m` |
| 15 minutes | `CandleInterval::_15m` |
| 30 minutes | `CandleInterval::_30m` |
| 1 hour | `CandleInterval::_1h` |
| 2 hours | `CandleInterval::_2h` |
| 4 hours | `CandleInterval::_4h` |
| 1 day | `CandleInterval::_1d` |
| 1 week | `CandleInterval::_1w` |
| 1 month | `CandleInterval::_1M` |

## Project Structure

```
bybit_cpp_api/
├── include/vk/bybit/
│   ├── bybit_rest_client.h       # REST API client
│   ├── bybit_ws_client.h         # WebSocket client
│   ├── bybit_ws_session.h        # WebSocket session (PIMPL)
│   ├── bybit_http_session.h      # HTTP/HTTPS session
│   ├── bybit_models.h            # Data models
│   ├── bybit_enums.h             # Enumerations
│   └── ...
├── src/
│   ├── bybit_rest_client.cpp
│   ├── bybit_ws_client.cpp
│   ├── bybit_http_session.cpp
│   └── ...
├── vk_cpp_common/                # Common utilities submodule
└── test/
    └── main.cpp
```

## API Documentation

Full Bybit API v5 documentation: https://bybit-exchange.github.io/docs/v5/intro

## License

MIT License - see source files for details.

## Author

Vitezslav Kot <vitezslav.kot@gmail.com>