/**
Bybit Event Data Models

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.
*/

#ifndef INCLUDE_STONKY_BYBIT_EVENT_MODELS_H
#define INCLUDE_STONKY_BYBIT_EVENT_MODELS_H

#include "stonky/interface/i_json.h"
#include "stonky/bybit/bybit_enums.h"
#include <nlohmann/json.hpp>

namespace stonky::bybit {
struct Event final : IJson {
    std::string topic{};
    ResponseType type{ResponseType::snapshot};
    std::int64_t ts{};
    nlohmann::json data{};

    ~Event() override = default;

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

struct EventTicker final : IJson {
    std::string symbol{};
    double ask1Price{};
    double ask1Size{};
    double bid1Price{};
    double bid1Size{};
    double lastPrice{};
    /// Local steady-clock receipt time in ms, stamped by WSStreamManager on
    /// every update (snapshot or delta). Not part of the wire format — lets
    /// consumers measure quote age without trusting exchange clocks.
    std::int64_t receivedTimestamp{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;

    void loadEventData(const Event& event);
};

struct EventCandlestick final : IJson {
    std::int64_t start{};
    std::int64_t end{};
    std::string interval{};
    double open{};
    double high{};
    double low{};
    double close{};
    double volume{};
    double turnover{};
    bool confirm{false};
    std::int64_t timestamp{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

/**
 * One order state change from the private WS "order" topic. The wire message
 * carries an ARRAY of these in Event.data; parse each element separately.
 * All numeric fields arrive as strings on the wire.
 * @see https://bybit-exchange.github.io/docs/v5/websocket/private/order
 */
struct EventOrderUpdate final : IJson {
    std::string category{}; /// spot | linear | inverse | option (all-in-one topic tags each row)
    std::string symbol{};
    std::string orderId{};
    std::string orderLinkId{};
    Side side{Side::None};
    OrderType orderType{OrderType::Limit};
    /// NOTE: spot-only status "PartiallyFilledCanceled" has no enum value and
    /// keeps the default; linear-perp flows never emit it.
    OrderStatus orderStatus{OrderStatus::Created};
    /// Kept as string — the enum tail is long and only a few values matter to
    /// callers (e.g. "EC_PostOnlyWillTakeLiquidity" = benign post-only cross,
    /// "EC_NoError"). See the docs for the complete list.
    std::string rejectReason{};
    std::string cancelType{}; /// e.g. CancelByUser; empty when not cancelled
    TimeInForce timeInForce{TimeInForce::GTC};
    double price{};
    double qty{};
    double avgPrice{};
    double leavesQty{};
    double cumExecQty{};
    double cumExecValue{};
    bool reduceOnly{false};
    std::int64_t createdTime{};
    std::int64_t updatedTime{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};

/**
 * One fill (execution) from the private WS "execution" topic. The wire message
 * carries an ARRAY of these in Event.data — one message may batch several fills
 * of the same order. Dedup by execId (unique per fill); seq is unique only per
 * symbol. execType==Funding entries are funding cashflows, not trades.
 * @see https://bybit-exchange.github.io/docs/v5/websocket/private/execution
 */
struct EventExecution final : IJson {
    std::string category{};
    std::string symbol{};
    std::string orderId{};
    std::string orderLinkId{};
    std::string execId{};
    Side side{Side::None};
    ExecType execType{ExecType::UNKNOWN};
    double execPrice{};
    double execQty{};
    double execValue{};
    double execFee{}; /// fee for THIS fill; negative = maker rebate
    double feeRate{};
    double leavesQty{};
    double closedSize{};
    bool isMaker{false};
    std::int64_t execTime{};
    std::int64_t seq{};

    [[nodiscard]] nlohmann::json toJson() const override;

    void fromJson(const nlohmann::json& json) override;
};
}
#endif //INCLUDE_STONKY_BYBIT_EVENT_MODELS_H
