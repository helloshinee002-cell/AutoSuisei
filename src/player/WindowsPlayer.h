#pragma once

#include <atomic>

#include "IPlayer.h"

namespace autopilot::player {

/**
 * Windows-specific player: synthesize keyboard/mouse via `SendInput`
 *
 * Timing: ใช้ delta จาก timestamp ของ Action ตัวก่อนหน้า → sleep ก่อน synthesize
 *   - speedMultiplier > 1 = เร็วขึ้น (delay หาร speed)
 *   - speedMultiplier 0 หรือลบ จะ clamp เป็น 1.0
 */
class WindowsPlayer final : public IPlayer {
public:
    WindowsPlayer() = default;
    ~WindowsPlayer() override = default;

    WindowsPlayer(const WindowsPlayer&) = delete;
    WindowsPlayer& operator=(const WindowsPlayer&) = delete;

    void play(const core::Macro& macro, const PlaybackOptions& opts) override;
    void requestStop() noexcept override;

private:
    std::atomic<bool> stopRequested_{false};

    void playOnce(const core::Macro& macro, const PlaybackOptions& opts);
    void executeAction(const core::Action& action);
};

}  // namespace autopilot::player
