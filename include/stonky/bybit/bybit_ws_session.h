/**
Bybit Futures WebSocket Session

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.
*/

#ifndef INCLUDE_STONKY_BYBIT_WS_SESSION_H
#define INCLUDE_STONKY_BYBIT_WS_SESSION_H

#include "stonky/utils/log_utils.h"
#include "stonky/bybit/bybit_event_models.h"
#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>
#include <memory>

namespace stonky::bybit {
using onDataEvent = std::function<void(const Event &event)>;

/**
 * One TLS WebSocket connection to a Bybit V5 stream endpoint (public or private).
 *
 * Outbound messages (subscribe, auth, JSON pings) go through an internal write
 * pump, so they are sent even on a quiet stream with no inbound traffic.
 * The session keeps itself connected: on any transport error, ping expiry or
 * auth failure it tears the socket down and reconnects with exponential backoff
 * (1 s → 30 s), re-authenticates (private) and replays all subscriptions.
 * Only close() stops the reconnect loop.
 */
class WebSocketSession final : public std::enable_shared_from_this<WebSocketSession> {
    struct P;
    std::unique_ptr<P> m_p;

public:
    explicit WebSocketSession(boost::asio::io_context &ioc, boost::asio::ssl::context &ctx, const onLogMessage &onLogMessageCB);

    ~WebSocketSession();

    /**
     * Set API credentials BEFORE run(). When set, the session authenticates
     * right after the WebSocket handshake ({"op":"auth"} with an HMAC-SHA256
     * signature of "GET/realtime{expires}") and holds all subscriptions back
     * until the auth ack arrives. Required for the /v5/private endpoint.
     * @param apiKey
     * @param apiSecret
     */
    void setCredentials(const std::string &apiKey, const std::string &apiSecret) const;

    /**
     * Run the session.
     * @param host e.g. stream.bybit.com
     * @param port e.g. 443
     * @param path WebSocket upgrade path, e.g. /v5/public/linear or /v5/private
     * @param subscriptionFilter Must not be empty, e.g. tickers.BTCUSDT or order
     * @param dataEventCB Data Message callback — invoked on the io thread
     */
    void run(const std::string &host, const std::string &port, const std::string &path, const std::string &subscriptionFilter,
             const onDataEvent &dataEventCB);

    /**
     * Close the session asynchronously and disable automatic reconnect.
     */
    void close() const;

    /**
     * Subscribe WebSocket according to the subscriptionFilter. Safe to call
     * from any thread; queued until the connection is up (and authenticated,
     * when credentials are set).
     * @param subscriptionFilter e.g. tickers.BTCUSDT or order
     */
    void subscribe(const std::string &subscriptionFilter) const;

    /**
     * Unsubscribe a topic. Safe from any thread. Removes it from the replay
     * set (a reconnect will not re-subscribe it) and, when the subscribe was
     * already sent, sends the venue an unsubscribe op.
     * @param subscriptionFilter e.g. tickers.BTCUSDT
     */
    void unsubscribe(const std::string &subscriptionFilter) const;

    /**
     * True once the auth ack of the CURRENT connection was accepted. Always
     * false for public (credential-less) sessions and while reconnecting.
     */
    [[nodiscard]] bool isAuthenticated() const;

    /**
     * Check if a stream is already subscribed (sent or pending).
     * @param subscriptionFilter
     * @return True if subscribed
     */
    [[nodiscard]] bool isSubscribed(const std::string &subscriptionFilter) const;
};
} // namespace stonky::bybit
#endif // INCLUDE_STONKY_BYBIT_WS_SESSION_H
