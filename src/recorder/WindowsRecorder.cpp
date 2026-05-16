#include "WindowsRecorder.h"

#include <windows.h>

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <system_error>
#include <thread>

#include <spdlog/spdlog.h>

#include "core/SpscRingBuffer.h"

namespace autopilot::recorder {

namespace {

/**
 * Raw event ที่ enqueue จาก hook callback — ต้อง trivially copyable
 * Fields ใช้ตาม WPARAM/LPARAM ของ LL hooks ใน MSDN
 */
struct RawEvent {
    enum class Source : std::uint8_t { Keyboard, Mouse } source;
    std::uint32_t wParam;  ///< message type (WM_KEYDOWN, WM_LBUTTONDOWN ฯลฯ)
    std::uint32_t vkCode;
    std::uint32_t flags;
    POINT point;
    std::int32_t wheelDelta;
    std::int64_t timestampNs;
};

constexpr std::size_t kQueueCapacity = 4096;
using EventQueue = core::SpscRingBuffer<RawEvent, kQueueCapacity>;

}  // namespace

struct WindowsRecorder::Impl {
    EventQueue queue;
    std::atomic<bool> running{false};
    std::atomic<DWORD> hookThreadId{0};
    std::thread hookThread;
    std::thread workerThread;
    HHOOK keyboardHook{nullptr};
    HHOOK mouseHook{nullptr};
    ActionCallback callback;
    std::atomic<std::uint64_t> droppedEvents{0};

    static inline Impl* s_active{nullptr};

    static LRESULT CALLBACK keyboardProc(int code, WPARAM wParam, LPARAM lParam) noexcept;
    static LRESULT CALLBACK mouseProc(int code, WPARAM wParam, LPARAM lParam) noexcept;

