#pragma once
#include <string>
#include <vector>
#include <optional>

// RESP (Redis Serialization Protocol) parser.
// Handles inline commands and proper RESP arrays.
struct Command {
    std::vector<std::string> args; // args[0] is the command name (uppercased)
};

class RespParser {
public:
    // Feed more data into the buffer.
    void feed(const char* data, size_t len);

    // Try to parse one complete command. Returns nullopt if not enough data yet.
    std::optional<Command> next_command();

    bool has_error() const { return error_; }

private:
    std::string buf_;
    bool error_ = false;

    std::optional<Command> try_parse_resp_array();
    std::optional<Command> try_parse_inline();
};

// RESP response builders
std::string resp_simple(const std::string& s);
std::string resp_error(const std::string& s);
std::string resp_integer(long long n);
std::string resp_bulk(const std::string& s);
std::string resp_null_bulk();
std::string resp_array(const std::vector<std::string>& parts);
