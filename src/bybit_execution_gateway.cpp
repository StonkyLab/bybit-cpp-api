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
#include <cstdlib>
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

/// Numeric retCode from the REST error format "Bybit API error, code: N, ..."
/// (handleBybitResponse). WS reject reasons carry symbolic strings instead
/// (e.g. "EC_PostOnlyWillTakeLiquidity") — those fall through to text matching.
std::optional<long> extractRetCode(const std::string &reason) {
    if (const auto pos = reason.find("code: "); pos != std::string::npos) {
        return std::strtol(reason.c_str() + pos + 6, nullptr, 10);
    }

    return std::nullopt;
}

/// Port of the Python reject classifier (live_executor.py on_order_rejected).
/// Numeric retCode is the primary key (venue reject STRINGS are not contract
/// and have drifted before — audit 2026-08-13); the free-text matching remains
/// as the fallback for WS event reasons, which carry no code.
RejectKind classifyRejectReason(const std::string &reason) {
    if (const auto code = extractRetCode(reason)) {
        switch (*code) {
            case 30208: /// post-only would cross ("...can only be a maker order")
                return RejectKind::BenignPostOnlyCross;
            case 110094: /// "Order does not meet minimum order value"
                return RejectKind::MinNotional;
            case 10006: /// "Too many visits!" — venue rate limit. As Hard it
                /// would count toward the fatal reject cap and resubmit into
                /// the throttle window (the 510-as-Hard MEXC failure mode).
                return RejectKind::Throttled;
            case 110017: /// reduce-only rule not satisfied / qty would truncate
                /// to zero — the position the reduce targeted is already gone
                /// (or smaller than the order). Goal met: end the leg cleanly
                /// instead of looping the same reject to the 20-cap (the MEXC
                /// 2009/SPELL pattern). A wrong-side reduce cannot reach the
                /// venue (chase-side guard), and a partial residual is
                /// re-derived from venue truth by the hourly cleanup.
                return RejectKind::PositionClosed;
            case 10029: /// symbol not whitelisted for this API key — no retry
                /// can ever succeed; live-observed 2026-07-06
            case 30228: /// delisting — venue refuses new positions
            case 110087: /// only reduce-only allowed (pre-delist state)
                return RejectKind::Permanent;
            default:
                break; /// unrecognized code → text fallback below
        }
    }

    const auto r = toLower(reason);

    if (r.find("postonly") != std::string::npos || r.find("post only") != std::string::npos || r.find("post-only") != std::string::npos ||
        r.find("take liquidity") != std::string::npos || r.find("takeliquidity") != std::string::npos) {
        return RejectKind::BenignPostOnlyCross;
    }

    if (r.find("minimum order value") != std::string::npos || r.find("min notional") != std::string::npos || r.find("min size") != std::string::npos ||
        r.find("exceeded lower limit") != std::string::npos) {
        return RejectKind::MinNotional;
    }

    if (r.find("too many visits") != std::string::npos || r.find("rate limit") != std::string::npos) {
        return RejectKind::Throttled;
    }

    if (r.find("reduce-only rule") != std::string::npos || r.find("reduce only rule") != std::string::npos || r.find("position is zero") != std::string::npos) {
        return RejectKind::PositionClosed;
    }

    if (r.find("sign the required agreement") != std::string::npos || r.find("unmatched ip") != std::string::npos || r.find("delisting") != std::string::npos ||
        r.find("no new positions") != std::string::npos || r.find("position idx") != std::string::npos || r.find("position mode") != std::string::npos ||
        r.find("whitelist") != std::string::npos) {
        /// "position idx not match position mode" = account not in one-way mode —
        /// a config error affecting EVERY order; burning the backoff ladder on it
        /// would eat the whole chase window instead of failing fast.
        return RejectKind::Permanent;
    }

    return RejectKind::Hard;
}

/// Venue responses to cancel/amend of an order that already left the book —
/// not failures from the chase core's perspective.
bool isOrderGoneReason(const std::string &reason) {
    if (const auto code = extractRetCode(reason)) {
        switch (*code) {
            case 110001: /// order does not exist
            case 110008: /// order already finished
            case 110010: /// order already cancelled
                return true;
            default:
                break;
        }
    }

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
    /// shared_ptr (not unique): the public WS stream manager holds a weak_ptr
    /// for its REST quote fallback (readEventTicker refreshes a stale ticker
    /// from REST when the stream goes silent).
    std::shared_ptr<RESTClient> restClient;
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

    /// Health gate for operations that CREATE exposure. The private stream is
    /// the only fill/order event source and Bybit does not replay a reconnect
    /// gap — an order submitted (or re-priced) while unauthenticated could
    /// fill with nobody listening. Throttled classification: the chase core
    /// backs off and retries without burning its fatal-reject cap, and resumes
    /// the moment the stream re-authenticates. cancel() is deliberately NOT
    /// gated — cancelling reduces exposure, and its order-gone path already
    /// reconciles missed fills from REST.
    void requireEventStream(const char *op) const {
        if (!privateStream->isAuthenticated()) {
            throw GatewayError(RejectKind::Throttled, fmt::format("Bybit private stream not authenticated — {} gated until reconnect (fills would be lost)", op));
        }
    }

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
    m_p->restClient = std::make_shared<RESTClient>(apiKey, apiSecret, env);
    m_p->privateStream = std::make_unique<WSPrivateStreamManager>(apiKey, apiSecret, env);
    m_p->publicStream = std::make_unique<WSStreamManager>();

    m_p->privateStream->setLoggerCallback(&logForwarder);
    m_p->publicStream->setLoggerCallback(&logForwarder);
    /// Bound the blocking window of readEventTicker when a symbol has no data
    /// yet; the chase core polls, it must not hang for the default 5 s.
    m_p->publicStream->setTimeout(1);
    /// Wire the REST quote fallback: with a stale/silent ticker stream,
    /// readEventTicker refreshes the cached quote from REST instead of
    /// reporting nothing until the stream recovers.
    m_p->publicStream->setRestClient(m_p->restClient);

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

    m_p->requireEventStream("submit");

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
    m_p->requireEventStream("amend");

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

int BybitExecutionGateway::cancelStrayOrders(const std::string &clientOrderIdPrefix, const std::string &settleCoin) {
    int cancelled = 0;

    for (const auto &order: m_p->restClient->getOpenOrders(Category::linear, "", settleCoin)) {
        if (!order.orderLinkId.starts_with(clientOrderIdPrefix)) {
            continue; /// manual or other strategies' orders on a shared account are not ours to touch
        }

        spdlog::warn("BybitGW: stray order from a previous run — cancelling {} linkId={} qty={} px={}", order.symbol, order.orderLinkId, order.qty, order.price);

        try {
            /// cancel() handles "already gone" (reconciles fills from REST) and
            /// classifies real failures; a stray we cannot cancel is rethrown —
            /// the caller must not report a clean venue.
            cancel(order.orderLinkId, order.symbol);
            ++cancelled;
        } catch (GatewayError &e) {
            throw GatewayError(e.kind, fmt::format("stray order {} on {} could not be cancelled: {}", order.orderLinkId, order.symbol, e.what()));
        }
    }

    return cancelled;
}

} // namespace stonky::execution
