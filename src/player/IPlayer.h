#pragma once

#include "../core/Macro.h"

namespace autopilot::player {

struct PlaybackOptions {
    float speedMultiplier{1.0f};
    bool stopOnError{true};
    int loopCount{1};  ///< -1 = infinite
};

class IPlayer {
public:
    virtual ~IPlayer() = default;

    /** เล่น macro ตาม options blocking; throw std::system_error เมื่อ WinAPI fail */
    virtual void play(const core::Macro& macro, const PlaybackOptions& opts) = 0;

    /** ขอให้หยุดเล่นแบบ cooperative */
    virtual void requestStop() noexcept = 0;
};

}  // namespace autopilot::player
