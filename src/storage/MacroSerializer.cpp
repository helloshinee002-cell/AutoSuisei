#include "MacroSerializer.h"

#include <chrono>
#include <stdexcept>
#include <string_view>
#include <variant>

#include <nlohmann/json.hpp>

namespace autopilot::storage {

namespace {

constexpr int kSchemaVersion = 1;

using json = nlohmann::json;

std::string kindToString(core::ActionKind k) {
    using K = core::ActionKind;
    switch (k) {
        case K::KeyDown: return "KeyDown";
        case K::KeyUp: return "KeyUp";
        case K::MouseMove: return "MouseMove";
        case K::MouseClick: return "MouseClick";
        case K::MouseScroll: return "MouseScroll";
        case K::WindowFocus: return "WindowFocus";
        case K::Delay: return "Delay";
        case K::Screenshot: return "Screenshot";
        case K::OcrCapture: return "OcrCapture";
        case K::WebCdpCommand: return "WebCdpCommand";
        case K::LuaScript: return "LuaScript";
        case K::Conditional: return "Conditional";
        case K::Loop: return "Loop";
    }
    throw std::runtime_error("kindToString: unknown ActionKind");
}

core::ActionKind kindFromString(std::string_view s) {
    using K = core::ActionKind;
    if (s == "KeyDown") return K::KeyDown;
    if (s == "KeyUp") return K::KeyUp;
    if (s == "MouseMove") return K::MouseMove;
    if (s == "MouseClick") return K::MouseClick;
    if (s == "MouseScroll") return K::MouseScroll;
    if (s == "WindowFocus") return K::WindowFocus;
    if (s == "Delay") return K::Delay;
    if (s == "Screenshot") return K::Screenshot;
    if (s == "OcrCapture") return K::OcrCapture;
    if (s == "WebCdpCommand") return K::WebCdpCommand;
    if (s == "LuaScript") return K::LuaScript;
    if (s == "Conditional") return K::Conditional;
    if (s == "Loop") return K::Loop;
    throw std::runtime_error(std::string{"kindFromString: unknown kind '"} +
                             std::string{s} + "'");
}

std::int64_t toNs(std::chrono::steady_clock::time_point tp) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(tp.time_since_epoch()).count();
}

std::chrono::steady_clock::time_point fromNs(std::int64_t ns) {
    return std::chrono::steady_clock::time_point{std::chrono::nanoseconds{ns}};
}

json actionToJson(const core::Action& a) {
    json j;
    j["kind"] = kindToString(a.kind);
    j["timestampNs"] = toNs(a.timestamp);

    std::visit(
        [&j](const auto& payload) {
            using T = std::decay_t<decltype(payload)>;
            if constexpr (std::is_same_v<T, core::KeyEvent>) {
                j["key"] = {{"vk", payload.virtualKey}, {"ext", payload.extended}};
            } else if constexpr (std::is_same_v<T, core::MouseEvent>) {
                j["mouse"] = {
                    {"x", payload.x},
                    {"y", payload.y},
                    {"button", payload.button},
                    {"scroll", payload.scrollDelta},
                };
            } else if constexpr (std::is_same_v<T, core::DelayEvent>) {
                j["delay"] = {{"ms", payload.duration.count()}};
            } else if constexpr (std::is_same_v<T, core::LuaEvent>) {
                j["lua"] = {{"scriptPath", payload.scriptPath}};
            }
            // std::monostate → no payload field
        },
        a.payload);
    return j;
}

core::Action actionFromJson(const json& j) {
    core::Action a{};
    a.kind = kindFromString(j.at("kind").get<std::string>());
    a.timestamp = fromNs(j.at("timestampNs").get<std::int64_t>());

    if (j.contains("key")) {
        a.payload = core::KeyEvent{
            .virtualKey = j["key"].at("vk").get<int>(),
            .extended = j["key"].at("ext").get<bool>(),
        };
    } else if (j.contains("mouse")) {
        a.payload = core::MouseEvent{
            .x = j["mouse"].at("x").get<int>(),
            .y = j["mouse"].at("y").get<int>(),
            .button = j["mouse"].at("button").get<int>(),
            .scrollDelta = j["mouse"].at("scroll").get<int>(),
        };
    } else if (j.contains("delay")) {
        a.payload = core::DelayEvent{
            .duration = std::chrono::milliseconds{j["delay"].at("ms").get<std::int64_t>()},
        };
    } else if (j.contains("lua")) {
        a.payload = core::LuaEvent{
            .scriptPath = j["lua"].at("scriptPath").get<std::string>(),
        };
    } else {
        a.payload = std::monostate{};
    }
    return a;
}

}  // namespace

std::string MacroSerializer::toJson(const core::Macro& macro) {
    json j;
    j["schemaVersion"] = kSchemaVersion;
    j["id"] = macro.id;
    j["name"] = macro.name;
    j["description"] = macro.description;
    j["createdAtUnixMs"] = macro.createdAtUnixMs;
    j["updatedAtUnixMs"] = macro.updatedAtUnixMs;
    j["actions"] = json::array();
    for (const auto& a : macro.actions) {
        j["actions"].push_back(actionToJson(a));
    }
    return j.dump();
}

core::Macro MacroSerializer::fromJson(const std::string& jsonStr) {
    const auto j = json::parse(jsonStr);

    const auto version = j.value("schemaVersion", 0);
    if (version != kSchemaVersion) {
        throw std::runtime_error("MacroSerializer: unsupported schemaVersion " +
                                 std::to_string(version));
    }

    core::Macro m{};
    m.id = j.value("id", 0LL);
    m.name = j.value("name", std::string{});
    m.description = j.value("description", std::string{});
    m.createdAtUnixMs = j.value("createdAtUnixMs", 0LL);
    m.updatedAtUnixMs = j.value("updatedAtUnixMs", 0LL);

    if (j.contains("actions")) {
        m.actions.reserve(j["actions"].size());
        for (const auto& a : j["actions"]) {
            m.actions.push_back(actionFromJson(a));
        }
    }
    return m;
}

}  // namespace autopilot::storage
