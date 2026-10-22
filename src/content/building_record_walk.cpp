#include "building_record_walk.h"

#include <initializer_list>
#include <limits>
#include <utility>
#include <string>
#include <utility>

namespace openrepublic::content::detail {
namespace {

constexpr std::size_t kMaximumInputBytes = 512u * 1024u * 1024u;
constexpr std::uint32_t kMaximumRecordCount = 65536;
constexpr std::uint32_t kMaximumNestedCount = 1u << 20;
constexpr std::size_t kFixedHeaderBytes = 0x548;
constexpr std::size_t kExtensionBytes = 400;

bool Fail(std::string* error, const std::string& message) {
    if (error != nullptr) *error = message;
    return false;
}

std::uint32_t ReadU32(std::string_view bytes, std::size_t offset) {
    const auto* p = reinterpret_cast<const unsigned char*>(bytes.data() + offset);
    return static_cast<std::uint32_t>(p[0]) |
           (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) |
           (static_cast<std::uint32_t>(p[3]) << 24);
}

std::int32_t ReadI32(std::string_view bytes, std::size_t offset) {
    const std::uint32_t raw = ReadU32(bytes, offset);
    if (raw <= static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()))
        return static_cast<std::int32_t>(raw);
    return static_cast<std::int32_t>(static_cast<std::int64_t>(raw) - 0x100000000ll);
}

class Reader {
public:
    explicit Reader(std::string_view bytes) : bytes_(bytes) {}

    std::size_t Position() const { return position_; }
    std::size_t Remaining() const { return bytes_.size() - position_; }
    void SetStage(std::string stage) { stage_ = std::move(stage); }
    const std::string& Error() const { return error_; }
    bool Reject(const std::string& message) {
        error_ = stage_ + ": " + message;
        return false;
    }

    bool Take(std::size_t count) {
        if (count > Remaining()) {
            error_ = stage_ + ": truncated stream at byte " + std::to_string(position_) +
                     " (need " + std::to_string(count) + ", have " +
                     std::to_string(Remaining()) + ")";
            return false;
        }
        position_ += count;
        return true;
    }

    bool Array(std::int32_t signedCount, std::size_t stride) {
        std::uint32_t count = 0;
        if (!Count(signedCount, count)) return false;
        if (stride != 0 && count > Remaining() / stride) {
            error_ = stage_ + ": array length exceeds remaining input";
            return false;
        }
        return Take(static_cast<std::size_t>(count) * stride);
    }

    bool Repeated(std::int32_t signedCount, std::size_t minimumBytes,
                  bool (*walkOne)(Reader&)) {
        std::uint32_t count = 0;
        if (!Count(signedCount, count)) return false;
        if (minimumBytes != 0 && count > Remaining() / minimumBytes) {
            error_ = stage_ + ": nested record count exceeds remaining input";
            return false;
        }
        for (std::uint32_t i = 0; i < count; ++i) {
            if (!walkOne(*this)) return false;
        }
        return true;
    }

    bool Count(std::int32_t signedCount, std::uint32_t& count) {
        if (signedCount < 0 || static_cast<std::uint32_t>(signedCount) > kMaximumNestedCount) {
            error_ = stage_ + ": count is negative or exceeds the supported bound";
            return false;
        }
        count = static_cast<std::uint32_t>(signedCount);
        return true;
    }

    bool Utf16(std::int32_t signedUnits) {
        std::uint32_t units = 0;
        if (!Count(signedUnits, units)) return false;
        if (units == std::numeric_limits<std::uint32_t>::max() ||
            units + 1 > Remaining() / 2) {
            error_ = stage_ + ": UTF-16 string length exceeds remaining input";
            return false;
        }
        const std::size_t bytes = (static_cast<std::size_t>(units) + 1) * 2;
        const std::size_t terminator = position_ + bytes - 2;
        if (bytes_[terminator] != '\0' || bytes_[terminator + 1] != '\0') {
            error_ = stage_ + ": UTF-16 terminator is missing";
            return false;
        }
        return Take(bytes);
    }

    bool ReadRaw(std::size_t count, std::string_view& value) {
        if (count > Remaining()) {
            error_ = stage_ + ": truncated fixed metadata";
            return false;
        }
        value = bytes_.substr(position_, count);
        position_ += count;
        return true;
    }

