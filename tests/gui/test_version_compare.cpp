#include <gtest/gtest.h>

#include <vector>

#include "gui/VersionCompare.h"

using autopilot::gui::isVersionNewer;
using autopilot::gui::parseSemver;

// ใช้ตัดสินใจใน Updater::checkLatest ว่า tag_name ของ release ใหม่กว่าเวอร์ชันที่รันอยู่ไหม
TEST(VersionCompare, NewerPatch) { EXPECT_TRUE(isVersionNewer("1.0.1", "1.0.0")); }
TEST(VersionCompare, NewerMinorBeatsHigherPatch) { EXPECT_TRUE(isVersionNewer("1.1.0", "1.0.9")); }
TEST(VersionCompare, NewerMajor) { EXPECT_TRUE(isVersionNewer("2.0.0", "1.9.9")); }
TEST(VersionCompare, EqualIsNotNewer) { EXPECT_FALSE(isVersionNewer("1.0.0", "1.0.0")); }
TEST(VersionCompare, OlderIsNotNewer) { EXPECT_FALSE(isVersionNewer("1.0.0", "1.0.1")); }

TEST(VersionCompare, StripsLeadingV) {
    EXPECT_TRUE(isVersionNewer("v1.2.0", "1.1.0"));
    EXPECT_FALSE(isVersionNewer("v1.0.0", "v1.0.0"));
}

TEST(VersionCompare, DifferentLengthsPadWithZero) {
    EXPECT_FALSE(isVersionNewer("1.0", "1.0.0"));  // 1.0 == 1.0.0
    EXPECT_TRUE(isVersionNewer("1.0.1", "1.0"));   // 1.0.1 > 1.0(.0)
}

TEST(VersionCompare, IgnoresPrereleaseSuffix) {
    EXPECT_TRUE(isVersionNewer("1.2.0-beta", "1.1.0"));
    EXPECT_FALSE(isVersionNewer("1.0.0-rc1", "1.0.0"));  // numeric เท่ากัน → ไม่ใหม่กว่า
}

TEST(VersionCompare, ParseStripsVAndSuffix) {
    EXPECT_EQ(parseSemver("v1.2.3"), (std::vector<int>{1, 2, 3}));
    EXPECT_EQ(parseSemver("1.0.0-beta"), (std::vector<int>{1, 0, 0}));
}
