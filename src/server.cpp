#include "store.hpp"
#include "pubsub.hpp"
#include "client.hpp"
#include "persistence.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <csignal>
#include <thread>
#include <iostream>
#include <cstring>
#include <atomic>

static std::atomic<bool> g_running{true};
static int g_server_fd = -1;

static void sig_handler(int) {
    g_running = false;
    // Closing the fd unblocks accept() immediately
    if (g_server_fd >= 0) { close(g_server_fd); g_server_fd = -1; }
}

int main(int argc, char* argv[]) {
    int port = 6380;
    std::string rdb_path = "dump.rdb";
    int rdb_interval = 60;

    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--port" && i + 1 < argc)   port = std::stoi(argv[++i]);
        if (std::string(argv[i]) == "--rdb"  && i + 1 < argc)   rdb_path = argv[++i];
        if (std::string(argv[i]) == "--save" && i + 1 < argc)   rdb_interval = std::stoi(argv[++i]);
    }

    signal(SIGINT,  sig_handler);
    signal(SIGTERM, sig_handler);
    signal(SIGPIPE, SIG_IGN); // don't crash on broken client connections

    // Bind first — if the port is busy we exit cleanly before starting any threads.
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); return 1; }
    g_server_fd = server_fd;

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(server_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        perror("bind"); close(server_fd); return 1;
    }
    if (listen(server_fd, SOMAXCONN) < 0) {
        perror("listen"); close(server_fd); return 1;
    }

    Store store;
    PubSub pubsub;
    Persistence persistence(store, rdb_path, std::chrono::seconds(rdb_interval));

    persistence.load();
    persistence.start();

    // Background eviction thread — sweeps expired keys every 100ms
    std::thread evict_thread([&] {
        while (g_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            store.evict_expired();
        }
    });

    std::cout << "redis-clone listening on port " << port << "\n";

    while (g_running) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
        if (client_fd < 0) {
            if (g_running) perror("accept");
            break;
        }

        std::thread([client_fd, &store, &pubsub] {
            handle_client(client_fd, store, pubsub);
        }).detach();
    }

    if (g_server_fd >= 0) { close(g_server_fd); g_server_fd = -1; }
    g_running = false;
    evict_thread.join();
    persistence.save();
    std::cout << "Shutdown complete.\n";
    return 0;
}
