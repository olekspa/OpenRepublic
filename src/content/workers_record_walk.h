#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace openrepublic::content::detail {

// Raw span for one current-schema worker record. No worker field semantics are
// assigned here; this is a bounded framing helper only.
struct WorkerRecordSpan {
    std::size_t offset = 0;
    std::size_t size = 0;
};

// Walks the current workers.bin schema only (reader internal version > 0x71):
// u32 count, then 0x718 + 16 bytes per record, plus tails selected by bytes
// at record offsets 0x700 and 0x70c. Output is transactional on failure.
bool WalkWorkerRecordOffsets(std::string_view bytes,
                             std::vector<WorkerRecordSpan>& records,
                             std::string* error = nullptr);

} // namespace openrepublic::content::detail
