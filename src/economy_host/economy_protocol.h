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
  /** Attaches to an existing world (and its catalog); both must outlive the dispatcher. */
  Dispatcher(Role role, economy::World &world);
  std::string HandleLine(std::string_view line);
  bool ShutdownRequested() const noexcept;
  /** Free-text provenance reported by `hello` (for example an imported map region). */
  void SetRegionDescription(std::string description);
  /** Machine-readable region geometry (a JSON object text) reported by `hello` as `regionInfo`. */
  void SetRegionInfoJson(std::string json);

private:
  economy_api::EconomyApi api_;
  Role role_;
  bool shutdown_{};
  std::string regionDescription_;
  std::string regionInfoJson_;
};

} // namespace openrepublic::economy_host
