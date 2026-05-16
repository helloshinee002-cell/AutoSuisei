#include "WindowsPlayer.h"

#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <variant>

#include <spdlog/spdlog.h>

namespace autopilot::player {

namespace {

void sendKeyInput(int virtualKey, bool isDown, bool extended) {
    INPUT in{};
    in.type = INPUT_KEYBOARD;
    in.ki.wVk = static_cast<WORD>(virtualKey);
    in.ki.wScan = static_cast<WORD>(MapVirtualKeyW(virtualKey, MAPVK_VK_TO_VSC));
    in.ki.dwFlags = KEYEVENTF_SCANCODE;
    if (!isDown) in.ki.dwFlags |= KEYEVENTF_KEYUP;
    if (extended) in.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;

    if (SendInput(1, &in, sizeof(INPUT)) != 1) {
        throw std::system_error(static_cast<int>(GetLastError()), std::system_category(),
                                "SendInput(keyboard) failed");
    }
}

/** Convert screen px → normalized 0..65535 ตามที่ SendInput MOUSEINPUT ต้องการ */
void sendMouseMoveAbsolute(int x, int y) {
    const int sw = std::max(1, GetSystemMetrics(SM_CXVIRTUALSCREEN));
    const int sh = std::max(1, GetSystemMetrics(SM_CYVIRTUALSCREEN));
    const int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);

    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dx = static_cast<LONG>(((x - vx) * 65535) / sw);
    in.mi.dy = static_cast<LONG>(((y - vy) * 65535) / sh);
    in.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;

    if (SendInput(1, &in, sizeof(INPUT)) != 1) {
        throw std::system_error(static_cast<int>(GetLastError()), std::system_category(),
                                "SendInput(mouse move) failed");
    }
}

void sendMouseButton(int button, bool isDown) {
    DWORD flag = 0;
    switch (button) {
        case 0: flag = isDown ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP; break;
        case 1: flag = isDown ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP; break;
        case 2: flag = isDown ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP; break;
        default:
            spdlog::warn("WindowsPlayer: unknown mouse button {}", button);
            return;
    }
    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = flag;
    if (SendInput(1, &in, sizeof(INPUT)) != 1) {
        throw std::system_error(static_cast<int>(GetLastError()), std::system_category(),
                                "SendInput(mouse button) failed");
    }
}

void sendMouseScroll(int delta) {
    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.mouseData = static_cast<DWORD>(delta);
    in.mi.dwFlags = MOUSEEVENTF_WHEEL;
    if (SendInput(1, &in, sizeof(INPUT)) != 1) {
        throw std::system_error(static_cast<int>(GetLastError()), std::system_category(),
                                "SendInput(scroll) failed");
    }
}

bool foregroundWindowAlive() {
    const HWND fg = GetForegroundWindow();
    return fg != nullptr && IsWindow(fg);
}

}  // namespace

void WindowsPlayer::executeAction(const core::Action& action) {
    using core::ActionKind;
    using core::KeyEvent;
    using core::MouseEvent;

    if (!foregroundWindowAlive()) {
        throw std::runtime_error("WindowsPlayer: no foreground window to receive input");
    }

    switch (action.kind) {
        case ActionKind::KeyDown:
        case ActionKind::KeyUp: {
            const auto& key = std::get<KeyEvent>(action.payload);
            sendKeyInput(key.virtualKey, action.kind == ActionKind::KeyDown, key.extended);
            break;
        }
        case ActionKind::MouseMove: {
            const auto& m = std::get<MouseEvent>(action.payload);
            sendMouseMoveAbsolute(m.x, m.y);
            break;
        }
        case ActionKind::MouseClick: {
            const auto& m = std::get<MouseEvent>(action.payload);
            sendMouseMoveAbsolute(m.x, m.y);
            sendMouseButton(m.button, true);
            sendMouseButton(m.button, false);
            break;
        }
        case ActionKind::MouseScroll: {
            const auto& m = std::get<MouseEvent>(action.payload);
            sendMouseScroll(m.scrollDelta);
            break;
        }
        default:
            spdlog::debug("WindowsPlayer: action kind {} not yet supported",
                          static_cast<int>(action.kind));
            break;
    }
}

void WindowsPlayer::playOnce(const core::Macro& macro, const PlaybackOptions& opts) {
    if (macro.actions.empty()) return;

    const float speed = (opts.speedMultiplier > 0.01f) ? opts.speedMultiplier : 1.0f;
    auto prev = macro.actions.front().timestamp;

    for (const auto& action : macro.actions) {
        if (stopRequested_.load(std::memory_order_acquire)) return;

        const auto deltaUs = std::chrono::duration_cast<std::chrono::microseconds>(
                                 action.timestamp - prev)
                                 .count();
        const auto scaledUs = static_cast<std::int64_t>(static_cast<double>(deltaUs) / speed);
        if (scaledUs > 0) {
            std::this_thread::sleep_for(std::chrono::microseconds{scaledUs});
        }
        prev = action.timestamp;

        try {
            executeAction(action);
        } catch (const std::exception& e) {
            spdlog::error("WindowsPlayer: action failed: {}", e.what());
            if (opts.stopOnError) throw;
        }
    }
}

void WindowsPlayer::play(const core::Macro& macro, const PlaybackOptions& opts) {
    stopRequested_.store(false, std::memory_order_release);

    const int loops = (opts.loopCount < 0) ? std::numeric_limits<int>::max()
                                            : std::max(1, opts.loopCount);

    for (int i = 0; i < loops; ++i) {
        if (stopRequested_.load(std::memory_order_acquire)) return;
        playOnce(macro, opts);
    }
}

void WindowsPlayer::requestStop() noexcept {
    stopRequested_.store(true, std::memory_order_release);
}

}  // namespace autopilot::player
