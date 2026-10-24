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

// The serializer's two in-block predicates select variable-length tails.
// These names describe only the framing gates, not the purpose of either
// optional payload.
struct WorkerRecordTailFlags {
    bool hasNineByteTail = false; // record byte +0x700 != 0
    bool has128ByteTail = false;  // record byte +0x70c != 0
};

// Reads the verified current-writer tail gates from one complete 0x718-byte
// base block. Returns false for a truncated block and leaves output unchanged.
bool ReadWorkerRecordTailFlags(std::string_view record,
                               WorkerRecordTailFlags& output,
                               std::string* error = nullptr);

// Walks the current workers.bin schema only (reader internal version > 0x71):
// u32 count, then 0x718 + 16 bytes per record, plus tails selected by bytes
// at record offsets 0x700 and 0x70c. Output is transactional on failure.
bool WalkWorkerRecordOffsets(std::string_view bytes,
                             std::vector<WorkerRecordSpan>& records,
                             std::string* error = nullptr);

} // namespace openrepublic::content::detail
