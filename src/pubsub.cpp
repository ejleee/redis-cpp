#include "pubsub.hpp"
#include <algorithm>

int PubSub::subscribe(const std::string& channel, SubscriberCallback cb) {
    std::lock_guard lock(mutex_);
    int id = next_id_++;
    channels_[channel].push_back({id, std::move(cb)});
    id_channels_[id].insert(channel);
    return id;
}

void PubSub::unsubscribe(int id, const std::string& channel) {
    std::lock_guard lock(mutex_);
    auto& subs = channels_[channel];
    subs.erase(std::remove_if(subs.begin(), subs.end(),
        [id](const Subscriber& s){ return s.id == id; }), subs.end());
    if (subs.empty()) channels_.erase(channel);
    if (auto it = id_channels_.find(id); it != id_channels_.end()) {
        it->second.erase(channel);
        if (it->second.empty()) id_channels_.erase(it);
    }
}

void PubSub::unsubscribe_all(int id) {
    std::lock_guard lock(mutex_);
    auto it = id_channels_.find(id);
    if (it == id_channels_.end()) return;
    for (const auto& ch : it->second) {
        auto& subs = channels_[ch];
        subs.erase(std::remove_if(subs.begin(), subs.end(),
            [id](const Subscriber& s){ return s.id == id; }), subs.end());
        if (subs.empty()) channels_.erase(ch);
    }
    id_channels_.erase(it);
}

int PubSub::publish(const std::string& channel, const std::string& message) {
    std::vector<SubscriberCallback> cbs;
    {
        std::lock_guard lock(mutex_);
        auto it = channels_.find(channel);
        if (it == channels_.end()) return 0;
        for (const auto& s : it->second) cbs.push_back(s.cb);
    }
    for (auto& cb : cbs) cb(channel, message);
    return static_cast<int>(cbs.size());
}
