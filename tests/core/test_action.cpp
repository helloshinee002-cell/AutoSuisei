#include <gtest/gtest.h>

#include "core/Action.h"

using autopilot::core::Action;
using autopilot::core::ActionKind;
using autopilot::core::KeyEvent;

TEST(Action, HoldsKeyEventViaVariant) {
    Action a{
        .kind = ActionKind::KeyDown,
        .timestamp = std::chrono::steady_clock::now(),
        .payload = KeyEvent{.virtualKey = 0x41, .extended = false},
    };

    ASSERT_TRUE(std::holds_alternative<KeyEvent>(a.payload));
    EXPECT_EQ(std::get<KeyEvent>(a.payload).virtualKey, 0x41);
}
