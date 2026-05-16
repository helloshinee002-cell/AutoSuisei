#include <gtest/gtest.h>

#include <chrono>

#include "core/Macro.h"
#include "storage/MacroSerializer.h"

using autopilot::core::Action;
using autopilot::core::ActionKind;
using autopilot::core::DelayEvent;
using autopilot::core::KeyEvent;
using autopilot::core::Macro;
using autopilot::core::MouseEvent;
using autopilot::storage::MacroSerializer;

namespace {

Action keyAction(int vk, ActionKind kind, std::int64_t ns) {
    return Action{
        .kind = kind,
        .timestamp = std::chrono::steady_clock::time_point{std::chrono::nanoseconds{ns}},
        .payload = KeyEvent{.virtualKey = vk, .extended = false},
    };
}

Action mouseAction(int x, int y, ActionKind kind, std::int64_t ns) {
    return Action{
        .kind = kind,
        .timestamp = std::chrono::steady_clock::time_point{std::chrono::nanoseconds{ns}},
        .payload = MouseEvent{.x = x, .y = y, .button = 0, .scrollDelta = 0},
    };
}

}  // namespace

TEST(MacroSerializer, RoundTripPreservesMetadataAndActions) {
    Macro m{};
    m.id = 42;
    m.name = "test macro";
    m.description = "บันทึก keystroke ภาษาไทย";
    m.createdAtUnixMs = 1700000000000;
    m.updatedAtUnixMs = 1700000005000;
    m.actions = {
        keyAction(0x41, ActionKind::KeyDown, 1000),
        keyAction(0x41, ActionKind::KeyUp, 1500),
        mouseAction(100, 200, ActionKind::MouseClick, 2000),
    };

    const auto json = MacroSerializer::toJson(m);
    const auto restored = MacroSerializer::fromJson(json);

    EXPECT_EQ(restored.id, m.id);
    EXPECT_EQ(restored.name, m.name);
    EXPECT_EQ(restored.description, m.description);
    EXPECT_EQ(restored.createdAtUnixMs, m.createdAtUnixMs);
    EXPECT_EQ(restored.updatedAtUnixMs, m.updatedAtUnixMs);
    ASSERT_EQ(restored.actions.size(), 3u);

    EXPECT_EQ(restored.actions[0].kind, ActionKind::KeyDown);
    EXPECT_EQ(std::get<KeyEvent>(restored.actions[0].payload).virtualKey, 0x41);

    EXPECT_EQ(restored.actions[2].kind, ActionKind::MouseClick);
    const auto& mouse = std::get<MouseEvent>(restored.actions[2].payload);
    EXPECT_EQ(mouse.x, 100);
    EXPECT_EQ(mouse.y, 200);
}

TEST(MacroSerializer, HandlesDelayActions) {
    Macro m{};
    m.name = "with delay";
    m.actions.push_back(Action{
        .kind = ActionKind::Delay,
        .timestamp = std::chrono::steady_clock::time_point{std::chrono::nanoseconds{500}},
        .payload = DelayEvent{.duration = std::chrono::milliseconds{750}},
    });

    const auto restored = MacroSerializer::fromJson(MacroSerializer::toJson(m));
    ASSERT_EQ(restored.actions.size(), 1u);
    const auto& d = std::get<DelayEvent>(restored.actions[0].payload);
    EXPECT_EQ(d.duration.count(), 750);
}

TEST(MacroSerializer, RejectsUnknownSchemaVersion) {
    const std::string bad = R"({"schemaVersion": 999, "name": "x", "actions": []})";
    EXPECT_THROW(MacroSerializer::fromJson(bad), std::runtime_error);
}

TEST(MacroSerializer, RejectsMalformedJson) {
    EXPECT_THROW(MacroSerializer::fromJson("{not json"), std::exception);
}
