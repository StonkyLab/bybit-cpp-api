/**
Bybit Execution Gateway

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2026 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.
*/

#ifndef BYBIT_EXECUTION_GATEWAY_H
#define BYBIT_EXECUTION_GATEWAY_H

#include <stonky/interface/i_execution_gateway.h>
#include "stonky/bybit/bybit_enums.h"
#include <memory>

namespace stonky::execution {

/**
 * IExecutionGateway adapter over bybit-cpp-api: REST for order ops and
 * instrument metadata, private WS (order+execution topics) for events, public
 * WS ticker stream for top-of-book quotes. Requires the account in one-way
 * position mode (positionIdx=0). Bybit reject reasons are classified into
 * RejectKind here — the chase core never sees venue strings.
 *
 * NOTE (Demo): market data comes from the mainnet public stream even in the
 * Demo environment; only REST and the private stream point at demo hosts.
 */
class BybitExecutionGateway final : public IExecutionGateway {
    struct P;
    std::unique_ptr<P> m_p{};

public:
    BybitExecutionGateway(const std::string &apiKey, const std::string &apiSecret, bybit::Environment env = bybit::Environment::Mainnet);

    ~BybitExecutionGateway() override;

    [[nodiscard]] std::string name() const override;

    void start() override;

    InstrumentSpec instrumentSpec(const std::string &symbol) override;

    void refreshInstruments() override;

    void subscribeQuotes(const std::string &symbol) override;

    void unsubscribeQuotes(const std::string &symbol) override;

    std::optional<Quote> lastQuote(const std::string &symbol) override;

    void setOrderUpdateCallback(const onOrderUpdateEvent &cb) override;

    void setFillCallback(const onFillEvent &cb) override;

    void setQuoteCallback(const onQuoteEvent &cb) override;

    void submitPostOnlyLimit(const std::string &clientOrderId, const std::string &symbol, OrderSide side, double qty, double price, bool reduceOnly) override;

    [[nodiscard]] bool supportsAmend() const override;

    void amendPrice(const std::string &clientOrderId, const std::string &symbol, double price) override;

    bool cancel(const std::string &clientOrderId, const std::string &symbol) override;

    void submitReduceOnlyMarket(const std::string &clientOrderId, const std::string &symbol, OrderSide side, double qty) override;
};

} // namespace stonky::execution

#endif // BYBIT_EXECUTION_GATEWAY_H
