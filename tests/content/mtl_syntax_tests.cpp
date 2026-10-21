#include "openrepublic/content/mtl_syntax.h"

#include <iostream>
#include <string>
#include <vector>

using namespace openrepublic::content;

namespace {

int failures = 0;

void Check(bool condition, const std::string& label) {
    if (!condition) {
        std::cerr << "FAIL: " << label << '\n';
        ++failures;
    }
}

const std::string kValidSynthetic =
    "\r\n"
    "$SUBMATERIAL synthetic_building\r\n"
    "$TEXTURE 0 textures/synthetic_diffuse.dds\r\n"
    "$TEXTURE 1 textures/synthetic_second.dds\r\n"
    "$DIFFUSECOLOR 0.7 0.6 0.5 1\r\n"
    "$SPECULARCOLOR 0.1 0.2 0.3 1\r\n"
    "$AMBIENTCOLOR 0 0 0 1\r\n"
    "$SPECULARPOWER 12\r\n"
    "$SUBMATERIAL optional_power_omitted\r\n"
    "$TEXTURE_MTL 2 textures/synthetic_alternate.dds\r\n"
    "$END\r\n";

void ExpectFailure(const std::string& text, const std::string& label) {
    MtlDocument output;
    output.submaterials.push_back(MtlSubmaterial{"sentinel", {}, 77});
    output.terminated = true;
    output.endLine = 88;
    std::string error;
    const bool parsed = ParseMtlSyntax(text, output, &error);
    Check(!parsed && !error.empty(), label + " rejects with diagnostic");
    Check(output.submaterials.size() == 1 && output.submaterials[0].nameToken == "sentinel" &&
          output.submaterials[0].line == 77 && output.terminated && output.endLine == 88,
          label + " leaves caller output unchanged");
}

void TestValidSyntaxAndUnknownPreservation() {
    MtlDocument parsed;
    std::string error = "stale";
    Check(ParseMtlSyntax(kValidSynthetic, parsed, &error), "synthetic sibling sections parse");
    Check(error.empty(), "success clears optional diagnostic");
    Check(parsed.terminated && parsed.endLine == 11, "terminal directive line is recorded");
    Check(parsed.submaterials.size() == 2, "both sibling sections are retained");
    if (parsed.submaterials.size() != 2) return;

    const auto& first = parsed.submaterials[0];
    Check(first.nameToken == "synthetic_building" && first.line == 2,
          "section name token remains opaque and line is retained");
    Check(first.directives.size() == 6, "repeated texture and color directives retain order");
    if (first.directives.size() >= 2) {
        Check(first.directives[0].name == "$TEXTURE" &&
              first.directives[0].arguments == std::vector<std::string>({"0", "textures/synthetic_diffuse.dds"}) &&
              first.directives[0].kind == MtlDirectiveKind::Texture,
              "texture numeric token and symbolic path remain unchanged");
        Check(first.directives[1].arguments[0] == "1",
              "texture numeric argument is preserved without assigning slot meaning");
    }
    const auto& second = parsed.submaterials[1];
    Check(second.nameToken == "optional_power_omitted" && second.directives.size() == 1,
          "section without SPECULARPOWER is accepted");
    if (!second.directives.empty()) {
        Check(second.directives[0].name == "$TEXTURE_MTL" &&
              second.directives[0].kind == MtlDirectiveKind::Unknown &&
              second.directives[0].arguments == std::vector<std::string>({"2", "textures/synthetic_alternate.dds"}),
              "unrecognized directive and its tokens are preserved without path resolution");
    }
}

void TestMalformedSyntax() {
    ExpectFailure("$END\n", "terminator before any section");
    ExpectFailure("$SUBMATERIAL only\n$TEXTURE 0 x.dds\n", "missing terminal directive");
    ExpectFailure("$SUBMATERIAL\n$END\n", "missing submaterial argument");
    ExpectFailure("$SUBMATERIAL x\n$TEXTURE 0\n$END\n", "wrong texture argument count");
    ExpectFailure("$SUBMATERIAL x\n$TEXTURE NaN x.dds\n$END\n", "non-finite texture numeric token");
    ExpectFailure("$SUBMATERIAL x\n$TEXTURE 1e999 x.dds\n$END\n", "out-of-range texture numeric token");
    ExpectFailure("$SUBMATERIAL x\n$DIFFUSECOLOR 0 1 2\n$END\n", "wrong color argument count");
    ExpectFailure("$SUBMATERIAL x\n$SPECULARPOWER 1 2\n$END\n", "wrong power argument count");
    ExpectFailure("$SUBMATERIAL x\n$END extra\n", "terminator with arguments");
    ExpectFailure("$TEXTURE 0 x.dds\n$SUBMATERIAL x\n$END\n", "directive before first section");
    ExpectFailure("$SUBMATERIAL x\n$END\n$UNKNOWN after\n", "content after terminator");
    ExpectFailure("\xef\xbb\xbf$SUBMATERIAL x\n$END\n", "unobserved UTF-8 BOM");
    ExpectFailure(std::string("$SUBMATERIAL x\n\0$END\n", 21), "embedded NUL byte");
    ExpectFailure("# comment syntax was not observed\n$SUBMATERIAL x\n$END\n", "unobserved comment line");
}

void TestInputBounds() {
    std::string longLine = "$SUBMATERIAL x\n$UNKNOWN " + std::string(4096, 'a') + "\n$END\n";
    ExpectFailure(longLine, "overlong line");

    std::string longToken = "$SUBMATERIAL x\n$UNKNOWN " + std::string(2049, 'a') + "\n$END\n";
    ExpectFailure(longToken, "overlong token");

    std::string tooManyTokens = "$SUBMATERIAL x\n$UNKNOWN";
    for (std::size_t i = 0; i < 128; ++i) tooManyTokens += " a";
    tooManyTokens += "\n$END\n";
    ExpectFailure(tooManyTokens, "too many tokens on a line");

    std::string tooManySections;
    for (std::size_t i = 0; i < 4097; ++i) tooManySections += "$SUBMATERIAL x\n";
    tooManySections += "$END\n";
    ExpectFailure(tooManySections, "too many sibling sections");

    std::string tooManyDirectives = "$SUBMATERIAL x\n";
    for (std::size_t i = 0; i < 65536; ++i) tooManyDirectives += "$U\n";
    tooManyDirectives += "$END\n";
    ExpectFailure(tooManyDirectives, "too many directives");

    std::string tooLarge = "$SUBMATERIAL x\n$UNKNOWN " + std::string(4u * 1024u * 1024u, 'a');
    ExpectFailure(tooLarge, "input above the byte limit");
}

} // namespace

int main() {
    TestValidSyntaxAndUnknownPreservation();
    TestMalformedSyntax();
    TestInputBounds();
    if (failures != 0) {
        std::cerr << failures << " MTL syntax test(s) failed\n";
        return 1;
    }
    std::cout << "All MTL syntax tests passed\n";
    return 0;
}
