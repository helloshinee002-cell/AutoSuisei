#pragma once

#include <optional>
#include <string>

#include <nlohmann/json.hpp>

#include "core/Action.h"

namespace autopilot::web {

/**
 * เปลี่ยน payload JSON ที่ browser-side JS ส่งมา (ผ่าน Runtime.bindingCalled) เป็น core::Action
 *
 * Schema (ดู BrowserRecorderScript.cpp):
 *   { kind: "keydown"|"keyup"|"mousedown"|"mouseup"|"click",
 *     code, key, keyCode, ctrl, shift, alt, meta,  // key events
 *     x, y, button,                                 // mouse events
 *     ts (performance.now ms) }
 */
class BrowserEvent {
public:
    /** @return nullopt ถ้า kind ไม่รู้จัก หรือ payload ขาด field สำคัญ */
    [[nodiscard]] static std::optional<core::Action> toAction(const std::string& payloadJson);
};

}  // namespace autopilot::web
