#include "agent_protocol.h"

#include <cstdlib>
#include <iostream>
#include <string>

#include "json.hpp"

using namespace openrepublic::agent_host;
using Json = nlohmann::json;

int main() {
    int failures = 0;
    auto check = [&failures](bool condition, const char* message) {
        if (condition) return;
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    };
    auto request = [](const char* id, const char* method) {
        return Json{{"id", id}, {"protocolVersion", ProtocolVersion}, {"method", method}}.dump();
    };
    auto parseResponse = [&check](const std::string& response, const char* message) {
        try { return Json::parse(response); }
        catch (...) {
            check(false, message);
            return Json::object();
        }
    };

    Dispatcher observer;
    const auto hello = parseResponse(observer.HandleLine(request("1", "hello")), "hello returns JSON");
    check(hello.value("ok", false) && hello["result"].value("role", "") == "observer",
        "default role is observer");
    const auto before = parseResponse(observer.HandleLine(request("2", "world")), "world query returns JSON");
    const auto beforeTick = before["result"]["world"]["tick"].get<std::string>();
    const auto mutation = Json{{"id", "3"}, {"protocolVersion", ProtocolVersion}, {"method", "step"}, {"ticks", 1}}.dump();
    const auto denied = parseResponse(observer.HandleLine(mutation), "observer mutation denial returns JSON");
    check(denied["ok"] && denied["result"]["errorCode"] == "permission_denied",
        "observer cannot advance simulation");

    const std::vector<std::string> invalidRequests{
        "not-json",
        "{\"id\":\"dup\",\"id\":\"second\",\"protocolVersion\":0,\"method\":\"step\",\"ticks\":1}",
        "{\"id\":\"future\",\"protocolVersion\":1,\"method\":\"step\",\"ticks\":1}",
        "{\"id\":\"extra\",\"protocolVersion\":0,\"method\":\"step\",\"ticks\":1,\"unexpected\":true}",
        "{\"id\":\"bad\",\"protocolVersion\":0,\"method\":\"step\",\"ticks\":1.5}",
        "{\"id\":\"bad\",\"protocolVersion\":0,\"method\":\"step\",\"ticks\":\"1\"}"
    };
    for (const auto& invalid : invalidRequests) {
        const auto response = parseResponse(observer.HandleLine(invalid), "invalid request returns JSON");
        check(!response.value("ok", true), "malformed, duplicate-key, wrong-version, unknown-field and wrong-type requests are rejected");
    }
    std::string deep = "{\"id\":\"deep\",\"protocolVersion\":0,\"method\":\"world\",\"x\":";
    deep.append(MaxJsonNesting + 4, '[');
    deep += "0";
    deep.append(MaxJsonNesting + 4, ']');
    deep += "}";
    const auto deepResponse = parseResponse(observer.HandleLine(deep), "deep JSON rejection returns JSON");
    check(!deepResponse.value("ok", true) && deepResponse["error"]["code"] == "json_too_deep",
        "excessive JSON nesting is bounded and rejected");

    std::string complex = "{\"id\":\"complex\",\"protocolVersion\":0,\"method\":\"world\",\"x\":[";
    for (std::size_t i = 0; i < MaxJsonNodes; ++i) {
        if (i) complex += ',';
        complex += '0';
    }
    complex += "]}";
    const auto complexResponse = parseResponse(observer.HandleLine(complex), "complex JSON rejection returns JSON");
    check(!complexResponse.value("ok", true) && complexResponse["error"]["code"] == "json_too_complex",
        "excessive JSON node count is bounded and rejected");

    const auto tooLarge = observer.HandleLine(std::string(MaxRequestLineBytes + 1, ' '));
    const auto largeResponse = parseResponse(tooLarge, "oversized request rejection returns JSON");
    check(!largeResponse.value("ok", true) && largeResponse["error"]["code"] == "request_too_large",
        "oversized requests are rejected before parsing");
    const auto after = parseResponse(observer.HandleLine(request("4", "world")), "world query after invalid input returns JSON");
    check(after["result"]["world"]["tick"] == beforeTick &&
        after["result"]["world"]["revision"] == before["result"]["world"]["revision"],
        "invalid protocol messages do not mutate tick, revision, or world state");

    Dispatcher controller(Role::Controller);
    const auto capabilities = parseResponse(controller.HandleLine(request("5", "capabilities")), "capabilities returns JSON");
    check(capabilities["ok"] && capabilities["result"]["actions"].size() == 8,
        "controller discovers the complete current API action set");
    const auto initial = parseResponse(controller.HandleLine(request("6", "world")), "controller world returns JSON");
    const auto initialRevision = initial["result"]["world"]["revision"].get<std::string>();
    const auto wrongRevision = Json{{"id", "7"}, {"protocolVersion", ProtocolVersion}, {"method", "place_storage"},
        {"revision", "9999999999999999999"}, {"x", 5}, {"z", 0}}.dump();
    const auto stale = parseResponse(controller.HandleLine(wrongRevision), "stale revision returns JSON");
    check(stale["ok"] && stale["result"]["errorCode"] == "stale_revision",
        "mutation with stale revision is rejected by the authoritative Agent API");
    const auto current = parseResponse(controller.HandleLine(request("8", "world")), "world query after stale submit returns JSON");
    check(current["result"]["world"]["revision"] == initialRevision &&
        current["result"]["world"]["entities"].size() == initial["result"]["world"]["entities"].size(),
        "stale mutation does not change revision or world entities");

    if (failures) return EXIT_FAILURE;
    std::cout << "All OpenRepublic Agent Protocol checks passed.\n";
    return EXIT_SUCCESS;
}
