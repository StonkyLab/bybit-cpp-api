/**
Bybit Execution Gateway

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2026 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.
*/

#include "stonky/bybit/bybit_execution_gateway.h"
#include "stonky/bybit/bybit_http_session.h"
#include "stonky/bybit/bybit_rest_client.h"
#include "stonky/bybit/bybit_ws_private_stream_manager.h"
#include "stonky/bybit/bybit_ws_stream_manager.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <map>
#include <mutex>
#include <thread>

namespace stonky::execution {
using namespace stonky::bybit;

namespace {
void logForwarder(const LogSeverity severity, const std::string &message) {
    switch (severity) {
        case LogSeverity::Info:
            spdlog::info(message);
            break;
        case LogSeverity::Warning:
            spdlog::warn(message);
            break;
        case LogSeverity::Critical:
        case LogSeverity::Error:
            spdlog::error(message);
            break;
        default:
            spdlog::debug(message);
            break;
    }
}

std::string toLower(std::string s) {
    std::ranges::transform(s, s.begin(), [](const unsigned char c) { return std::tolower(c); });
    return s;
}

/// Port of the Python reject classifier (live_executor.py on_order_rejected)
/// extended with the sync REST error strings observed on Bybit V5.
RejectKind classifyRejectReason(const std::string &reason) {
    const auto r = toLower(reason);

    if (r.find("postonly") != std::string::npos || r.find("post only") != std::string::npos || r.find("post-only") != std::string::npos ||
        r.find("take liquidity") != std::string::npos || r.find("takeliquidity") != std::string::npos) {
        return RejectKind::BenignPostOnlyCross;
    }

    if (r.find("minimum order value") != std::string::npos || r.find("min notional") != std::string::npos || r.find("min size") != std::string::npos ||
        r.find("exceeded lower limit") != std::string::npos) {
        return RejectKind::MinNotional;
    }

    /// 10006 "Too many visits!" — venue rate limit. As Hard it would count
    /// toward the fatal reject cap and resubmit into the throttle window
    /// (the exact 510-as-Hard failure mode fixed on the MEXC side).
    if (r.find("code: 10006") != std::string::npos || r.find("too many visits") != std::string::npos || r.find("rate limit") != std::string::npos) {
        return RejectKind::Throttled;
    }

    /// 110017 "reduce-only rule not satisfied" / "position is zero" — a reduce
    /// whose position is already gone. Goal met: end the leg cleanly instead of
    /// looping the same reject to the 20-cap (the MEXC 2009/SPELL pattern). A
    /// wrong-side reduce cannot reach the venue (chase-side guard), and a
    /// partial residual is re-derived from venue truth by the hourly cleanup.
    if (r.find("code: 110017") != std::string::npos || r.find("reduce-only rule") != std::string::npos || r.find("reduce only rule") != std::string::npos ||
        r.find("position is zero") != std::string::npos) {
        return RejectKind::PositionClosed;
    }

    if (r.find("sign the required agreement") != std::string::npos || r.find("unmatched ip") != std::string::npos || r.find("delisting") != std::string::npos ||
        r.find("no new positions") != std::string::npos || r.find("position idx") != std::string::npos || r.find("position mode") != std::string::npos ||
        r.find("whitelist") != std::string::npos) {
        /// "position idx not match position mode" = account not in one-way mode —
        /// a config error affecting EVERY order; burning the backoff ladder on it
        /// would eat the whole chase window instead of failing fast.
        /// 10029 "symbol is not whitelisted" = the API key cannot trade this
        /// symbol at all (key symbol restriction, pre-market perp) — no retry
        /// can ever succeed; live-observed 2026-07-06.
        return RejectKind::Permanent;
    }

    return RejectKind::Hard;
}

/// Venue responses to cancel/amend of an order that already left the book —
/// not failures from the chase core's perspective.
bool isOrderGoneReason(const std::string &reason) {
    const auto r = toLower(reason);
    return r.find("order not exists") != std::string::npos || r.find("too late") != std::string::npos || r.find("order does not exist") != std::string::npos ||
           r.find("110001") != std::string::npos;
}

/// Server-side responses whose outcome is UNKNOWN — Bybit may have executed the
/// request despite answering with an error. retCode 10000 "Server Timeout" and
/// 10016 "Server error" arrive as valid API responses; HTTP 5xx ("Bad response,
/// code 5xx" from checkResponse) never carries a retCode at all. None of them
/// prove the order does NOT exist, so they must not become a definitive
/// GatewayError reject — the chase core's non-GatewayError path safety-cancels
/// and reconciles instead.
bool isAmbiguousOutcomeReason(const std::string &reason) {
    const auto r = toLower(reason);
    return r.find("code: 10000") != std::string::npos || r.find("code: 10016") != std::string::npos || r.find("bad response, code 5") != std::string::npos;
}
} // namespace

struct BybitExecutionGateway::P {
    std::unique_ptr<RESTClient> restClient;
    std::unique_ptr<WSPrivateStreamManager> privateStream;
    std::unique_ptr<WSStreamManager> publicStream;

