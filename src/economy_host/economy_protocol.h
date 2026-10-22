#pragma once

#include "openrepublic/economy_api/economy_api.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace openrepublic::economy_host {

constexpr std::uint32_t ProtocolVersion = 0;
constexpr std::size_t MaxRequestLineBytes = 64 * 1024;
constexpr std::size_t MaxJsonNesting = 16;
constexpr std::size_t MaxJsonNodes = 4096;
// The economy host is long-lived behind the local HTTP bridge. Keep a finite
// lifetime bound while allowing multi-day interactive sessions at UI poll rates.
constexpr std::size_t MaxRequestsPerProcess = 10'000'000;

enum class Role { Observer, Controller };

class Dispatcher final {
public:
  explicit Dispatcher(Role role, const economy::Catalog &catalog);
  /** Attaches to an existing world (and its catalog); both must outlive the dispatcher. */
  Dispatcher(Role role, economy::World &world);
  /** Borrows an existing API so command handles and event history are shared; both must outlive the dispatcher. */
  Dispatcher(Role role, economy_api::EconomyApi &api, bool requireSessionBinding,
      bool allowShutdown = false);
  std::string HandleLine(std::string_view line);
  bool ShutdownRequested() const noexcept;
  /** Free-text provenance reported by `hello` (for example an imported map region). */
  void SetRegionDescription(std::string description);
  /** Machine-readable region geometry (a JSON object text) reported by `hello` as `regionInfo`. */
  void SetRegionInfoJson(std::string json);
  /** Builds a tuned catalog from rule text (from the base catalog, not compounding); enables `reload_rules`. */
  using RulesBuilder = std::function<bool(std::string_view text, economy::Catalog &out, std::string &error)>;
  void SetRulesBuilder(RulesBuilder builder);

private:
  std::unique_ptr<economy_api::EconomyApi> ownedApi_;
  economy_api::EconomyApi *api_{};
  Role role_;
  bool requireSessionBinding_{};
  bool allowShutdown_{true};
  std::string sessionId_;
  bool shutdown_{};
  std::string regionDescription_;
  std::string regionInfoJson_;
  RulesBuilder rulesBuilder_;
  std::vector<std::unique_ptr<economy::Catalog>> retiredCatalogs_;
};

} // namespace openrepublic::economy_host
