#include <gtest/gtest.h>

#include "web/BrowserEvent.h"
#include "web/BrowserRecorderScript.h"

using autopilot::core::ActionKind;
using autopilot::core::KeyEvent;
using autopilot::core::MouseEvent;
using autopilot::web::BrowserEvent;
using autopilot::web::BrowserRecorderScript;

TEST(BrowserEvent, ParsesKeydownToKeyDownAction) {
    const std::string payload = R"({"kind":"keydown","keyCode":65,"key":"a","ts":123.4})";
    const auto a = BrowserEvent::toAction(payload);
    ASSERT_TRUE(a.has_value());
    EXPECT_EQ(a->kind, ActionKind::KeyDown);
    EXPECT_EQ(std::get<KeyEvent>(a->payload).virtualKey, 65);
}

TEST(BrowserEvent, ParsesClickToMouseClick) {
    const std::string payload = R"({"kind":"click","x":120,"y":240,"button":0,"ts":1})";
    const auto a = BrowserEvent::toAction(payload);
    ASSERT_TRUE(a.has_value());
    EXPECT_EQ(a->kind, ActionKind::MouseClick);
    const auto& m = std::get<MouseEvent>(a->payload);
    EXPECT_EQ(m.x, 120);
    EXPECT_EQ(m.y, 240);
}

TEST(BrowserEvent, RejectsUnknownKind) {
    EXPECT_FALSE(BrowserEvent::toAction(R"({"kind":"weird","ts":0})").has_value());
}

TEST(BrowserEvent, RejectsMalformedJson) {
    EXPECT_FALSE(BrowserEvent::toAction("{nope").has_value());
}

TEST(BrowserEvent, RejectsKeyEventWithoutKeyCode) {
    EXPECT_FALSE(BrowserEvent::toAction(R"({"kind":"keydown","ts":0})").has_value());
}

TEST(BrowserRecorderScript, SubstitutesBindingName) {
    const auto src = BrowserRecorderScript::source("myBinding42");
    EXPECT_NE(src.find("window[\"myBinding42\"]"), std::string::npos);
    EXPECT_EQ(src.find("{{BINDING_NAME}}"), std::string::npos);
}

TEST(BrowserRecorderScript, IncludesAllEventListeners) {
    const auto src = BrowserRecorderScript::source("x");
    EXPECT_NE(src.find("keydown"), std::string::npos);
    EXPECT_NE(src.find("keyup"), std::string::npos);
    EXPECT_NE(src.find("mousedown"), std::string::npos);
    EXPECT_NE(src.find("click"), std::string::npos);
}