    void runHookThread();
    void runWorkerThread();
    void dispatchRaw(const RawEvent& raw);
};

namespace {

std::int64_t nowNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

core::Action toAction(const RawEvent& raw) {
    core::Action a{};
    a.timestamp = std::chrono::steady_clock::time_point{
        std::chrono::nanoseconds{raw.timestampNs}};

    if (raw.source == RawEvent::Source::Keyboard) {
        const bool isDown = (raw.wParam == WM_KEYDOWN || raw.wParam == WM_SYSKEYDOWN);
        a.kind = isDown ? core::ActionKind::KeyDown : core::ActionKind::KeyUp;
        a.payload = core::KeyEvent{
            .virtualKey = static_cast<int>(raw.vkCode),
            .extended = (raw.flags & LLKHF_EXTENDED) != 0,
        };
        return a;
    }

    // mouse
    int button = 0;
    int scroll = 0;
    switch (raw.wParam) {
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
            a.kind = core::ActionKind::MouseClick;
            button = 0;
            break;
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
            a.kind = core::ActionKind::MouseClick;
            button = 1;
            break;
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
            a.kind = core::ActionKind::MouseClick;
            button = 2;
            break;
        case WM_MOUSEWHEEL:
            a.kind = core::ActionKind::MouseScroll;
            scroll = raw.wheelDelta;
            break;
        case WM_MOUSEMOVE:
        default:
            a.kind = core::ActionKind::MouseMove;
            break;
    }
    a.payload = core::MouseEvent{
        .x = raw.point.x,
        .y = raw.point.y,
        .button = button,
        .scrollDelta = scroll,
    };
    return a;
}

}  // namespace

LRESULT CALLBACK WindowsRecorder::Impl::keyboardProc(int code, WPARAM wParam,
                                                     LPARAM lParam) noexcept {
    if (code == HC_ACTION && s_active != nullptr) {
        const auto* info = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
        const RawEvent ev{
            .source = RawEvent::Source::Keyboard,
            .wParam = static_cast<std::uint32_t>(wParam),
            .vkCode = info->vkCode,
            .flags = info->flags,
            .point = {0, 0},
            .wheelDelta = 0,
            .timestampNs = nowNs(),
        };
        if (!s_active->queue.push(ev)) {
            s_active->droppedEvents.fetch_add(1, std::memory_order_relaxed);
        }
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

LRESULT CALLBACK WindowsRecorder::Impl::mouseProc(int code, WPARAM wParam,
                                                  LPARAM lParam) noexcept {
    if (code == HC_ACTION && s_active != nullptr) {
        const auto* info = reinterpret_cast<const MSLLHOOKSTRUCT*>(lParam);
        const RawEvent ev{
            .source = RawEvent::Source::Mouse,
            .wParam = static_cast<std::uint32_t>(wParam),
            .vkCode = 0,
            .flags = info->flags,
            .point = info->pt,
            .wheelDelta = static_cast<std::int32_t>(GET_WHEEL_DELTA_WPARAM(info->mouseData)),
            .timestampNs = nowNs(),
        };
        if (!s_active->queue.push(ev)) {
            s_active->droppedEvents.fetch_add(1, std::memory_order_relaxed);
        }
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

void WindowsRecorder::Impl::runHookThread() {
    hookThreadId.store(GetCurrentThreadId(), std::memory_order_release);

    keyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, &keyboardProc,
                                     GetModuleHandleW(nullptr), 0);
    mouseHook = SetWindowsHookExW(WH_MOUSE_LL, &mouseProc, GetModuleHandleW(nullptr), 0);

    if (keyboardHook == nullptr || mouseHook == nullptr) {
        const auto err = GetLastError();
        spdlog::error("SetWindowsHookExW failed (GetLastError={})", err);
        running.store(false, std::memory_order_release);
        return;
    }

    MSG msg{};
    while (running.load(std::memory_order_acquire)) {
        if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) break;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        } else {
            // hook thread mostly idle — let scheduler breathe
            MsgWaitForMultipleObjects(0, nullptr, FALSE, 10, QS_ALLINPUT);
        }
    }

    if (keyboardHook) UnhookWindowsHookEx(keyboardHook);
    if (mouseHook) UnhookWindowsHookEx(mouseHook);
    keyboardHook = nullptr;
    mouseHook = nullptr;
}

void WindowsRecorder::Impl::dispatchRaw(const RawEvent& raw) {
    if (callback) {
        try {
            callback(toAction(raw));
        } catch (const std::exception& e) {
            spdlog::error("recorder callback threw: {}", e.what());
        }
    }
}

void WindowsRecorder::Impl::runWorkerThread() {
    RawEvent ev{};
    while (running.load(std::memory_order_acquire)) {
        if (queue.pop(ev)) {
            dispatchRaw(ev);
        } else {
            std::this_thread::sleep_for(std::chrono::microseconds{500});
        }
    }
    // drain remaining
    while (queue.pop(ev)) dispatchRaw(ev);
}

WindowsRecorder::WindowsRecorder() : impl_(new Impl()) {}

WindowsRecorder::~WindowsRecorder() {
    try {
        stop();
    } catch (...) {
        // ห้าม throw จาก dtor
    }
    delete impl_;
}

void WindowsRecorder::start(ActionCallback onAction) {
    if (impl_->running.exchange(true)) {
        throw std::runtime_error("WindowsRecorder already running");
    }

    if (Impl::s_active != nullptr) {
        impl_->running.store(false);
        throw std::runtime_error("WindowsRecorder: another recorder instance already active");
    }

    impl_->callback = std::move(onAction);
    Impl::s_active = impl_;

    impl_->hookThread = std::thread([this] { impl_->runHookThread(); });
    impl_->workerThread = std::thread([this] { impl_->runWorkerThread(); });
}

void WindowsRecorder::stop() {
    if (!impl_->running.exchange(false)) {
        return;  // already stopped
    }

    const auto tid = impl_->hookThreadId.load(std::memory_order_acquire);
    if (tid != 0) {
        PostThreadMessageW(tid, WM_QUIT, 0, 0);
    }

    if (impl_->hookThread.joinable()) impl_->hookThread.join();
    if (impl_->workerThread.joinable()) impl_->workerThread.join();

    Impl::s_active = nullptr;
    impl_->callback = nullptr;

    const auto dropped = impl_->droppedEvents.load(std::memory_order_acquire);
    if (dropped > 0) {
        spdlog::warn("WindowsRecorder dropped {} events (queue overflow)", dropped);
    }
}

bool WindowsRecorder::isRecording() const noexcept {
    return impl_->running.load(std::memory_order_acquire);
}

}  // namespace autopilot::recorder
