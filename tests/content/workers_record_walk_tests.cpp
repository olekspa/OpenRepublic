#include "workers_record_walk.h"

#include <iostream>
#include <string>
#include <vector>

using openrepublic::content::detail::WalkWorkerRecordOffsets;
using openrepublic::content::detail::WorkerRecordSpan;

namespace {

int failures = 0;

void Check(bool condition, const char* label) {
    if (!condition) {
        std::cerr << "FAIL: " << label << '\n';
        ++failures;
    }
}

void PutU32(std::string& bytes, std::size_t offset, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i)
        bytes[offset + i] = static_cast<char>((value >> (i * 8)) & 0xff);
}

std::string Record(bool nineByteTail, bool hasLargeTail) {
    std::string bytes(0x728, '\0');
    if (nineByteTail) bytes[0x700] = 1;
    if (hasLargeTail) bytes[0x70c] = 1;
    bytes.append(nineByteTail ? 9 : 0, '\x11');
    bytes.append(hasLargeTail ? 128 : 0, '\x22');
    return bytes;
}

void TestEmptyAndVariableRecords() {
    std::string empty(4, '\0');
    std::vector<WorkerRecordSpan> spans;
    std::string error;
    Check(WalkWorkerRecordOffsets(empty, spans, &error) && spans.empty(), "empty count");

    std::string bytes(4, '\0');
    PutU32(bytes, 0, 3);
    bytes += Record(false, false);
    bytes += Record(true, false);
    bytes += Record(false, true);
    Check(WalkWorkerRecordOffsets(bytes, spans, &error), "walk optional tails");
    Check(spans.size() == 3, "record count");
    if (spans.size() == 3) {
        Check(spans[0].offset == 4 && spans[0].size == 0x728, "base record span");
        Check(spans[1].offset == 4 + 0x728 && spans[1].size == 0x728 + 9,
              "nine-byte optional span");
        Check(spans[2].offset == 4 + 2 * 0x728 + 9 && spans[2].size == 0x728 + 128,
              "128-byte optional span");
    }
}

void TestFailClosedAndTransactional() {
    std::vector<WorkerRecordSpan> spans{{12, 34}};
    std::string error;
    Check(!WalkWorkerRecordOffsets("\0\0\0", spans, &error), "truncated count rejected");
    Check(spans.size() == 1 && spans[0].offset == 12, "output unchanged on count failure");

    std::string bytes(4, '\0');
    PutU32(bytes, 0, 1);
    bytes += Record(false, false);
    bytes.pop_back();
    Check(!WalkWorkerRecordOffsets(bytes, spans, &error), "truncated record rejected");
    Check(spans.size() == 1 && spans[0].offset == 12, "output unchanged on record failure");

    bytes.push_back('\0');
    bytes.push_back('\0');
    Check(!WalkWorkerRecordOffsets(bytes, spans, &error), "trailing bytes rejected");

    std::string hugeCount(4, '\0');
    PutU32(hugeCount, 0, 0xffffffffu);
    Check(!WalkWorkerRecordOffsets(hugeCount, spans, &error), "unbounded count rejected");
}

} // namespace

int main() {
    TestEmptyAndVariableRecords();
    TestFailClosedAndTransactional();
    return failures == 0 ? 0 : 1;
}
