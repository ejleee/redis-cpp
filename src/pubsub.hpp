#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <mutex>
#include <functional>

// Each subscriber is identified by a unique integer ID.
// The callback is called (under no locks) when a message arrives.
using SubscriberCallback = std::function<void(const std::string& channel,
                                              const std::string& message)>;

class PubSub {
public:
    // Returns subscriber ID
    int subscribe(const std::string& channel, SubscriberCallback cb);
    void unsubscribe(int id, const std::string& channel);
    void unsubscribe_all(int id);
    int publish(const std::string& channel, const std::string& message);

private:
    struct Subscriber {
        int id;
        SubscriberCallback cb;
    };

    std::mutex mutex_;
    int next_id_ = 1;
    // channel -> list of subscribers
    std::unordered_map<std::string, std::vector<Subscriber>> channels_;
    // id -> set of subscribed channels (for cleanup)
    std::unordered_map<int, std::unordered_set<std::string>> id_channels_;
};
