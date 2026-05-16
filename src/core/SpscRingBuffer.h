#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <type_traits>

namespace autopilot::core {

/**
 * Lock-free single-producer single-consumer ring buffer
 *
 * ออกแบบสำหรับ LowLevel hook → worker thread เท่านั้น
 *   - Producer: hook callback (must not allocate / not block)
 *   - Consumer: 1 worker thread
 *
 * Cap ต้องเป็น power-of-two เพื่อให้ใช้ bitmask แทน modulo (เร็วกว่า)
 * T ต้องเป็น trivially-copyable (ไม่เรียก ctor/dtor ที่อาจ throw หรือ alloc)
 */
template <typename T, std::size_t Cap>
class SpscRingBuffer {
    static_assert(Cap >= 2, "Cap must be >= 2");
    static_assert((Cap & (Cap - 1)) == 0, "Cap must be a power of two");
    static_assert(std::is_trivially_copyable_v<T>,
                  "T must be trivially copyable for lock-free safety");

public:
    /** Producer-side: ลองใส่ item คืน false ถ้าคิวเต็ม (drop policy ให้ caller จัดการ) */
    [[nodiscard]] bool push(const T& item) noexcept {
        const auto head = head_.load(std::memory_order_relaxed);
        const auto next = (head + 1) & kMask;
        if (next == tail_.load(std::memory_order_acquire)) {
            return false;  // full
        }
        buffer_[head] = item;
        head_.store(next, std::memory_order_release);
        return true;
    }

    /** Consumer-side: ลอง pop คืน false ถ้าว่าง */
    [[nodiscard]] bool pop(T& out) noexcept {
        const auto tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) {
            return false;  // empty
        }
        out = buffer_[tail];
        tail_.store((tail + 1) & kMask, std::memory_order_release);
        return true;
    }

    /** Approximate size — สำหรับ monitoring/test เท่านั้น (อาจ stale ระหว่าง concurrent ops) */
    [[nodiscard]] std::size_t approxSize() const noexcept {
        const auto h = head_.load(std::memory_order_acquire);
        const auto t = tail_.load(std::memory_order_acquire);
        return (h - t) & kMask;
    }

    [[nodiscard]] static constexpr std::size_t capacity() noexcept { return Cap - 1; }

private:
    static constexpr std::size_t kMask = Cap - 1;

    alignas(64) std::atomic<std::size_t> head_{0};
    alignas(64) std::atomic<std::size_t> tail_{0};
    std::array<T, Cap> buffer_{};
};

}  // namespace autopilot::core
