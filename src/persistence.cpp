#include "persistence.hpp"
#include <fstream>
#include <cstring>
#include <iostream>

namespace {

void write_u8(std::ostream& os, uint8_t v)  { os.write(reinterpret_cast<const char*>(&v), 1); }
void write_u32(std::ostream& os, uint32_t v){ os.write(reinterpret_cast<const char*>(&v), 4); }
void write_u64(std::ostream& os, uint64_t v){ os.write(reinterpret_cast<const char*>(&v), 8); }
void write_i64(std::ostream& os, int64_t v) { os.write(reinterpret_cast<const char*>(&v), 8); }
void write_str(std::ostream& os, const std::string& s) {
    write_u32(os, static_cast<uint32_t>(s.size()));
    os.write(s.data(), s.size());
}

bool read_u8(std::istream& is, uint8_t& v)  { return !!is.read(reinterpret_cast<char*>(&v), 1); }
bool read_u32(std::istream& is, uint32_t& v){ return !!is.read(reinterpret_cast<char*>(&v), 4); }
bool read_u64(std::istream& is, uint64_t& v){ return !!is.read(reinterpret_cast<char*>(&v), 8); }
bool read_i64(std::istream& is, int64_t& v) { return !!is.read(reinterpret_cast<char*>(&v), 8); }
bool read_str(std::istream& is, std::string& s) {
    uint32_t len; if (!read_u32(is, len)) return false;
    s.resize(len); return !!is.read(s.data(), len);
}

} // namespace

Persistence::Persistence(Store& store, std::string path, std::chrono::seconds interval)
    : store_(store), path_(std::move(path)), interval_(interval) {}

Persistence::~Persistence() {
    running_ = false;
    if (thread_.joinable()) thread_.join();
}

void Persistence::load() {
    std::ifstream f(path_, std::ios::binary);
    if (!f) return; // no snapshot yet

    uint64_t count = 0;
    if (!read_u64(f, count)) return;

    std::vector<std::pair<std::string, Entry>> entries;
    for (uint64_t i = 0; i < count; ++i) {
        std::string key, val;
        uint8_t has_expiry;
        int64_t expiry_ms;
        if (!read_str(f, key)) break;
        if (!read_u8(f, has_expiry)) break;
        if (has_expiry && !read_i64(f, expiry_ms)) break;
        if (!read_str(f, val)) break;

        Entry e;
        e.value = val;
        if (has_expiry) {
            // expiry_ms is Unix epoch milliseconds
            auto now_sys = std::chrono::system_clock::now();
            auto now_steady = Clock::now();
            int64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now_sys.time_since_epoch()).count();
            int64_t remaining_ms = expiry_ms - now_ms;
            if (remaining_ms > 0) {
                e.expires_at = now_steady + std::chrono::milliseconds(remaining_ms);
            } else {
                continue; // already expired
            }
        }
        entries.emplace_back(std::move(key), std::move(e));
    }
    store_.load(std::move(entries));
    std::cout << "[persistence] loaded " << entries.size() << " keys from " << path_ << "\n";
}

void Persistence::save() {
    auto snap = store_.snapshot();
    std::ofstream f(path_, std::ios::binary | std::ios::trunc);
    if (!f) { std::cerr << "[persistence] cannot write " << path_ << "\n"; return; }

    write_u64(f, snap.size());
    auto now_sys = std::chrono::system_clock::now();
    auto now_steady = Clock::now();
    int64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now_sys.time_since_epoch()).count();

    for (const auto& [key, entry] : snap) {
        write_str(f, key);
        if (entry.expires_at) {
            auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                *entry.expires_at - now_steady).count();
            int64_t abs_ms = now_ms + remaining;
            write_u8(f, 1);
            write_i64(f, abs_ms);
        } else {
            write_u8(f, 0);
        }
        write_str(f, entry.value);
    }
    std::cout << "[persistence] saved " << snap.size() << " keys to " << path_ << "\n";
}

void Persistence::start() {
    running_ = true;
    thread_ = std::thread([this] {
        while (running_) {
            for (int i = 0; i < interval_.count() && running_; ++i)
                std::this_thread::sleep_for(std::chrono::seconds(1));
            if (running_) save();
        }
    });
}
