#include "openrepublic/content/building_definition.h"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <utility>

namespace openrepublic::content {
namespace {

constexpr std::size_t kMaxLineBytes = 4096;
constexpr std::size_t kMaxTokenBytes = 2048;
constexpr std::size_t kMaxTokensPerLine = 128;

bool Fail(std::string* error, const std::string& message) {
    if (error) *error = message;
    return false;
}

bool IsWhitespace(char value) {
    return value == ' ' || value == '\t' || value == '\v' || value == '\f';
}

bool Tokenize(const std::string& line, std::vector<std::string>& tokens) {
    tokens.clear();
    std::size_t cursor = 0;
    while (cursor < line.size()) {
        while (cursor < line.size() && IsWhitespace(line[cursor])) ++cursor;
        if (cursor == line.size()) break;
        const std::size_t start = cursor;
        while (cursor < line.size() && !IsWhitespace(line[cursor])) ++cursor;
        if (cursor - start > kMaxTokenBytes || tokens.size() == kMaxTokensPerLine) return false;
        tokens.emplace_back(line.data() + start, cursor - start);
    }
    return true;
}

bool ParseFinite(const std::string& token, double& value) {
    if (token.empty()) return false;
    errno = 0;
    char* end = nullptr;
    value = std::strtod(token.c_str(), &end);
    return end == token.c_str() + token.size() && errno != ERANGE && std::isfinite(value);
}

bool StartsWith(const std::string& text, const char* prefix) {
    return text.rfind(prefix, 0) == 0;
}

bool IsElectricalConnection(const std::string& name) {
    return name == "$CONNECTION_ELETRIC_HIGH_INPUT" || name == "$CONNECTION_ELETRIC_HIGH_OUTPUT" ||
           name == "$CONNECTION_ELETRIC_LOW_INPUT" || name == "$CONNECTION_ELETRIC_LOW_OUTPUT" ||
           name == "$CONNECTION_ELETRIC_HIGH_INPUT_ONLY" ||
           name == "$CONNECTION_ELETRIC_HIGH_OUTPUT_ONLY" ||
           name == "$CONNECTION_ELETRIC_LOW_INPUT_ONLY" ||
           name == "$CONNECTION_ELETRIC_LOW_OUTPUT_ONLY";
}

bool ParseElectricalPoint(const std::string& line, std::array<double, 3>& point) {
    std::size_t cursor = 0;
    for (double& coordinate : point) {
        while (cursor < line.size() && IsWhitespace(line[cursor])) ++cursor;
        if (cursor == line.size()) return false;

        errno = 0;
        char* end = nullptr;
        coordinate = std::strtod(line.c_str() + cursor, &end);
        if (end == line.c_str() + cursor || errno == ERANGE || !std::isfinite(coordinate)) return false;
        cursor = static_cast<std::size_t>(end - line.c_str());
    }
    while (cursor < line.size() && IsWhitespace(line[cursor])) ++cursor;
    return cursor == line.size();
}

void AddDiagnostic(BuildingElectricalConnection& connection, const std::string& message) {
    if (!connection.diagnostic.empty()) connection.diagnostic += "; ";
    connection.diagnostic += message;
}

} // namespace

bool ParseBuildingDefinition(const std::string& text, BuildingDefinition& output, std::string* error) {
    if (text.size() > MaxBuildingDefinitionBytes)
        return Fail(error, "building definition exceeds the 4 MiB limit");
    if (text.find('\0') != std::string::npos)
        return Fail(error, "building definition contains an embedded NUL byte");

    BuildingDefinition candidate;
    std::size_t directiveCount = 0;
    std::size_t lineNumber = 0;
    std::optional<std::size_t> activeConnection;
    std::size_t connectionRows = 0;
    std::istringstream input(text);
    std::string line;
    std::vector<std::string> tokens;
    const auto where = [&]() { return " at line " + std::to_string(lineNumber); };

    while (std::getline(input, line)) {
        ++lineNumber;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.size() > kMaxLineBytes) return Fail(error, "line exceeds the 4096-byte limit" + where());
        const auto comment = line.find("//");
        if (comment != std::string::npos) line.erase(comment);

        std::size_t first = 0;
        while (first < line.size() && IsWhitespace(line[first])) ++first;
        if (first == line.size()) continue;
        if (activeConnection && line[first] != '$') {
            ++candidate.ignoredLines;
            auto& connection = candidate.electricalConnections[*activeConnection];
            std::array<double, 3> point{};
            if (ParseElectricalPoint(line, point)) {
                connection.points[connectionRows] = point;
            } else {
                AddDiagnostic(connection, "malformed coordinate row at line " + std::to_string(lineNumber));
            }
            ++connectionRows;
            if (connectionRows == 2) {
                connection.valid = connection.diagnostic.empty() && connection.points[0].has_value() &&
                                   connection.points[1].has_value();
                activeConnection.reset();
                connectionRows = 0;
            }
            continue;
        }
        if (!Tokenize(line, tokens)) return Fail(error, "line exceeds token limits" + where());
        if (tokens.empty()) continue;
        const std::string& name = tokens.front();
        if (name.front() != '$') {
            ++candidate.ignoredLines;
            continue;
        }
        if (activeConnection) {
            auto& connection = candidate.electricalConnections[*activeConnection];
            AddDiagnostic(connection, "expected 2 coordinate rows before directive at line " +
                                         std::to_string(lineNumber));
            activeConnection.reset();
            connectionRows = 0;
        }
        if (++directiveCount > MaxBuildingDefinitionDirectives)
            return Fail(error, "building definition exceeds the directive limit");
        const std::vector<std::string> args(tokens.begin() + 1, tokens.end());

        double number = 0.0;
        if (name == "$NAME") {
            if (args.size() != 1) return Fail(error, "malformed $NAME" + where());
            candidate.nameId = args[0];
        } else if (StartsWith(name, "$TYPE_") && name.size() > 6 && args.empty()) {
            candidate.typeFlags.push_back(name.substr(6));
        } else if (name == "$WORKERS_NEEDED" || name == "$WORKING_VEHICLES_NEEDED") {
            if (args.size() != 1 || !ParseFinite(args[0], number))
                return Fail(error, "malformed " + name + where());
            (name == "$WORKERS_NEEDED" ? candidate.workersNeeded : candidate.workingVehiclesNeeded) = number;
        } else if (name == "$PRODUCTION" || name == "$CONSUMPTION" || name == "$CONSUMPTION_PER_SECOND") {
            if (args.size() < 2 || !ParseFinite(args[1], number))
                return Fail(error, "malformed " + name + where());
            BuildingResourceRate rate{args[0], number, lineNumber,
                                      candidate.workersNeeded && *candidate.workersNeeded > 0.0};
            auto& target = name == "$PRODUCTION" ? candidate.production
                         : name == "$CONSUMPTION" ? candidate.consumption
                                                  : candidate.consumptionPerSecond;
            target.push_back(std::move(rate));
        } else if (IsElectricalConnection(name)) {
            BuildingElectricalConnection connection;
            connection.directive = name;
            connection.line = lineNumber;
            if (!args.empty()) AddDiagnostic(connection, "electrical directive must not have inline arguments");
            candidate.other.push_back({name, args, lineNumber});
            candidate.electricalConnections.push_back(std::move(connection));
            activeConnection = candidate.electricalConnections.size() - 1;
            connectionRows = 0;
        } else if (StartsWith(name, "$STORAGE") && (args.size() == 2 || args.size() == 3) &&
                   ParseFinite(args[1], number)) {
            candidate.storage.push_back({name, args[0], number, args.size() == 3 ? args[2] : std::string(),
                                         lineNumber});
        } else {
            candidate.other.push_back({name, args, lineNumber});
        }
    }

    if (activeConnection) {
        auto& connection = candidate.electricalConnections[*activeConnection];
        AddDiagnostic(connection, "expected 2 coordinate rows; found " + std::to_string(connectionRows));
    }

    output = std::move(candidate);
    if (error) error->clear();
    return true;
}

} // namespace openrepublic::content
