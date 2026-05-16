#pragma once

#include <functional>

#include "../core/Action.h"

namespace autopilot::recorder {

using ActionCallback = std::function<void(const core::Action&)>;

/**
 * IRecorder = interface สำหรับจับ input events
 * Implementation: WindowsRecorder (WinAPI hooks)
 */
class IRecorder {
public:
    virtual ~IRecorder() = default;

    /** เริ่มจับ event ห้าม block — ต้อง spawn thread เอง */
    virtual void start(ActionCallback onAction) = 0;

    /** หยุดจับและ unhook ทุก hook */
    virtual void stop() = 0;

    /** true ถ้ากำลังจับอยู่ */
    [[nodiscard]] virtual bool isRecording() const noexcept = 0;
};

}  // namespace autopilot::recorder
