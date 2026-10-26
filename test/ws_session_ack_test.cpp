/**
Bybit WS session — subscribe-ack bookkeeping check (offline)

Copyright (c) 2026 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.

Feeds subscribe acks straight into the session's control-message handler and
checks what isSubscribed() / isSubscriptionConfirmed() report afterwards — with
and without req_id, success and failure. Includes the session source to reach
its internals; does not link bybit_api.
*/

#include "../src/bybit_ws_session.cpp"

#include <cstdio>
#include <cstdlib>

#define CHECK(cond) ((cond) ? (void) 0 : (std::fprintf(stderr, "CHECK failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__), std::abort()))

using namespace stonky::bybit;

int main() {
    boost::asio::io_context ioc;
    boost::asio::ssl::context ctx(boost::asio::ssl::context::tls_client);

    const auto inflight = [](WebSocketSession::P &s, const std::string &reqId, std::vector<std::string> topics) {
        for (const auto &t: topics) {
            s.subscriptions.push_back(t);
        }
        s.inflightSubscribes[reqId] = std::move(topics);
    };
    const auto ack = [](WebSocketSession::P &s, const bool success, const std::string &reqId) {
        nlohmann::json j{{"op", "subscribe"}, {"success", success}, {"ret_msg", success ? "" : "internal server error"}};
        if (!reqId.empty()) {
            j["req_id"] = reqId;
        }
        s.handleApiControlMsg({}, 0, j);
    };

    // 1. Ack with req_id: success confirms exactly that op's topics.
    {
        WebSocketSession::P s(ioc, ctx, {});
        inflight(s, "1", {"order", "execution"});
        CHECK(s.isSubscribed("order") && !s.isSubscriptionConfirmed("order"));
        ack(s, true, "1");
        CHECK(s.isSubscriptionConfirmed("order") && s.isSubscriptionConfirmed("execution"));
    }
    // 2. Failure WITHOUT req_id: attributed to the oldest op, its topics pruned
    //    (so they are requested again), the younger op untouched.
    {
        WebSocketSession::P s(ioc, ctx, {});
        inflight(s, "9", {"order", "execution"});
        inflight(s, "10", {"tickers.BTCUSDT"}); // "10" sorts before "9" as text — must not be taken for the oldest
        ack(s, false, "");
        CHECK(!s.isSubscribed("order") && !s.isSubscribed("execution"));
        CHECK(s.inflightSubscribes.size() == 1 && s.inflightSubscribes.contains("10"));
        ack(s, true, "");
        CHECK(s.isSubscriptionConfirmed("tickers.BTCUSDT") && s.inflightSubscribes.empty());
    }
    // 3. Success WITHOUT req_id confirms the oldest op only.
    {
        WebSocketSession::P s(ioc, ctx, {});
        inflight(s, "2", {"order"});
        inflight(s, "3", {"execution"});
        ack(s, true, "");
        CHECK(s.isSubscriptionConfirmed("order") && !s.isSubscriptionConfirmed("execution"));
    }
    // 4. An ack carrying a req_id we do not know is not attributed to anything.
    {
        WebSocketSession::P s(ioc, ctx, {});
        inflight(s, "4", {"order"});
        ack(s, false, "77");
        CHECK(s.isSubscribed("order") && s.inflightSubscribes.contains("4"));
    }

    std::puts("ws_session_ack_test: all checks passed");
    return 0;
}
