#include "parser.hpp"
#include <algorithm>
#include <sstream>
#include <stdexcept>

void RespParser::feed(const char* data, size_t len) {
    buf_.append(data, len);
}

std::optional<Command> RespParser::next_command() {
    if (buf_.empty() || error_) return std::nullopt;
    if (buf_[0] == '*') return try_parse_resp_array();
    return try_parse_inline();
}

static std::optional<size_t> find_crlf(const std::string& s, size_t start = 0) {
    auto pos = s.find("\r\n", start);
    if (pos == std::string::npos) return std::nullopt;
    return pos;
}

std::optional<Command> RespParser::try_parse_resp_array() {
    auto crlf = find_crlf(buf_);
    if (!crlf) return std::nullopt;

    int count = std::stoi(buf_.substr(1, *crlf - 1));
    if (count <= 0) { buf_.erase(0, *crlf + 2); return std::nullopt; }

    size_t pos = *crlf + 2;
    Command cmd;
    for (int i = 0; i < count; ++i) {
        if (pos >= buf_.size() || buf_[pos] != '$') { error_ = true; return std::nullopt; }
        auto next_crlf = find_crlf(buf_, pos + 1);
        if (!next_crlf) return std::nullopt;
        int len = std::stoi(buf_.substr(pos + 1, *next_crlf - pos - 1));
        pos = *next_crlf + 2;
        if (pos + len + 2 > buf_.size()) return std::nullopt;
        cmd.args.push_back(buf_.substr(pos, len));
        pos += len + 2; // skip \r\n after bulk string
    }
    buf_.erase(0, pos);
    if (!cmd.args.empty())
        std::transform(cmd.args[0].begin(), cmd.args[0].end(),
                       cmd.args[0].begin(), ::toupper);
    return cmd;
}

std::optional<Command> RespParser::try_parse_inline() {
    auto crlf = find_crlf(buf_);
    if (!crlf) return std::nullopt;

    std::string line = buf_.substr(0, *crlf);
    buf_.erase(0, *crlf + 2);

    Command cmd;
    std::istringstream iss(line);
    std::string token;
    while (iss >> token) cmd.args.push_back(std::move(token));
    if (!cmd.args.empty())
        std::transform(cmd.args[0].begin(), cmd.args[0].end(),
                       cmd.args[0].begin(), ::toupper);
    return cmd.args.empty() ? std::nullopt : std::optional<Command>(cmd);
}

// ---- Response builders ----

std::string resp_simple(const std::string& s) { return "+" + s + "\r\n"; }
std::string resp_error(const std::string& s)  { return "-ERR " + s + "\r\n"; }
std::string resp_integer(long long n)          { return ":" + std::to_string(n) + "\r\n"; }
std::string resp_bulk(const std::string& s)    {
    return "$" + std::to_string(s.size()) + "\r\n" + s + "\r\n";
}
std::string resp_null_bulk()                   { return "$-1\r\n"; }
std::string resp_array(const std::vector<std::string>& parts) {
    std::string out = "*" + std::to_string(parts.size()) + "\r\n";
    for (const auto& p : parts) out += resp_bulk(p);
    return out;
}