    bool ExactEnd() const { return position_ == bytes_.size(); }

private:
    std::string_view bytes_;
    std::size_t position_ = 0;
    std::string stage_ = "file header";
    std::string error_;
};

bool WalkNestedArrays(Reader& reader, std::initializer_list<std::pair<std::size_t, std::size_t>> fields,
                      std::string_view header) {
    for (const auto& field : fields) {
        if (!reader.Array(ReadI32(header, field.first), field.second)) return false;
    }
    return true;
}

bool WalkFirstHelper(Reader& reader) {
    std::string_view header;
    if (!reader.ReadRaw(32, header)) return false;
    return WalkNestedArrays(reader, {{0, 72}, {20, 188}, {24, 12}, {28, 12}}, header);
}

bool WalkSecondHelper(Reader& reader) {
    std::string_view header;
    if (!reader.ReadRaw(16, header)) return false;
    if (header[4] != '\0' || header[5] != '\0' || header[6] != '\0' || header[7] != '\0' ||
        header[8] != '\0' || header[9] != '\0' || header[10] != '\0' || header[11] != '\0' ||
        header[12] != '\0' || header[13] != '\0' || header[14] != '\0' || header[15] != '\0')
        return reader.Reject("reserved helper bytes are nonzero");
    if (!reader.Array(ReadI32(header, 0), 72)) return false;
    return true;
}

bool WalkThirdHelper(Reader& reader) { return reader.Take(60); }

bool WalkFourthHelper(Reader& reader) {
    std::string_view header;
    if (!reader.ReadRaw(44, header)) return false;
    return WalkNestedArrays(reader, {{16, 4}, {12, 8}, {24, 8}, {20, 4}, {28, 4},
                                     {32, 12}, {36, 4}, {40, 4}}, header);
}

bool WalkFifthHelper(Reader& reader) {
    std::string_view header;
    if (!reader.ReadRaw(36, header)) return false;
    return WalkNestedArrays(reader, {{4, 8}, {8, 8}, {12, 4}}, header);
}

bool WalkSixthHelper(Reader& reader) {
    std::string_view header;
    if (!reader.ReadRaw(24, header)) return false;
    return reader.Array(ReadI32(header, 8), 72);
}

bool WalkSeventhHelper(Reader& reader) {
    std::string_view header;
    if (!reader.ReadRaw(40, header)) return false;
    return reader.Array(ReadI32(header, 36), 72);
}

bool WalkFirstNestedSequence(Reader& reader) {
    std::string_view metadata;
    if (!reader.ReadRaw(28, metadata)) return false;
    const std::int32_t repeatedCount = ReadI32(metadata, 20);
    std::uint32_t boundedCount = 0;
    if (!reader.Count(repeatedCount, boundedCount)) return false;
    if (!reader.Take(0x2000)) return false;
    std::string_view countBytes;
    if (!reader.ReadRaw(4, countBytes)) return false;
    if (!reader.Array(ReadI32(countBytes, 0), 32)) return false;
    if (!reader.Array(static_cast<std::int32_t>(boundedCount), 72)) return false;
    return true;
}

bool WalkConditionalNestedSequence(Reader& reader) {
    std::string_view countBytes;
    if (!reader.ReadRaw(4, countBytes)) return false;
    if (!reader.Array(ReadI32(countBytes, 0), 32)) return false;
    if (!reader.ReadRaw(4, countBytes)) return false;
    if (!reader.Array(ReadI32(countBytes, 0), 16)) return false;
    return reader.Take(20);
}

bool WalkOneRecord(Reader& reader, std::uint32_t recordIndex, std::size_t& offset) {
    offset = reader.Position();
    reader.SetStage("record " + std::to_string(recordIndex) + " fixed header");
    std::string_view fixed;
    std::string_view extension;
    if (!reader.ReadRaw(kFixedHeaderBytes, fixed) || !reader.ReadRaw(kExtensionBytes, extension))
        return false;

    const auto hCount = [&](std::size_t at) { return ReadI32(fixed, at); };
    const auto eCount = [&](std::size_t at) { return ReadI32(extension, at); };
    const auto arrayH = [&](std::size_t at, std::size_t stride) {
        reader.SetStage("record " + std::to_string(recordIndex) + " fixed array");
        return reader.Array(hCount(at), stride);
    };
    const auto arrayE = [&](std::size_t at, std::size_t stride) {
        reader.SetStage("record " + std::to_string(recordIndex) + " extension array");
        return reader.Array(eCount(at), stride);
    };
    const auto repeated = [&](std::int32_t count, std::size_t minimumBytes,
                              bool (*walkOne)(Reader&), const char* label) {
        reader.SetStage("record " + std::to_string(recordIndex) + " " + label);
        return reader.Repeated(count, minimumBytes, walkOne);
    };

    reader.SetStage("record " + std::to_string(recordIndex) + " nested metadata");
    std::uint32_t firstCount = 0;
    if (!reader.Count(hCount(0x3f0), firstCount)) return false;
    constexpr std::size_t kMinimumFirstNestedBytes = 28 + 0x2000 + 4;
    if (firstCount > reader.Remaining() / kMinimumFirstNestedBytes)
        return reader.Reject("first nested stream count exceeds remaining input");
    for (std::uint32_t i = 0; i < firstCount; ++i) {
        if (!WalkFirstNestedSequence(reader)) return false;
    }

    for (const auto& field : {std::pair<std::size_t, std::size_t>{0x400, 188}, {0x404, 4},
                              {0x40c, 188}, {0x408, 4}})
        if (!arrayH(field.first, field.second)) return false;

    if (!repeated(hCount(0x418), 32, WalkFirstHelper, "nested helper group A")) return false;
    std::uint32_t definitionKind = 0;
    if (!reader.Count(eCount(0x1c), definitionKind)) return false;
    if (definitionKind == 0x20 || definitionKind == 0x21)
        if (!arrayH(0x418, 4)) return false;
    if (hCount(0x514) != 1)
        return reader.Reject("unsupported fixed-header flag");
    if (!repeated(hCount(0x418), 16, WalkSecondHelper, "nested helper group B")) return false;
    const auto packedBits = static_cast<unsigned char>(extension[0x97]);
    if (packedBits > 127) return reader.Reject("unsupported packed-field length");
    reader.SetStage("record " + std::to_string(recordIndex) + " packed field");
    if (!reader.Take(static_cast<std::size_t>(packedBits))) return false;

    if (!repeated(hCount(0x424), 60, WalkThirdHelper, "nested helper group C") ||
        !repeated(hCount(0x428), 60, WalkThirdHelper, "nested helper group D")) return false;
    for (const auto& field : {std::pair<std::size_t, std::size_t>{0x42c, 32}, {0x450, 32},
                              {0x478, 4}, {0x47c, 4}, {0x480, 4}, {0x484, 4}})
        if (!arrayH(field.first, field.second)) return false;
    if (!arrayE(0xe8, 4) || !arrayH(0x488, 4)) return false;
    if (!repeated(hCount(0x48c), 44, WalkFourthHelper, "nested helper group E") ||
        !repeated(hCount(0x50c), 36, WalkFifthHelper, "nested helper group F")) return false;
    for (const auto& field : {std::pair<std::size_t, std::size_t>{0x4c8, 12}, {0x504, 12},
                              {0x4cc, 28}, {0x4dc, 12}, {0x4e0, 12}, {0x4e4, 12}})
        if (!arrayH(field.first, field.second)) return false;
    if (!arrayE(0x2c, 12)) return false;
    for (const auto& field : {std::pair<std::size_t, std::size_t>{0x4e8, 8}, {0x4ec, 8},
                              {0x414, 4}, {0x4f4, 4}})
        if (!arrayH(field.first, field.second)) return false;
    if (!arrayE(0xb4, 4)) return false;
    if (hCount(0x4f8) < 0 || static_cast<std::uint32_t>(hCount(0x4f8)) > kMaximumNestedCount)
        return reader.Reject("conditional nested sequence count is negative or exceeds the supported bound");
    if (hCount(0x4f8) > 0) {
        reader.SetStage("record " + std::to_string(recordIndex) + " conditional nested sequence");
        if (!WalkConditionalNestedSequence(reader)) return false;
    }
    if (!arrayH(0x524, 4)) return false;
    const std::int32_t nameFlag = hCount(0x52c);
    if (nameFlag != 0 && nameFlag != 1) return reader.Reject("unsupported fixed-header name flag");
    if (nameFlag && !reader.Take(256)) return false;
    const std::int32_t helperFlag = hCount(0x530);
    if (helperFlag != 0 && helperFlag != 1) return reader.Reject("unsupported fixed-header helper flag");
    if (!repeated(helperFlag * 4, 24, WalkSixthHelper, "conditional helper group")) return false;
    std::uint32_t repeatedHelpers = 0;
    reader.SetStage("record " + std::to_string(recordIndex) + " repeated helper group");
    if (!reader.Count(hCount(0x534), repeatedHelpers)) return false;
    for (std::uint32_t i = 0; i < repeatedHelpers; ++i) {
        if (!reader.Take(4) || !repeated(2, 24, WalkSixthHelper, "paired nested helpers")) return false;
    }
    if (!arrayH(0x540, 4)) return false;
    reader.SetStage("record " + std::to_string(recordIndex) + " fixed trailing block");
    if (!reader.Take(128)) return false;
    const std::int32_t stringFlag = hCount(0x538);
    if (stringFlag < 0 || static_cast<std::uint32_t>(stringFlag) > kMaximumNestedCount)
        return reader.Reject("UTF-16 string length is negative or exceeds the supported bound");
    if (stringFlag != 0) {
        reader.SetStage("record " + std::to_string(recordIndex) + " UTF-16 string");
        if (!reader.Utf16(stringFlag)) return false;
    }
    if (!arrayH(0x544, 4) || !arrayE(0x0c, 136)) return false;
    if (eCount(0x18) < 0 || eCount(0x18) > 9)
        return reader.Reject("extension scalar count is outside the supported range");
    if (!arrayE(0x18, 4) || !arrayE(0x38, 16) || !arrayE(0x3c, 192) ||
        !arrayE(0x40, 4) || !arrayE(0x4c, 4)) return false;
    std::uint32_t optionalBlockFlag = 0;
    if (!reader.Count(eCount(0x98), optionalBlockFlag)) return false;
    if (optionalBlockFlag != 0 && !reader.Take(128)) return false;
    if (!repeated(eCount(0xec), 40, WalkSeventhHelper, "extension helper group") ||
        !arrayE(0xf0, 4) || !arrayE(0x108, 16)) return false;
    return true;
}

} // namespace

