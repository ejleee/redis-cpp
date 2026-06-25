#include "store.hpp"

bool Store::is_expired(const Entry& e) const {
    return e.expires_at.has_value() && Clock::now() > *e.expires_at;
}

bool Store::set(const std::string& key, const std::string& value,
                std::optional<std::chrono::milliseconds> ttl) {
    std::lock_guard lock(mutex_);
    Entry e;
    e.value = value;
    if (ttl) e.expires_at = Clock::now() + *ttl;
    data_[key] = std::move(e);
    return true;
}

std::optional<std::string> Store::get(const std::string& key) {
    std::lock_guard lock(mutex_);
    auto it = data_.find(key);
    if (it == data_.end()) return std::nullopt;
    if (is_expired(it->second)) {
        data_.erase(it);
        return std::nullopt;
    }
    return it->second.value;
}

bool Store::del(const std::string& key) {
    std::lock_guard lock(mutex_);
    return data_.erase(key) > 0;
}

bool Store::expire(const std::string& key, std::chrono::milliseconds ttl) {
    std::lock_guard lock(mutex_);
    auto it = data_.find(key);
    if (it == data_.end() || is_expired(it->second)) return false;
    it->second.expires_at = Clock::now() + ttl;
    return true;
}

std::optional<long long> Store::ttl_ms(const std::string& key) {
    std::lock_guard lock(mutex_);
    auto it = data_.find(key);
    if (it == data_.end()) return std::nullopt;
    if (is_expired(it->second)) {
        data_.erase(it);
        return std::nullopt;
    }
    if (!it->second.expires_at) return -1; // no expiry
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        *it->second.expires_at - Clock::now()).count();
    return ms < 0 ? 0 : ms;
}

std::vector<std::pair<std::string, Entry>> Store::snapshot() const {
    std::lock_guard lock(mutex_);
    std::vector<std::pair<std::string, Entry>> out;
    for (const auto& [k, v] : data_) {
        if (!is_expired(v)) out.emplace_back(k, v);
    }
    return out;
}

void Store::load(std::vector<std::pair<std::string, Entry>> entries) {
    std::lock_guard lock(mutex_);
    data_.clear();
    for (auto& [k, v] : entries) {
        if (!is_expired(v)) data_.emplace(std::move(k), std::move(v));
    }
}

void Store::evict_expired() {
    std::lock_guard lock(mutex_);
    for (auto it = data_.begin(); it != data_.end(); ) {
        if (is_expired(it->second)) it = data_.erase(it);
        else ++it;
    }
}
