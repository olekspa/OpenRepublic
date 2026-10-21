#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace openrepublic::content {

// These names describe syntax observed in a bounded local sample only. They do
// not assign game-specific meaning to numeric texture arguments or colors.
enum class MtlDirectiveKind {
    Unknown,
    Texture,
    DiffuseColor,
    SpecularColor,
    AmbientColor,
    SpecularPower,
};

struct MtlDirective {
    std::string name;
    std::vector<std::string> arguments;
    MtlDirectiveKind kind = MtlDirectiveKind::Unknown;
    std::size_t line = 0;
};

struct MtlSubmaterial {
    // Kept as an opaque token; no name or material semantics are inferred.
    std::string nameToken;
    std::vector<MtlDirective> directives;
    std::size_t line = 0;
};

struct MtlDocument {
    std::vector<MtlSubmaterial> submaterials;
    bool terminated = false;
    std::size_t endLine = 0;
};

// Parses a deliberately narrow, bounded text profile seen in three local MTL
// samples. It recognizes only directive names and argument shapes; values are
// retained as strings and numeric tokens are checked only for finite syntax.
// Texture paths are not resolved or loaded, and unknown directives inside a
// submaterial are preserved in order. This is not a claim of general MTL or
// WRSR material compatibility. On failure output is unchanged.
bool ParseMtlSyntax(const std::string& text, MtlDocument& output,
                    std::string* error = nullptr);

} // namespace openrepublic::content
