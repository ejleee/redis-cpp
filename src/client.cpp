#include "client.hpp"
#include "commands.hpp"
#include "parser.hpp"
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <mutex>

namespace {
    // Serialize concurrent writes to stdout
    std::mutex log_mutex;
}

void handle_client(int fd, Store& store, PubSub& pubsub) {
    RespParser parser;

    // Thread-safe write helper
    auto send_response = [&](const std::string& data) {
        size_t total = 0;
        while (total < data.size()) {
            ssize_t n = ::send(fd, data.data() + total, data.size() - total, MSG_NOSIGNAL);
            if (n <= 0) return;
            total += n;
        }
    };

    ClientContext ctx{store, pubsub, 0, false, send_response};

    char buf[4096];
    while (true) {
        ssize_t n = recv(fd, buf, sizeof(buf), 0);
        if (n <= 0) break; // client disconnected or error

        parser.feed(buf, n);
        if (parser.has_error()) break;

        while (auto cmd = parser.next_command()) {
            std::string resp = dispatch(*cmd, ctx);
            if (!resp.empty()) send_response(resp);

            // QUIT: flush then close
            if (!cmd->args.empty() && cmd->args[0] == "QUIT") goto done;
        }
    }
done:
    if (ctx.sub_id != 0) pubsub.unsubscribe_all(ctx.sub_id);
    close(fd);
}
