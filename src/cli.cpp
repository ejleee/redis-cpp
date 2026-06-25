#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

static int connect_to(const char* host, int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host, &addr.sin_addr);
    if (connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        perror("connect"); close(fd); return -1;
    }
    return fd;
}

static std::string build_resp(const std::vector<std::string>& args) {
    std::string out = "*" + std::to_string(args.size()) + "\r\n";
    for (const auto& a : args)
        out += "$" + std::to_string(a.size()) + "\r\n" + a + "\r\n";
    return out;
}

static char recv_char(int fd) {
    char c; recv(fd, &c, 1, 0); return c;
}

static std::string recv_line(int fd) {
    std::string s;
    while (true) {
        char c = recv_char(fd);
        if (c == '\r') { recv_char(fd); break; }
        s += c;
    }
    return s;
}

static void print_resp(int fd, int indent = 0) {
    char type = recv_char(fd);
    std::string pad(indent * 2, ' ');
    switch (type) {
        case '+': std::cout << pad << recv_line(fd) << "\n"; break;
        case '-': std::cout << pad << "(error) " << recv_line(fd) << "\n"; break;
        case ':': std::cout << pad << "(integer) " << recv_line(fd) << "\n"; break;
        case '$': {
            int len = std::stoi(recv_line(fd));
            if (len < 0) { std::cout << pad << "(nil)\n"; break; }
            std::string val(len, '\0');
            size_t got = 0;
            while ((int)got < len) got += recv(fd, val.data() + got, len - got, 0);
            recv_char(fd); recv_char(fd); // \r\n
            std::cout << pad << "\"" << val << "\"\n";
            break;
        }
        case '*': {
            int n = std::stoi(recv_line(fd));
            if (n < 0) { std::cout << pad << "(nil)\n"; break; }
            for (int i = 0; i < n; ++i) {
                std::cout << pad << (i + 1) << ") ";
                print_resp(fd, indent + 1);
            }
            break;
        }
        default: std::cout << pad << type << recv_line(fd) << "\n";
    }
}

static std::vector<std::string> tokenize(const std::string& line) {
    std::vector<std::string> args;
    std::istringstream iss(line);
    std::string tok;
    while (iss >> std::quoted(tok)) args.push_back(tok);
    return args;
}

int main(int argc, char* argv[]) {
    const char* host = "127.0.0.1";
    int port = 6380;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "-p" && i + 1 < argc) port = std::stoi(argv[++i]);
        if (std::string(argv[i]) == "-h" && i + 1 < argc) host = argv[++i];
    }

    int fd = connect_to(host, port);
    if (fd < 0) return 1;
    std::cout << "Connected to " << host << ":" << port << ". Type commands, Ctrl-D to quit.\n";

    std::string line;
    while (true) {
        std::cout << host << ":" << port << "> " << std::flush;
        if (!std::getline(std::cin, line)) break;
        if (line.empty()) continue;

        auto args = tokenize(line);
        if (args.empty()) continue;
        if (args[0] == "quit" || args[0] == "exit") break;

        std::string msg = build_resp(args);
        send(fd, msg.data(), msg.size(), 0);
        print_resp(fd);
    }

    close(fd);
    return 0;
}
