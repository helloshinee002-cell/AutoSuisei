#pragma once

#include <algorithm>
#include <string>
#include <vector>

namespace autopilot::gui {

/**
 * แปลง version string ("v1.2.3" / "1.2.3" / "1.2.3-beta") → component ints {1,2,3}.
 * ตัด leading 'v'/'V'; หยุดที่อักขระแรกที่ไม่ใช่เลข/จุด (เช่น suffix "-beta") — pure, no Qt.
 */
inline std::vector<int> parseSemver(std::string s) {
    if (!s.empty() && (s.front() == 'v' || s.front() == 'V')) s.erase(0, 1);
    std::vector<int> parts;
    std::string cur;
    auto flush = [&]() {
        parts.push_back(cur.empty() ? 0 : std::stoi(cur));
        cur.clear();
    };
    for (char c : s) {
        if (c >= '0' && c <= '9') {
            cur += c;
        } else if (c == '.') {
            flush();
        } else {
            break;  // suffix เช่น "-beta" / "+build" → จบแค่ตัวเลข
        }
    }
    flush();
    return parts;
}

/**
 * true ถ้า `latest` ใหม่กว่า `current` ตาม semver (เทียบทีละ component, เติม 0 ให้ยาวเท่ากัน).
 * เช่น 1.0.1 > 1.0.0, v1.1 > 1.0.9, 2.0.0 > 1.9.9. เท่ากัน → false.
 */
inline bool isVersionNewer(const std::string& latest, const std::string& current) {
    std::vector<int> a = parseSemver(latest);
    std::vector<int> b = parseSemver(current);
    const std::size_t n = std::max(a.size(), b.size());
    a.resize(n, 0);
    b.resize(n, 0);
    for (std::size_t i = 0; i < n; ++i) {
        if (a[i] != b[i]) return a[i] > b[i];
    }
    return false;
}

}  // namespace autopilot::gui
