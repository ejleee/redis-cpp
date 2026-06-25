#pragma once
#include "store.hpp"
#include "pubsub.hpp"
#include <string>

// Handles one connected client on its own thread.
// Takes ownership of the fd (closes it on destruction).
void handle_client(int fd, Store& store, PubSub& pubsub);
