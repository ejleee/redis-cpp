#pragma once
#include "store.hpp"
#include <string>
#include <chrono>
#include <thread>
#include <atomic>

// Periodically writes the store to a binary RDB-style file and loads it on startup.
// Format: [count:uint64][key_len:uint32][key][has_expiry:uint8]
//         [expiry_ms:int64 (only if has_expiry)][val_len:uint32][val] ...
class Persistence {
public:
    Persistence(Store& store, std::string path,
                std::chrono::seconds interval = std::chrono::seconds(60));
    ~Persistence();

    void load();       // Call once at startup
    void save();       // Can be called manually (e.g. on shutdown)
    void start();      // Starts background save thread

private:
    Store& store_;
    std::string path_;
    std::chrono::seconds interval_;
    std::thread thread_;
    std::atomic<bool> running_{false};
};
