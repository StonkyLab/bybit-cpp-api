/**
Bybit Private WebSocket Stream Manager

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2026 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.
*/

#ifndef INCLUDE_STONKY_BYBIT_WS_PRIVATE_STREAM_MANAGER_H
#define INCLUDE_STONKY_BYBIT_WS_PRIVATE_STREAM_MANAGER_H

#include "stonky/utils/log_utils.h"
#include "stonky/bybit/bybit_enums.h"
#include "stonky/bybit/bybit_event_models.h"
#include <memory>
#include <string>

namespace stonky::bybit {
using onOrderUpdate = std::function<void(const EventOrderUpdate&)>;
using onExecution = std::function<void(const EventExecution&)>;

/**
 * Authenticated stream of the account's private "order" and "execution" topics
 * (wss://stream.bybit.com/v5/private). Auth, heartbeat, reconnect with re-auth
 * and re-subscribe are handled internally by the underlying session.
 *
 * Delivery is push-only via callbacks — unlike the public WSStreamManager's
 * last-value polling, private events form a sequence where every element
 * matters (fills, cancels, rejects), so a lossy latest-value cache would drop
 * order state transitions.
 *
 * IMPORTANT: callbacks are invoked on the WebSocket io thread. They must be
 * fast and non-blocking — copy the event into your own queue/state (under your
 * own lock) and return. Blocking the callback stalls the whole connection.
 *
 * Bybit gotchas the consumer must handle (see the docs):
 *  - No replay: after a disconnect the stream does NOT backfill missed events;
 *    reconcile via REST when the connection was down.
 *  - Dedup fills by execId (one message can batch several fills; a reconnect
 *    can re-deliver).
 *  - execType == Funding entries are funding cashflows, not trades.
 *  - A Cancelled order may still carry cumExecQty > 0 (partial fill before
 *    cancel); "Cancelled" does not mean zero-fill.
 */
class WSPrivateStreamManager {
    struct P;
    std::unique_ptr<P> m_p{};

public:
    /**
     * @param apiKey
     * @param apiSecret
     * @param env Bybit environment — selects the private stream host
     *            (stream / stream-testnet / stream-demo .bybit.com)
     */
    WSPrivateStreamManager(const std::string& apiKey, const std::string& apiSecret, Environment env = Environment::Mainnet);

    ~WSPrivateStreamManager();

    /**
     * Set logger callback, if not set then all errors are written to the stderr stream only
     * @param onLogMessageCB
     */
    void setLoggerCallback(const onLogMessage& onLogMessageCB) const;

    /**
     * Set the order-update callback (private "order" topic). Invoked on the
     * io thread — see the class comment.
     * @param onOrderUpdateCB
     */
    void setOrderUpdateCallback(const onOrderUpdate& onOrderUpdateCB) const;

    /**
     * Set the execution callback (private "execution" topic). Invoked on the
     * io thread — see the class comment.
     * @param onExecutionCB
     */
    void setExecutionCallback(const onExecution& onExecutionCB) const;

    /**
     * Connect, authenticate and subscribe the "order" + "execution" topics.
     * Returns immediately; events start flowing once auth completes. Set the
     * callbacks BEFORE calling connect.
     */
    void connect() const;

    /**
     * True while the CURRENT connection is authenticated (drops to false
     * during a reconnect and comes back after re-auth).
     */
    [[nodiscard]] bool isAuthenticated() const;
};
} // namespace stonky::bybit

#endif // INCLUDE_STONKY_BYBIT_WS_PRIVATE_STREAM_MANAGER_H
