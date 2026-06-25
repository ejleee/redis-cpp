#include "commands.hpp"
#include <algorithm>
#include <charconv>

static long long parse_ll(const std::string& s, bool& ok) {
    long long v = 0;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    ok = (ec == std::errc{} && ptr == s.data() + s.size());
    return v;
}

std::string dispatch(const Command& cmd, ClientContext& ctx) {
    if (cmd.args.empty()) return resp_error("empty command");
    const std::string& name = cmd.args[0];

    // ---- PING ----
    if (name == "PING") {
        if (cmd.args.size() > 1) return resp_bulk(cmd.args[1]);
        return resp_simple("PONG");
    }

    // ---- QUIT ----
    if (name == "QUIT") return resp_simple("OK"); // client will close

    // In pub/sub mode, only SUBSCRIBE/UNSUBSCRIBE/PING/QUIT are allowed
    if (ctx.in_pubsub && name != "SUBSCRIBE" && name != "UNSUBSCRIBE"
        && name != "PING" && name != "QUIT") {
        return resp_error("only (P)SUBSCRIBE / (P)UNSUBSCRIBE / PING / QUIT allowed in sub mode");
    }

    // ---- SET ----
    if (name == "SET") {
        if (cmd.args.size() < 3) return resp_error("wrong number of arguments for SET");
        std::optional<std::chrono::milliseconds> ttl;
        for (size_t i = 3; i < cmd.args.size(); ++i) {
            std::string opt = cmd.args[i];
            std::transform(opt.begin(), opt.end(), opt.begin(), ::toupper);
            if ((opt == "EX" || opt == "PX") && i + 1 < cmd.args.size()) {
                bool ok; long long v = parse_ll(cmd.args[++i], ok);
                if (!ok || v <= 0) return resp_error("invalid expire time");
                ttl = (opt == "EX") ? std::chrono::seconds(v) : std::chrono::milliseconds(v);
            }
        }
        ctx.store.set(cmd.args[1], cmd.args[2], ttl);
        return resp_simple("OK");
    }

    // ---- GET ----
    if (name == "GET") {
        if (cmd.args.size() < 2) return resp_error("wrong number of arguments for GET");
        auto val = ctx.store.get(cmd.args[1]);
        return val ? resp_bulk(*val) : resp_null_bulk();
    }

    // ---- DEL ----
    if (name == "DEL") {
        if (cmd.args.size() < 2) return resp_error("wrong number of arguments for DEL");
        long long count = 0;
        for (size_t i = 1; i < cmd.args.size(); ++i)
            count += ctx.store.del(cmd.args[i]) ? 1 : 0;
        return resp_integer(count);
    }

    // ---- EXISTS ----
    if (name == "EXISTS") {
        if (cmd.args.size() < 2) return resp_error("wrong number of arguments for EXISTS");
        long long count = 0;
        for (size_t i = 1; i < cmd.args.size(); ++i)
            count += ctx.store.get(cmd.args[i]).has_value() ? 1 : 0;
        return resp_integer(count);
    }

    // ---- EXPIRE / PEXPIRE ----
    if (name == "EXPIRE" || name == "PEXPIRE") {
        if (cmd.args.size() < 3) return resp_error("wrong number of arguments");
        bool ok; long long v = parse_ll(cmd.args[2], ok);
        if (!ok || v < 0) return resp_error("invalid expire time");
        auto ttl = (name == "EXPIRE") ? std::chrono::milliseconds(v * 1000)
                                      : std::chrono::milliseconds(v);
        return resp_integer(ctx.store.expire(cmd.args[1], ttl) ? 1 : 0);
    }

    // ---- TTL / PTTL ----
    if (name == "TTL" || name == "PTTL") {
        if (cmd.args.size() < 2) return resp_error("wrong number of arguments");
        auto ms = ctx.store.ttl_ms(cmd.args[1]);
        if (!ms) return resp_integer(-2); // key doesn't exist
        if (*ms == -1) return resp_integer(-1); // no expiry
        if (name == "TTL") return resp_integer(*ms / 1000);
        return resp_integer(*ms);
    }

    // ---- SUBSCRIBE ----
    if (name == "SUBSCRIBE") {
        if (cmd.args.size() < 2) return resp_error("wrong number of arguments");
        if (ctx.sub_id == 0) ctx.sub_id = 0; // will be set per-channel below
        std::string out;
        for (size_t i = 1; i < cmd.args.size(); ++i) {
            const std::string& ch = cmd.args[i];
            // Register a one-shot subscriber ID per channel; re-use or track per client
            ctx.in_pubsub = true;
            auto send_fn = ctx.send;
            int id = ctx.pubsub.subscribe(ch, [send_fn, ch](const std::string& /*ch*/, const std::string& msg) {
                send_fn(resp_array({"message", ch, msg}));
            });
            if (ctx.sub_id == 0) ctx.sub_id = id;
            out += resp_array({"subscribe", ch, std::to_string(i)});
        }
        return out;
    }

    // ---- UNSUBSCRIBE ----
    if (name == "UNSUBSCRIBE") {
        if (ctx.sub_id != 0) {
            if (cmd.args.size() < 2) {
                ctx.pubsub.unsubscribe_all(ctx.sub_id);
                ctx.in_pubsub = false;
                return resp_array({"unsubscribe", "", "0"});
            }
            for (size_t i = 1; i < cmd.args.size(); ++i)
                ctx.pubsub.unsubscribe(ctx.sub_id, cmd.args[i]);
        }
        return resp_simple("OK");
    }

    // ---- PUBLISH ----
    if (name == "PUBLISH") {
        if (cmd.args.size() < 3) return resp_error("wrong number of arguments");
        int n = ctx.pubsub.publish(cmd.args[1], cmd.args[2]);
        return resp_integer(n);
    }

    // ---- COMMAND (minimal stub for redis-cli compatibility) ----
    if (name == "COMMAND") return resp_simple("OK");

    return resp_error("unknown command '" + name + "'");
}
