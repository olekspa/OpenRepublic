#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace openrepublic::content::detail {

// Strictly walks one explicitly supported placement-record profile and returns
// the byte offset of each fixed record header. This validates stream lengths,
// bounded counts, supported flags, and exact EOF; it does not interpret opaque
// record fields or establish placement identity semantics.
bool WalkBuildingRecordOffsets(std::string_view bytes, std::uint32_t formatVersion,
                               std::vector<std::size_t>& offsets,
                               std::string* error = nullptr);

} // namespace openrepublic::content::detail
