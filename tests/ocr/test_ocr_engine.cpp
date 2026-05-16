#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "ocr/OcrEngine.h"

namespace {

std::string testDataPath(const std::string& filename) {
    return (std::filesystem::path(AUTOPILOT_TEST_DATA_DIR) / filename).string();
}

}  // namespace

using autopilot::ocr::OcrEngine;
using autopilot::ocr::OcrOptions;

// ================ TDD seed ================
// เทสต์เหล่านี้ FAIL ก่อน implement จนกว่า:
//   1. ใส่ไฟล์ภาพ tests/data/number_20.png (ภาพที่มีเลข 20)
//   2. ติดตั้ง vcpkg deps + traineddata เรียบร้อย
//
// ตรงกับเคสที่ user ขอ: "ชื่อไฟล์แปลกๆ แต่ในรูปเห็นเลข 20 → store: filename + digits=20"

TEST(OcrEngine, ReadsDigitsFromNumberImage) {
    OcrEngine engine{OcrOptions{.languages = "eng"}};

    const auto result = engine.recognize(testDataPath("number_20.png"));

    EXPECT_EQ(result.digits, "20");
    EXPECT_GT(result.confidence, 60.0f);
    EXPECT_FALSE(result.filename.empty());
}

TEST(OcrEngine, PreservesOriginalFilenameInResult) {
    // เคสจริงที่ user ยกมา ชื่อไฟล์แปลก ๆ ก็ยังเก็บ trace กลับมาได้
    OcrEngine engine;
    const std::string oddFilename = testDataPath("xZ9k_3f8a1.png");

    const auto result = engine.recognize(oddFilename);

    EXPECT_EQ(result.filename, oddFilename);
}

TEST(OcrEngine, ThrowsWhenImageMissing) {
    OcrEngine engine;
    EXPECT_THROW(engine.recognize("C:/nonexistent/path.png"), std::runtime_error);
}

TEST(OcrEngine, HandlesUnicodeFilename) {
    // cv::imread บน Windows ไม่ handle UTF-8 path → ต้องใช้ ifstream + imdecode
    // เคสจริงผู้ใช้อาจมีไฟล์ภาพ filename ภาษาไทย
    OcrEngine engine;
    const auto path = testDataPath("ไทย_20.png");

    EXPECT_NO_THROW({
        const auto result = engine.recognize(path);
        EXPECT_EQ(result.filename, path);
        EXPECT_EQ(result.digits, "20");
    });
}
