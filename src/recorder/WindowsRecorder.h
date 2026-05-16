#pragma once

#include <atomic>
#include <thread>

#include "IRecorder.h"

namespace autopilot::recorder {

/**
 * Windows-specific recorder ใช้ `SetWindowsHookEx` กับ `WH_KEYBOARD_LL` + `WH_MOUSE_LL`
 *
 * Threading model:
 *   1. start() spawn **hook thread** ที่:
 *      - Install LL hooks (LL hook ต้องอยู่บน thread ที่มี message loop)
 *      - รัน GetMessage loop จนกว่าจะ stop
 *   2. start() spawn **worker thread** ที่ drain SPSC queue → convert raw → core::Action → callback
 *
 * เพราะ hook proc เป็น C-style callback ที่ต้อง static เราใช้ `s_activeInstance`
 * pointer เพื่อ route กลับเข้า object — รองรับ recorder instance เดียวต่อ process
 * (ซึ่งเป็น constraint ของ LL hooks อยู่แล้วเชิงปฏิบัติ)
 */
class WindowsRecorder final : public IRecorder {
public:
    WindowsRecorder();
    ~WindowsRecorder() override;

    WindowsRecorder(const WindowsRecorder&) = delete;
    WindowsRecorder& operator=(const WindowsRecorder&) = delete;
    WindowsRecorder(WindowsRecorder&&) = delete;
    WindowsRecorder& operator=(WindowsRecorder&&) = delete;

    void start(ActionCallback onAction) override;
    void stop() override;
    [[nodiscard]] bool isRecording() const noexcept override;

private:
    struct Impl;
    Impl* impl_;  ///< owning raw ptr; deleted in dtor — ซ่อน Windows.h ออกจาก header
};

}  // namespace autopilot::recorder
