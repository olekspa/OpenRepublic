#pragma once

#include "openrepublic/agent_api/agent_api.h"

#include <cstddef>
#include <string>
#include <string_view>

namespace openrepublic::agent_host {

constexpr std::uint32_t ProtocolVersion = 0;
constexpr std::size_t MaxRequestLineBytes = 64 * 1024;
constexpr std::size_t MaxJsonNesting = 16;
constexpr std::size_t MaxJsonNodes = 4096;
constexpr std::size_t MaxRequestsPerProcess = 8192;

enum class Role { Observer, Controller };

// One process owns one in-memory synthetic simulation. This is a local stdio
// protocol adapter, not an authenticated network or multi-client boundary.
class Dispatcher final {
public:
    explicit Dispatcher(Role role = Role::Observer);

    // Parses and dispatches exactly one complete JSON request line. Malformed
    // requests return a protocol error and never call the gameplay API.
    std::string HandleLine(std::string_view line);

private:
    agent_api::AgentApi api_;
    Role role_;
};

} // namespace openrepublic::agent_host