bool WalkBuildingRecordOffsets(std::string_view bytes, std::uint32_t formatVersion,
                               std::vector<std::size_t>& offsets, std::string* error) {
    if (formatVersion != 121 && formatVersion != 124)
        return Fail(error, "unsupported placement format version");
    if (bytes.size() > kMaximumInputBytes)
        return Fail(error, "placement input exceeds the 512 MiB limit");
    if (bytes.size() < 4)
        return Fail(error, "placement input is shorter than its count header");

    const std::int32_t signedRecordCount = ReadI32(bytes, 0);
    if (signedRecordCount < 0 || static_cast<std::uint32_t>(signedRecordCount) > kMaximumRecordCount)
        return Fail(error, "placement record count exceeds the supported limit");
    const std::uint32_t recordCount = static_cast<std::uint32_t>(signedRecordCount);
    if (recordCount > (bytes.size() - 4) / (kFixedHeaderBytes + kExtensionBytes + 128))
        return Fail(error, "placement record count exceeds the minimum available record bytes");

    Reader reader(bytes);
    if (!reader.Take(4)) return Fail(error, reader.Error());
    std::vector<std::size_t> found;
    found.reserve(recordCount);
    for (std::uint32_t i = 0; i < recordCount; ++i) {
        std::size_t offset = 0;
        if (!WalkOneRecord(reader, i, offset))
            return Fail(error, reader.Error().empty() ? "unsupported placement record field" : reader.Error());
        found.push_back(offset);
    }
    if (!reader.ExactEnd())
        return Fail(error, "placement records do not consume the input exactly");
    offsets = std::move(found);
    if (error != nullptr) error->clear();
    return true;
}

} // namespace openrepublic::content::detail
