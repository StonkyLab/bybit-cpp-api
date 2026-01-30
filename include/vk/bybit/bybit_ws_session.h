/**
Bybit Futures WebSocket Session

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@gmail.com>.
*/

#ifndef INCLUDE_VK_BYBIT_WS_SESSION_H
#define INCLUDE_VK_BYBIT_WS_SESSION_H

#include "vk/utils/log_utils.h"
#include "vk/bybit/bybit_event_models.h"
#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>
#include <memory>

namespace vk::bybit {
using onDataEvent = std::function<void(const Event &event)>;

class WebSocketSession final : public std::enable_shared_from_this<WebSocketSession> {
    struct P;
    std::unique_ptr<P> m_p;

public:
    explicit WebSocketSession(boost::asio::io_context &ioc, boost::asio::ssl::context &ctx, const onLogMessage &onLogMessageCB);

    ~WebSocketSession();

    /**
     * Run the session.
     * @param host
     * @param port
     * @param subscriptionFilter Must not be empty
     * @param dataEventCB Data Message callback
     */
    void run(const std::string &host, const std::string &port, const std::string &subscriptionFilter, const onDataEvent &dataEventCB);

    /**
     * Close the session asynchronously
     */
    void close() const;

    /**
     * Subscribe WebSocket according to the subscriptionFilter
     * @param subscriptionFilter e.g. instrument_info.100ms.BTCUSD
     * @see https://bybit-exchange.github.io/docs/futuresV2/linear/?console#t-subscribe
     */
    void subscribe(const std::string &subscriptionFilter) const;

    /**
     * Check if a stream is already subscribed
     * @param subscriptionFilter
     * @return True if subscribed
     */
    [[nodiscard]] bool isSubscribed(const std::string &subscriptionFilter) const;
};
} // namespace vk::bybit
#endif // INCLUDE_VK_BYBIT_WS_SESSION_H
