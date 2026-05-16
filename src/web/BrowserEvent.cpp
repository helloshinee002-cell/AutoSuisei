#include "BrowserEvent.h"

#include <chrono>

namespace autopilot::web {

namespace {

std::chrono::steady_clock::time_point tsToTimepoint(double performanceNowMs) {
    const auto ns = static_cast<std::int64_t>(performanceNowMs * 1e6);
    return std::chrono::steady_clock::time_point{std::chrono::nanoseconds{ns}};
}

}  // namespace

std::optional<core::Action> BrowserEvent::toAction(const std::string& payloadJson) {
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(payloadJson);
    } catch (...) {
        return std::nullopt;
    }

    if (!j.contains("kind") || !j["kind"].is_string()) return std::nullopt;
    const auto kind = j["kind"].get<std::string>();
    const double ts = j.value("ts", 0.0);

    core::Action a{};
    a.timestamp = tsToTimepoint(ts);

    if (kind == "keydown" || kind == "keyup") {
        if (!j.contains("keyCode") || !j["keyCode"].is_number_integer()) return std::nullopt;
        a.kind = (kind == "keydown") ? core::ActionKind::KeyDown : core::ActionKind::KeyUp;
        a.payload = core::KeyEvent{
            .virtualKey = j["keyCode"].get<int>(),
            .extended = false,
        };
        return a;
    }

    if (kind == "click" || kind == "mousedown" || kind == "mouseup") {
        if (!j.contains("x") || !j.contains("y")) return std::nullopt;
        a.kind = (kind == "click") ? core::ActionKind::MouseClick : core::ActionKind::MouseMove;
        a.payload = core::MouseEvent{
            .x = j["x"].get<int>(),
            .y = j["y"].get<int>(),
            .button = j.value("button", 0),
            .scrollDelta = 0,
        };
        return a;
    }

    return std::nullopt;
}

}  // namespace autopilot::web