    onOrderUpdateEvent orderUpdateCB;
    onFillEvent fillCB;
    onQuoteEvent quoteCB;

    std::mutex specM;
    std::map<std::string, InstrumentSpec> specCache;

    /// EventTicker.receivedTimestamp is steady-clock ms — reconstruct the
    /// steady time_point for the Quote freshness contract.
    static Quote toQuote(const EventTicker &ticker) {
        Quote quote;
        quote.bid = ticker.bid1Price;
        quote.ask = ticker.ask1Price;
        quote.receivedAt = std::chrono::steady_clock::time_point(std::chrono::milliseconds(ticker.receivedTimestamp));
        return quote;
    }

    static OrderSide toSide(const Side side) { return side == Side::Buy ? OrderSide::Buy : OrderSide::Sell; }

    static Side fromSide(const OrderSide side) { return side == OrderSide::Buy ? Side::Buy : Side::Sell; }

    /// The venue reported an order gone at cancel time — "gone" can mean FILLED.
    /// If the private WS dropped that fill during a reconnect gap, the core's
    /// accounting is stale and its next submit would OVERFILL the target. Pull
    /// the order's executions from REST and re-emit them: each carries a stable
    /// execId, so the core dedups against anything the WS did deliver
    /// (re-crediting is harmless) and credits only what it missed. Bybit uses
    /// amend, so cancel — and thus this path — is rare.
    ///
    /// INVARIANT (keeps this stateless + race-free): cancel() — and therefore
    /// this — is always called synchronously on the owning leg's worker thread
    /// while its clientOrderId route is still live (routes are swept only at the
    /// very end of finishLeg). fillCB → onFill then routes to the live leg and
    /// dedups by execId. Do NOT call cancel() off the leg thread or after
    /// teardown, or a genuinely-missed fill would be dropped instead of credited.
    void reconcileMissedFills(const std::string &clientOrderId, const std::string &symbol) const {
        if (!fillCB) {
            return;
        }

        try {
            int reemitted = 0;

            for (const auto &execution: restClient->getExecutions(Category::linear, symbol, clientOrderId)) {
                if (execution.execType != ExecType::Trade || execution.execId.empty()) {
                    continue;
                }

                FillEvent fill;
                fill.clientOrderId = clientOrderId;
                fill.symbol = symbol;
                fill.fillId = execution.execId; /// same key as the WS feed → core dedups
                fill.qty = execution.execQty;
                fill.price = execution.execPrice;
                fill.isMaker = execution.isMaker;
                fillCB(fill);
                ++reemitted;
            }

            if (reemitted > 0) {
                spdlog::info("BybitGW: {} order {} gone at cancel — re-emitted {} execution(s) from REST (WS-gap safety; core dedups by execId, credits any missed fill)", symbol,
                             clientOrderId, reemitted);
            } else {
                /// Empty is normal for a cleanly-cancelled (never-filled) order.
                /// It is ALSO what a not-yet-indexed execution/list looks like, so
                /// a WS-gap fill coinciding with REST lag would stay uncredited
                /// here — bounded by the hourly venue-truth cleanup + neutrality
                /// alert. Debug-level to avoid alarming on every teardown cancel.
                spdlog::debug("BybitGW: {} order {} gone at cancel — no executions from REST (unfilled cancel, or REST lag on a gap fill)", symbol, clientOrderId);
            }
        } catch (std::exception &e) {
            spdlog::warn("BybitGW: {} fill reconciliation for {} failed ({}) — verify the position; a WS-gap fill may be uncredited", symbol, clientOrderId, e.what());
        }
    }
};

BybitExecutionGateway::BybitExecutionGateway(const std::string &apiKey, const std::string &apiSecret, const Environment env) : m_p(std::make_unique<P>()) {
    m_p->restClient = std::make_unique<RESTClient>(apiKey, apiSecret, env);
    m_p->privateStream = std::make_unique<WSPrivateStreamManager>(apiKey, apiSecret, env);
    m_p->publicStream = std::make_unique<WSStreamManager>();

    m_p->privateStream->setLoggerCallback(&logForwarder);
    m_p->publicStream->setLoggerCallback(&logForwarder);
    /// Bound the blocking window of readEventTicker when a symbol has no data
    /// yet; the chase core polls, it must not hang for the default 5 s.
    m_p->publicStream->setTimeout(1);

    m_p->publicStream->setTickerUpdateCallback([this](const EventTicker &ticker) {
        if (m_p->quoteCB) {
            m_p->quoteCB(ticker.symbol, P::toQuote(ticker));
        }
    });

    m_p->privateStream->setOrderUpdateCallback([this](const EventOrderUpdate &event) {
        spdlog::debug("BybitGW order event: {} {} linkId={} px={} cumQty={} reject={}", event.symbol, magic_enum::enum_name(event.orderStatus), event.orderLinkId,
                      event.price, event.cumExecQty, event.rejectReason);

        if (!m_p->orderUpdateCB || event.orderLinkId.empty()) {
            return; /// orders without our client id are not ours
        }

        OrderUpdate update;
        update.clientOrderId = event.orderLinkId;
        update.symbol = event.symbol;
        update.price = event.price;
        update.cumFilledQty = event.cumExecQty;
        update.reason = event.rejectReason;

        switch (event.orderStatus) {
            case OrderStatus::Created:
            case OrderStatus::New:
                update.state = OrderState::Accepted;
                break;
            case OrderStatus::PartiallyFilled:
                update.state = OrderState::PartiallyFilled;
                break;
            case OrderStatus::Filled:
                update.state = OrderState::Filled;
                break;
            case OrderStatus::PendingCancel:
                return; /// interim state — wait for the definitive Cancelled/Filled
            case OrderStatus::Cancelled:
            case OrderStatus::PartiallyFilledCanceled:
            case OrderStatus::Deactivated:
                /// Bybit delivers a post-only cross as orderStatus=Cancelled with
                /// rejectReason=EC_PostOnlyWillTakeLiquidity (live-observed on
                /// Demo; the docs claim Rejected — handle both). Translate to a
                /// benign reject so the chase core applies its cross backoff
                /// instead of treating it as a plain cancel.
                if (classifyRejectReason(event.rejectReason) == RejectKind::BenignPostOnlyCross) {
                    update.state = OrderState::Rejected;
                    update.rejectKind = RejectKind::BenignPostOnlyCross;
                } else {
                    update.state = OrderState::Cancelled;
                }
                break;
            case OrderStatus::Rejected:
                update.state = OrderState::Rejected;
                update.rejectKind = classifyRejectReason(event.rejectReason);
                break;
            default:
                return; /// Untriggered/Triggered/Active — conditional orders, not used
        }

        m_p->orderUpdateCB(update);
    });

    m_p->privateStream->setExecutionCallback([this](const EventExecution &event) {
        /// Funding cashflows, ADL, liquidations etc. share the topic — only
        /// genuine trades count toward fill accounting.
        if (!m_p->fillCB || event.execType != ExecType::Trade || event.orderLinkId.empty()) {
            return;
        }

        FillEvent fill;
        fill.clientOrderId = event.orderLinkId;
        fill.symbol = event.symbol;
        fill.fillId = event.execId;
        fill.qty = event.execQty;
        fill.price = event.execPrice;
        fill.isMaker = event.isMaker;
        m_p->fillCB(fill);
    });
}

BybitExecutionGateway::~BybitExecutionGateway() = default;

std::string BybitExecutionGateway::name() const { return "Bybit"; }

void BybitExecutionGateway::start() {
    /// Prime the REST client's instruments cache with the FULL linear universe
    /// (paginated, force-refreshed). placeOrder/amendOrder derive each order's
    /// price/qty precision from this cache — without the full set they fall
    /// back to 0.01 steps and the venue rejects anything finer-grained.
    const auto instrumentCount = m_p->restClient->getInstrumentsInfo(Category::linear, "", true).size();
    spdlog::info("BybitExecutionGateway: {} linear instruments cached", instrumentCount);

    m_p->privateStream->connect();

    /// The event feed must be live before any order op — a fill on an order
    /// submitted pre-auth would be lost (Bybit does not replay).
    for (int i = 0; i < 100; ++i) {
        if (m_p->privateStream->isAuthenticated()) {
            spdlog::info("BybitExecutionGateway: private stream authenticated");
            return;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    throw std::runtime_error("BybitExecutionGateway: private stream auth timeout (10 s)");
}

InstrumentSpec BybitExecutionGateway::instrumentSpec(const std::string &symbol) {
    {
        std::lock_guard lk(m_p->specM);

        if (const auto it = m_p->specCache.find(symbol); it != m_p->specCache.end()) {
            return it->second;
        }
    }

    /// First pass hits the full cache primed at start(); the force retry
    /// covers a market listed after startup (symbol-filtered fetch, which by
    /// design does NOT touch the shared cache).
    for (const bool force: {false, true}) {
        for (const auto &inst: m_p->restClient->getInstrumentsInfo(Category::linear, symbol, force)) {
            if (inst.symbol == symbol) {
                InstrumentSpec spec;
                spec.symbol = symbol;
                spec.tickSize = inst.priceFilter.tickSize;
                spec.qtyStep = inst.lotSizeFilter.qtyStep;
                spec.minQty = inst.lotSizeFilter.minOrderQty;
                spec.maxQty = inst.lotSizeFilter.maxOrderQty;
                spec.minNotional = inst.lotSizeFilter.minNotionalValue;

                std::lock_guard lk(m_p->specM);
                m_p->specCache[symbol] = spec;
                return spec;
            }
        }
    }

    throw std::runtime_error(fmt::format("Bybit: unknown instrument {}", symbol));
}

void BybitExecutionGateway::refreshInstruments() {
    /// Force-reload the REST client's full linear universe (placeOrder derives
    /// price/qty precision from it) and drop the spec cache — instrumentSpec
    /// then lazily re-reads from the fresh cache at zero extra REST cost.
    const auto instrumentCount = m_p->restClient->getInstrumentsInfo(Category::linear, "", true).size();

    std::lock_guard lk(m_p->specM);
    m_p->specCache.clear();
    spdlog::debug("BybitExecutionGateway: instruments refreshed ({} linear)", instrumentCount);
}

void BybitExecutionGateway::subscribeQuotes(const std::string &symbol) { m_p->publicStream->subscribeTickerStream(symbol); }

void BybitExecutionGateway::unsubscribeQuotes(const std::string &symbol) { m_p->publicStream->unsubscribeTickerStream(symbol); }

std::optional<Quote> BybitExecutionGateway::lastQuote(const std::string &symbol) {
    if (const auto ticker = m_p->publicStream->readEventTicker(symbol)) {
        return P::toQuote(*ticker);
    }

    return std::nullopt;
}

void BybitExecutionGateway::setOrderUpdateCallback(const onOrderUpdateEvent &cb) { m_p->orderUpdateCB = cb; }

void BybitExecutionGateway::setFillCallback(const onFillEvent &cb) { m_p->fillCB = cb; }

void BybitExecutionGateway::setQuoteCallback(const onQuoteEvent &cb) { m_p->quoteCB = cb; }

void BybitExecutionGateway::submitPostOnlyLimit(const std::string &clientOrderId, const std::string &symbol, const OrderSide side, const double qty, const double price,
                                                const bool reduceOnly) {
    Order order;
    order.category = Category::linear;
    order.symbol = symbol;
    order.side = P::fromSide(side);
    order.orderType = OrderType::Limit;
    order.timeInForce = TimeInForce::PostOnly;
    order.qty = qty;
    order.price = price;
    order.reduceOnly = reduceOnly;
    order.orderLinkId = clientOrderId;
    order.positionIdx = 0; /// one-way mode required

    try {
        [[maybe_unused]] const auto orderId = m_p->restClient->placeOrder(order);
    } catch (TransportError &) {
        /// Outcome UNKNOWN — the order may rest on the venue although we never
        /// saw the ack. Propagate untouched: GatewayError means "the venue
        /// definitively rejected this", and on that the chase core deletes the
        /// route; here it must instead keep the route and safety-cancel.
        throw;
    } catch (std::exception &e) {
        if (isAmbiguousOutcomeReason(e.what())) {
            throw; /// server-side timeout/5xx — same unknown-outcome contract as TransportError
        }

        throw GatewayError(classifyRejectReason(e.what()), e.what());
    }
}

bool BybitExecutionGateway::supportsAmend() const { return true; }

void BybitExecutionGateway::amendPrice(const std::string &clientOrderId, const std::string &symbol, const double price) {
    try {
        [[maybe_unused]] const auto orderId = m_p->restClient->amendOrder(Category::linear, symbol, "", clientOrderId, price);
    } catch (TransportError &) {
        /// Outcome UNKNOWN — the order may now rest at either price. Propagate
        /// untouched; the core's amend-failure path cancels the order, which
        /// resolves the ambiguity either way.
        throw;
    } catch (std::exception &e) {
        if (isAmbiguousOutcomeReason(e.what())) {
            throw;
        }

        throw GatewayError(classifyRejectReason(e.what()), e.what());
    }
}

bool BybitExecutionGateway::cancel(const std::string &clientOrderId, const std::string &symbol) {
    try {
        const auto orderId = m_p->restClient->cancelOrder(Category::linear, symbol, "", clientOrderId);
        spdlog::debug("BybitGW cancel ack: {} linkId={} orderId={}", symbol, clientOrderId, orderId.orderId);
        return true;
    } catch (TransportError &) {
        /// Outcome UNKNOWN — the cancel may or may not have reached the venue.
        /// Propagate untouched so the core keeps the order pending and retries,
        /// instead of reading a definitive venue answer into a network fault.
        throw;
    } catch (std::exception &e) {
        if (isOrderGoneReason(e.what())) {
            spdlog::debug("BybitGW cancel — order already gone: {} linkId={} ({})", symbol, clientOrderId, e.what());
            /// "Gone" can mean FILLED — if the private WS dropped the fill, the
            /// core would resubmit the full remaining and overfill. Reconcile
            /// from REST before reporting the order gone.
            m_p->reconcileMissedFills(clientOrderId, symbol);
            return false; /// terminal event already delivered (or lost) — do not wait for one
        }

        if (isAmbiguousOutcomeReason(e.what())) {
            throw;
        }

        throw GatewayError(classifyRejectReason(e.what()), e.what());
    }
}

void BybitExecutionGateway::submitReduceOnlyMarket(const std::string &clientOrderId, const std::string &symbol, const OrderSide side, const double qty) {
    Order order;
    order.category = Category::linear;
    order.symbol = symbol;
    order.side = P::fromSide(side);
    order.orderType = OrderType::Market;
    order.timeInForce = TimeInForce::IOC;
    order.qty = qty;
    order.reduceOnly = true;
    order.orderLinkId = clientOrderId;
    order.positionIdx = 0;

    try {
        [[maybe_unused]] const auto orderId = m_p->restClient->placeOrder(order);
    } catch (TransportError &) {
        /// Outcome UNKNOWN — a market IOC may have executed without the ack.
        /// Propagate untouched; the caller must verify the position instead of
        /// re-sending the close on a supposed reject.
        throw;
    } catch (std::exception &e) {
        if (isAmbiguousOutcomeReason(e.what())) {
            throw;
        }

        throw GatewayError(classifyRejectReason(e.what()), e.what());
    }
}

} // namespace stonky::execution
