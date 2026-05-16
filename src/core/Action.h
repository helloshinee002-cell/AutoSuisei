#pragma once

#include <chrono>
#include <string>
#include <variant>

namespace autopilot::core {

/** ประเภทของ action ที่ macro engine รองรับ */
enum class ActionKind {
    KeyDown,
    KeyUp,
    MouseMove,
    MouseClick,
    MouseScroll,
    WindowFocus,
    Delay,
    Screenshot,
    OcrCapture,
    WebCdpCommand,
    LuaScript,
    Conditional,
    Loop
};

struct KeyEvent {
    int virtualKey;
    bool extended;
};

struct MouseEvent {
    int x;
    int y;
    int button;       ///< 0=left, 1=right, 2=middle
    int scrollDelta;  ///< signed wheel delta
};

struct DelayEvent {
    std::chrono::milliseconds duration;
};

struct LuaEvent {
    std::string scriptPath;
};

/**
 * Action เป็น value-type ตัวเดียวที่เคลื่อนผ่าน recorder → storage → player
 * ใช้ std::variant เพื่อ type-safe dispatch โดยไม่ต้องมี virtual table
 */
struct Action {
    ActionKind kind;
    std::chrono::steady_clock::time_point timestamp;
    std::variant<KeyEvent, MouseEvent, DelayEvent, LuaEvent, std::monostate> payload;
};

}  // namespace autopilot::core
