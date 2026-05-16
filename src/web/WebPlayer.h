#pragma once

#include <atomic>
#include <memory>
#include <string>

#include "core/Macro.h"
#include "ICdpClient.h"

namespace autopilot::web {

struct WebPlaybackOptions {
    float speedMultiplier{1.0f};
    bool stopOnError{true};
    int requestTimeoutMs{5000};
    std::string sessionId{};  ///< optional CDP session for per-target dispatch
};

/**
 * แปลง `core::Macro` → CDP `Input.dispatch*` แล้วส่งผ่าน `ICdpClient`
 * Note: CDP coordinates เป็น CSS pixels ของ viewport, ไม่ใช่ screen pixels
 */
class WebPlayer {
public:
    explicit WebPlayer(ICdpClient& client);

    /** Blocking — รอ response ของแต่ละ CDP command ก่อนทำ action ถัดไป */
    void play(const core::Macro& macro, const WebPlaybackOptions& opts = {});

    void requestStop() noexcept;

private:
    ICdpClient& client_;
    std::atomic<bool> stopRequested_{false};

    void dispatchAction(const core::Action& action, const WebPlaybackOptions& opts);
};

}  // namespace autopilot::web
