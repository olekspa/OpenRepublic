#include "economy_protocol.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace {
using namespace openrepublic;

enum class LineStatus { Line, End, TooLong };

LineStatus ReadBoundedLine(std::istream &input, std::string &line) {
  line.clear();
  bool sawByte = false;
  char ch{};
  while (input.get(ch)) {
    sawByte = true;
    if (ch == '\n')
      return LineStatus::Line;
    if (line.size() == economy_host::MaxRequestLineBytes)
      return LineStatus::TooLong;
    line.push_back(ch);
  }
  return sawByte ? LineStatus::Line : LineStatus::End;
}

void PrintUsage() {
  std::cerr << "Usage: OpenRepublicEconomyHost [--role observer|controller] [--catalog file]\n"
            << "Reads protocolVersion 0 JSONL requests from stdin and writes responses to stdout.\n";
}
} // namespace

int main(int argc, char **argv) {
  economy_host::Role role = economy_host::Role::Observer;
  std::string catalogPath;
  for (int i = 1; i < argc; ++i) {
    const std::string arg(argv[i]);
    if (arg == "--role" && i + 1 < argc) {
      const std::string value(argv[++i]);
      if (value == "observer")
        role = economy_host::Role::Observer;
      else if (value == "controller")
        role = economy_host::Role::Controller;
      else {
        std::cerr << "Invalid role: " << value << "\n";
        PrintUsage();
        return 2;
      }
    } else if (arg == "--catalog" && i + 1 < argc) {
      catalogPath = argv[++i];
    } else if (arg == "--help" || arg == "-h") {
      PrintUsage();
      return 0;
    } else {
      std::cerr << "Unknown argument: " << arg << "\n";
      PrintUsage();
      return 2;
    }
  }

  economy::Catalog catalog = economy::MakeSyntheticCatalog();
  if (!catalogPath.empty()) {
    std::ifstream input(catalogPath, std::ios::binary);
    if (!input) {
      std::cerr << "Could not open catalog file.\n";
      return 2;
    }
    const std::string text((std::istreambuf_iterator<char>(input)), {});
    economy::Catalog parsed;
    economy::ParseError error;
    if (!economy::ParseCatalogText(text, parsed, error)) {
      std::cerr << "Catalog parse error at line " << error.line << ": " << error.message << "\n";
      return 2;
    }
    catalog = std::move(parsed);
  }

  economy_host::Dispatcher dispatcher(role, catalog);
  std::string line;
  std::size_t requests = 0;
  while (!dispatcher.ShutdownRequested()) {
    const auto status = ReadBoundedLine(std::cin, line);
    if (status == LineStatus::End)
      break;
    if (requests >= economy_host::MaxRequestsPerProcess) {
      std::cout << "{\"id\":null,\"protocolVersion\":0,\"ok\":false,\"error\":{\"code\":\"request_limit_"
                   "exceeded\",\"message\":\"process request limit reached\"}}\n";
      return 3;
    }
    ++requests;
    if (status == LineStatus::TooLong) {
      std::cout << "{\"id\":null,\"protocolVersion\":0,\"ok\":false,\"error\":{\"code\":\"request_too_"
                   "large\",\"message\":\"request line exceeds the configured byte limit\"}}\n";
      std::cout.flush();
      return std::cout ? 5 : 4;
    }
    std::cout << dispatcher.HandleLine(line) << '\n';
    std::cout.flush();
    if (!std::cout)
      return 4;
  }
  return 0;
}
