#include "openrepublic/economy/checkpoint.h"

#include <limits>
#include <string>
#include <utility>

namespace openrepublic::economy {
namespace {
constexpr std::string_view kCheckpointHeader = "OPENREPUBLIC_ECONOMY_CHECKPOINT 1";
constexpr std::string_view kCatalogLengthLabel = "CATALOG_BYTES ";
constexpr std::string_view kWorldLengthLabel = "WORLD_BYTES ";

struct FramedCheckpoint {
    std::string_view catalog;
    std::string_view world;
};

void SetError(ParseError &error, ParseErrorCode code, std::uint32_t line, std::string message) {
    error.code = code;
    error.line = line;
    error.message = std::move(message);
}

bool ReadLine(std::string_view text, std::size_t &offset, std::string_view &line) {
    const auto newline = text.find('\n', offset);
    if (newline == std::string_view::npos)
        return false;
    line = text.substr(offset, newline - offset);
    offset = newline + 1;
    return true;
}

bool ParseLengthLine(std::string_view line, std::string_view label, std::size_t maximum,
                     std::size_t &value, ParseError &error, std::uint32_t lineNumber) {
    if (line.substr(0, label.size()) != label) {
        SetError(error, ParseErrorCode::InvalidHeader, lineNumber, "invalid checkpoint length header");
        return false;
    }
    const auto digits = line.substr(label.size());
    if (digits.empty() || (digits.size() > 1 && digits.front() == '0')) {
        SetError(error, ParseErrorCode::InvalidNumber, lineNumber, "checkpoint length is not canonical decimal");
        return false;
    }
    std::size_t parsed = 0;
    for (const char ch : digits) {
        if (ch < '0' || ch > '9') {
            SetError(error, ParseErrorCode::InvalidNumber, lineNumber,
                     "checkpoint length is not canonical decimal");
            return false;
        }
        const auto digit = static_cast<std::size_t>(ch - '0');
        if (parsed > (std::numeric_limits<std::size_t>::max() - digit) / 10) {
            SetError(error, ParseErrorCode::InvalidNumber, lineNumber, "checkpoint length overflows size range");
            return false;
        }
        parsed = parsed * 10 + digit;
    }
    if (parsed == 0) {
        SetError(error, ParseErrorCode::InvalidNumber, lineNumber, "checkpoint lengths must be nonzero");
        return false;
    }
    if (parsed > maximum) {
        SetError(error, ParseErrorCode::LimitExceeded, lineNumber, "checkpoint section exceeds its byte limit");
        return false;
    }
    value = parsed;
    return true;
}

bool Frame(std::string_view text, FramedCheckpoint &framed, ParseError &error) {
    error = {};
    if (text.empty()) {
        SetError(error, ParseErrorCode::EmptyInput, 0, "empty checkpoint");
        return false;
    }
    if (text.size() > MaxCheckpointBytes) {
        SetError(error, ParseErrorCode::LimitExceeded, 0, "checkpoint exceeds its byte limit");
        return false;
    }

    std::size_t offset = 0;
    std::string_view line;
    if (!ReadLine(text, offset, line) || line != kCheckpointHeader) {
        SetError(error, ParseErrorCode::InvalidHeader, 1, "invalid checkpoint header");
        return false;
    }
    if (!ReadLine(text, offset, line)) {
        SetError(error, ParseErrorCode::InvalidHeader, 2, "missing catalog length header");
        return false;
    }
    std::size_t catalogBytes = 0;
    if (!ParseLengthLine(line, kCatalogLengthLabel, MaxCheckpointCatalogBytes, catalogBytes, error, 2))
        return false;
    if (!ReadLine(text, offset, line)) {
        SetError(error, ParseErrorCode::InvalidHeader, 3, "missing world length header");
        return false;
    }
    std::size_t worldBytes = 0;
    if (!ParseLengthLine(line, kWorldLengthLabel, MaxWorldSaveBytes, worldBytes, error, 3))
        return false;

    if (catalogBytes > std::numeric_limits<std::size_t>::max() - worldBytes ||
        offset > std::numeric_limits<std::size_t>::max() - catalogBytes - worldBytes ||
        offset + catalogBytes + worldBytes != text.size()) {
        SetError(error, ParseErrorCode::InvalidHeader, 0,
                 "checkpoint section lengths do not exactly match the remaining bytes");
        return false;
    }
    framed.catalog = text.substr(offset, catalogBytes);
    framed.world = text.substr(offset + catalogBytes, worldBytes);
    return true;
}

bool ParseCanonicalCatalog(std::string_view text, Catalog &catalog, ParseError &error) {
    if (!ParseCatalogText(text, catalog, error))
        return false;
    if (WriteCatalogText(catalog) != text) {
        SetError(error, ParseErrorCode::CatalogInvalid, 0, "checkpoint catalog is not canonical text");
        return false;
    }
    return true;
}

} // namespace

bool SaveCheckpoint(const World &world, std::string &outText, ParseError &error) {
    error = {};
    const std::string catalogText = WriteCatalogText(world.GetCatalog());
    if (catalogText.empty() || catalogText.size() > MaxCheckpointCatalogBytes) {
        SetError(error, ParseErrorCode::LimitExceeded, 0, "catalog text is empty or exceeds its byte limit");
        return false;
    }
    Catalog parsedCatalog;
    if (!ParseCanonicalCatalog(catalogText, parsedCatalog, error))
        return false;

    const std::string worldText = SaveWorld(world);
    if (worldText.empty()) {
        SetError(error, ParseErrorCode::LimitExceeded, 0, "world save is empty or exceeds its byte limit");
        return false;
    }
    if (worldText.size() > MaxWorldSaveBytes ||
        catalogText.size() > std::numeric_limits<std::size_t>::max() - worldText.size()) {
        SetError(error, ParseErrorCode::LimitExceeded, 0, "checkpoint section exceeds its byte limit");
        return false;
    }

    std::string result;
    result.reserve(kCheckpointHeader.size() + 1 + 14 + 1 + 12 + 1 + catalogText.size() + worldText.size());
    result.append(kCheckpointHeader);
    result.push_back('\n');
    result.append(kCatalogLengthLabel);
    result.append(std::to_string(catalogText.size()));
    result.push_back('\n');
    result.append(kWorldLengthLabel);
    result.append(std::to_string(worldText.size()));
    result.push_back('\n');
    result.append(catalogText);
    result.append(worldText);
    if (result.size() > MaxCheckpointBytes) {
        SetError(error, ParseErrorCode::LimitExceeded, 0, "checkpoint exceeds its byte limit");
        return false;
    }
    outText = std::move(result);
    return true;
}

bool RestoreCheckpoint(std::string_view text, RestoredCheckpoint &out, ParseError &error) {
    FramedCheckpoint framed;
    if (!Frame(text, framed, error))
        return false;

    auto catalog = std::make_unique<Catalog>();
    if (!ParseCanonicalCatalog(framed.catalog, *catalog, error))
        return false;
    auto world = std::make_unique<World>(*catalog, 1, 1);
    if (!LoadWorld(framed.world, *catalog, *world, error))
        return false;

    RestoredCheckpoint restored;
    restored.catalog = std::move(catalog);
    restored.world = std::move(world);
    out.catalog.swap(restored.catalog);
    out.world.swap(restored.world);
    error = {};
    return true;
}

bool LoadCheckpoint(std::string_view text, const Catalog &expected, World &out, ParseError &error) {
    FramedCheckpoint framed;
    if (!Frame(text, framed, error))
        return false;
    Catalog embedded;
    if (!ParseCanonicalCatalog(framed.catalog, embedded, error))
        return false;
    if (WriteCatalogText(expected) != framed.catalog) {
        SetError(error, ParseErrorCode::CatalogInvalid, 0,
                 "checkpoint catalog does not exactly match the expected catalog");
        return false;
    }

    World restored(expected, 1, 1);
    if (!LoadWorld(framed.world, expected, restored, error))
        return false;
    out = std::move(restored);
    error = {};
    return true;
}

} // namespace openrepublic::economy
