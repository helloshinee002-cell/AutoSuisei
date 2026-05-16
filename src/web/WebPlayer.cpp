#include "WebPlayer.h"

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <thread>
#include <variant>

#include <spdlog/spdlog.h>

namespace autopilot::web {

namespace {

std::string mouseButtonName(int b) {
    switch (b) {
        case 0: return "left";
        case 1: return "right";
        case 2: return "middle";
        default: return "none";
    }
}

nlohmann::json buildKeyParams(const core::KeyEvent& key, bool isDown) {
    return nlohmann::json{
        {"type", isDown ? "keyDown" : "keyUp"},
        {"windowsVirtualKeyCode", key.virtualKey},
        {"nativeVirtualKeyCode", key.virtualKey},
    };
}

nlohmann::json buildMouseParams(const core::MouseEvent& m, const std::string& type) {
    return nlohmann::json{
        {"type", type},
        {"x", m.x},
        {"y", m.y},
        {"button", mouseButtonName(m.button)},
        {"clickCount", type == "mousePressed" || type == "mouseReleased" ? 1 : 0},
    };
}

}  // namespace

WebPlayer::WebPlayer(ICdpClient& client) : client_(client) {}

void WebPlayer::dispatchAction(const core::Action& action, const WebPlaybackOptions& opts) {
    using core::ActionKind;
    using core::KeyEvent;
    using core::MouseEvent;

    auto wait = [&](std::future<nlohmann::json>& f, const std::string& label) {
        const auto status = f.wait_for(std::chrono::milliseconds{opts.requestTimeoutMs});
        if (status != std::future_status::ready) {
            throw std::runtime_error("WebPlayer: CDP command timed out: " + label);
        }
        f.get();
    };

    switch (action.kind) {
        case ActionKind::KeyDown:
        case ActionKind::KeyUp: {
            const auto& k = std::get<KeyEvent>(action.payload);
            auto fut = client_.send("Input.dispatchKeyEvent",
                                    buildKeyParams(k, action.kind == ActionKind::KeyDown));
            wait(fut, "Input.dispatchKeyEvent");
            break;
        }
        case ActionKind::MouseMove: {
            const auto& m = std::get<MouseEvent>(action.payload);
            auto fut =
                client_.send("Input.dispatchMouseEvent", buildMouseParams(m, "mouseMoved"));
            wait(fut, "mouseMoved");
            break;
        }
        case ActionKind::MouseClick: {
            const auto& m = std::get<MouseEvent>(action.payload);
            auto fdown =
                client_.send("Input.dispatchMouseEvent", buildMouseParams(m, "mousePressed"));
            wait(fdown, "mousePressed");
            auto fup =
                client_.send("Input.dispatchMouseEvent", buildMouseParams(m, "mouseReleased"));
            wait(fup, "mouseReleased");
            break;
        }
        case ActionKind::MouseScroll: {
            const auto& m = std::get<MouseEvent>(action.payload);
            nlohmann::json p = buildMouseParams(m, "mouseWheel");
            p["deltaX"] = 0;
            p["deltaY"] = -m.scrollDelta;
            auto fut = client_.send("Input.dispatchMouseEvent", p);
            wait(fut, "mouseWheel");
            break;
        }
        default:
            spdlog::debug("WebPlayer: action kind {} not supported on web",
                          static_cast<int>(action.kind));
            break;
    }
}

void WebPlayer::play(const core::Macro& macro, const WebPlaybackOptions& opts) {
    if (!client_.isConnected()) {
        throw std::runtime_error("WebPlayer::play: ICdpClient not connected");
    }

    stopRequested_.store(false, std::memory_order_release);
    const float speed = (opts.speedMultiplier > 0.01f) ? opts.speedMultiplier : 1.0f;

    if (macro.actions.empty()) return;
    auto prev = macro.actions.front().timestamp;

    for (const auto& action : macro.actions) {
        if (stopRequested_.load(std::memory_order_acquire)) return;

        const auto deltaUs = std::chrono::duration_cast<std::chrono::microseconds>(
                                 action.timestamp - prev)
                                 .count();
        const auto scaledUs = static_cast<std::int64_t>(static_cast<double>(deltaUs) / speed);
        if (scaledUs > 0) std::this_thread::sleep_for(std::chrono::microseconds{scaledUs});
        prev = action.timestamp;

        try {
            dispatchAction(action, opts);
        } catch (const std::exception& e) {
            spdlog::error("WebPlayer: action failed: {}", e.what());
            if (opts.stopOnError) throw;
        }
    }
}

void WebPlayer::requestStop() noexcept {
    stopRequested_.store(true, std::memory_order_release);
}

}  // namespace autopilot::web
