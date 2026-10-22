#pragma once

#include "openrepublic/economy_api/economy_api.h"

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>

namespace openrepublic::economy_host {

constexpr std::uint32_t ProtocolVersion = 0;
constexpr std::size_t MaxRequestLineBytes = 64 * 1024;
constexpr std::size_t MaxJsonNesting = 16;
constexpr std::size_t MaxJsonNodes = 4096;
constexpr std::size_t MaxRequestsPerProcess = 8192;

enum class Role { Observer, Controller };

class Dispatcher final {
public:
  explicit Dispatcher(Role role, const economy::Catalog &catalog);
  std::string HandleLine(std::string_view line);
  bool ShutdownRequested() const noexcept;

private:
  economy_api::EconomyApi api_;
  Role role_;
  bool shutdown_{};
};

} // namespace openrepublic::economy_host
