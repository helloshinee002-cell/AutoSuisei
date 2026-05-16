#pragma once

#include <string>

namespace autopilot::web {

/**
 * JavaScript ที่ inject เข้าหน้า browser ผ่าน CDP Page.addScriptToEvaluateOnNewDocument
 *
 * วิธีคิด: ใช้ Runtime.addBinding สร้าง global function `window.<binding>` ที่เมื่อเรียก
 * จะกระตุ้น Runtime.bindingCalled event กลับมาฝั่ง C++
 * Script เพิ่ม event listener (keydown/keyup/mousedown/mouseup/mousemove/click) แล้ว serialize
 * ส่ง JSON string ผ่าน binding
 *
 * เก็บแยกเป็นไฟล์เพื่อ unit-test ได้ (verify shape ของ payload ที่จะส่ง)
 */
class BrowserRecorderScript {
public:
    /** @return JS source ที่พร้อม inject แทนที่ {{BINDING_NAME}} ด้วยชื่อ binding จริง */
    [[nodiscard]] static std::string source(const std::string& bindingName);

    /** ชื่อ binding default ที่ใช้ */
    static constexpr const char* kDefaultBindingName = "__autopilotRecord";
};

}  // namespace autopilot::web
