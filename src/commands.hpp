#pragma once
#include "store.hpp"
#include "pubsub.hpp"
#include "parser.hpp"
#include <string>
#include <functional>

// Context passed to every command handler.
struct ClientContext {
    Store& store;
    PubSub& pubsub;
    int sub_id = 0;          // assigned when client first subscribes
    bool in_pubsub = false;  // once subscribed, only SUBSCRIBE/UNSUBSCRIBE/PING allowed
    // Called to push data to the client socket (used by SUBSCRIBE callbacks)
    std::function<void(std::string)> send;
};

// Returns a RESP-encoded response string.
std::string dispatch(const Command& cmd, ClientContext& ctx);
