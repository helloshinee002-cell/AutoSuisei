#pragma once

#include <optional>
#include <string>

namespace autopilot::vision {

struct MatchResult {
    int x{0};            ///< top-left ของ match บน haystack
    int y{0};
    int width{0};        ///< ขนาด needle
    int height{0};
    float confidence{0}; ///< 0..1 (CV_TM_CCOEFF_NORMED scale)

    /** จุดศูนย์กลางสำหรับ click */
    [[nodiscard]] int centerX() const noexcept { return x + width / 2; }
    [[nodiscard]] int centerY() const noexcept { return y + height / 2; }
};

struct MatchOptions {
    float minConfidence{0.8f};
    bool grayscale{true};  ///< แปลงเป็น gray ก่อน match — ทนต่อสีเปลี่ยน
};

/**
 * Template matching ด้วย OpenCV
 *   - ใช้ CV_TM_CCOEFF_NORMED (normalized cross-correlation) — ทนต่อแสงเล็กน้อย
 *   - คืน nullopt ถ้า confidence < minConfidence
 */
class ImageMatcher {
public:
    /** Match needle (template) ใน haystack image; ทั้งคู่เป็น file paths */
    [[nodiscard]] static std::optional<MatchResult> matchFiles(
        const std::string& haystackPath,
        const std::string& needlePath,
        const MatchOptions& options = {});
};

}  // namespace autopilot::vision
