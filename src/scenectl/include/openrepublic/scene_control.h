#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>

namespace openrepublic::scenectl {

inline constexpr std::uint32_t ProtocolVersion = 0;
inline constexpr std::size_t MaxRequestLineBytes = 64 * 1024;

struct CameraLook {
  double yawDegrees{45.0};
  double elevationDegrees{24.5};
  double distanceFactor{1.0};
};

struct SceneLook {
  double sunElevationDegrees{35.0};
  double sunYawOffsetDegrees{0.0};
  double sunWarmthKelvin{5200.0};
  double exposure{1.0};
  double cloudCoverage{0.0};
  double fogDensity{0.0};
  bool oceanEnabled{true};
  bool shadowsEnabled{true};
  bool bloomEnabled{true};
  bool ssaoEnabled{true};
  bool rtaoEnabled{false};
  bool rayTracedReflectionsEnabled{false};
  CameraLook camera{};
};

// Deterministic OpenRepublic presentation curve. Daylight is 06:00-18:00;
// elevation rises/falls as 2 + 68*sin(pi*fraction), yaw moves -90 to +90,
// warmth is 2600 K + 3400*sin(pi*fraction). Outside daylight elevation is 2,
// yaw is held at the nearest horizon endpoint, and warmth is 2600 K.
SceneLook LookAtTimeOfDay(double hours);

class SceneControlState {
public:
  explicit SceneControlState(SceneLook initial = {});
  std::string Apply(std::string_view requestJson);
  bool ConsumeDirty();
  SceneLook CurrentLook() const;
  std::uint64_t Revision() const;

private:
  mutable std::mutex mutex_;
  SceneLook look_;
  std::uint64_t revision_{0};
  bool dirty_{false};
};

} // namespace openrepublic::scenectl
