#include "agent_protocol.h"

#include <iostream>
#include <string>

namespace {
using namespace openrepublic::agent_host;

enum class LineStatus { Line, End, TooLong };

LineStatus ReadBoundedLine(std::istream& input, std::string& line) {
    line.clear();
    bool sawByte = false;
    char ch{};
    while (input.get(ch)) {
        sawByte = true;
        if (ch == '\n') return LineStatus::Line;
        if (line.size() == MaxRequestLineBytes) return LineStatus::TooLong;
        line.push_back(ch);
    }
    if (!sawByte) return LineStatus::End;
    return LineStatus::Line;
}

void PrintUsage() {
    std::cerr << "Usage: OpenRepublicAgentHost [--role observer|controller]\n"
        << "Reads one protocolVersion 0 JSON request per line from stdin; writes one response per line to stdout.\n";
}
}

int main(int argc, char** argv) {
    Role role = Role::Observer;
    for (int i = 1; i < argc; ++i) {
        const std::string arg(argv[i]);
        if (arg == "--role" && i + 1 < argc) {
            const std::string value(argv[++i]);
            if (value == "observer") role = Role::Observer;
            else if (value == "controller") role = Role::Controller;
            else {
                std::cerr << "Invalid role: " << value << "\n";
                PrintUsage();
                return 2;
            }
        } else if (arg == "--help" || arg == "-h") {
            PrintUsage();
            return 0;
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            PrintUsage();
            return 2;
        }
    }

    Dispatcher dispatcher(role);
    std::string line;
    std::size_t requestCount = 0;
    while (true) {
        const auto status = ReadBoundedLine(std::cin, line);
        if (status == LineStatus::End) break;
        if (requestCount >= MaxRequestsPerProcess) {
            std::cout << "{\"id\":null,\"protocolVersion\":0,\"ok\":false,\"error\":{\"code\":\"request_limit_exceeded\",\"message\":\"process request limit reached\"}}\n";
            std::cout.flush();
            return 3;
        }
        ++requestCount;
        if (status == LineStatus::TooLong) {
            std::cout << "{\"id\":null,\"protocolVersion\":0,\"ok\":false,\"error\":{\"code\":\"request_too_large\",\"message\":\"request line exceeds the configured byte limit\"}}\n";
            std::cout.flush();
            if (!std::cout) return 4;
            return 5;
        } else {
            std::cout << dispatcher.HandleLine(line) << '\n';
        }
        std::cout.flush();
        if (!std::cout) return 4;
    }
    return 0;
}
