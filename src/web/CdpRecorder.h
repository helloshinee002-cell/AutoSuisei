#pragma once

#include <functional>
#include <string>

#include "core/Action.h"
#include "ICdpClient.h"

namespace autopilot::web {

/**
 * บันทึก input บน Chrome page ผ่าน CDP
 *
 * Lifecycle:
 *   start() → Page.enable + Runtime.enable + Runtime.addBinding + Page.addScript
 *   ระหว่าง record: Runtime.bindingCalled event ทุกครั้ง → toAction() → emit callback
 *   stop()  → remove binding + script
 *
 * ใช้ ICdpClient ที่ connect แล้ว — recorder ไม่ได้ manage ลูกค้า
 */
class CdpRecorder {
public:
    using ActionCallback = std::function<void(const core::Action&)>;

    CdpRecorder(ICdpClient& client, std::string bindingName);

    /** @throws std::runtime_error ถ้า CDP command timeout/error */
    void start(ActionCallback onAction);
    void stop() noexcept;
    [[nodiscard]] bool isRecording() const noexcept { return recording_; }

private:
    ICdpClient& client_;
    std::string bindingName_;
    bool recording_{false};
    std::string scriptIdentifier_;
    ActionCallback callback_;

    void onCdpEvent(const std::string& method, const nlohmann::json& params);
};

}  // namespace autopilot::web
