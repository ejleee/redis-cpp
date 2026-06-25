#pragma once
#include <string>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <optional>
#include <vector>

using Clock = std::chrono::steady_clock;
using TimePoint = std::chrono::time_point<Clock>;

struct Entry {
    std::string value;
    std::optional<TimePoint> expires_at;
};

class Store {
public:
    bool set(const std::string& key, const std::string& value,
             std::optional<std::chrono::milliseconds> ttl = std::nullopt);
    std::optional<std::string> get(const std::string& key);
    bool del(const std::string& key);
    bool expire(const std::string& key, std::chrono::milliseconds ttl);
    std::optional<long long> ttl_ms(const std::string& key);

    // Persistence helpers — caller must hold no lock
    std::vector<std::pair<std::string, Entry>> snapshot() const;
    void load(std::vector<std::pair<std::string, Entry>> entries);

    void evict_expired();

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, Entry> data_;

    bool is_expired(const Entry& e) const;
};
