#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include "ocr/OcrEngine.h"
#include "ocr/OcrFormatter.h"

using autopilot::ocr::OcrFormatter;
using autopilot::ocr::OcrResult;
using nlohmann::json;

namespace {

OcrResult sample() {
    return OcrResult{
        .filename = "C:/tmp/odd_name.png",
        .text = "Value: 20",
        .digits = "20",
        .confidence = 87.5f,
    };
}

}  // namespace

TEST(OcrFormatter, ProducesValidJsonWithExpectedKeys) {
    const auto serialized = OcrFormatter::toJson(sample(), "eng");
    const auto parsed = json::parse(serialized);

    EXPECT_EQ(parsed["filename"], "C:/tmp/odd_name.png");
    EXPECT_EQ(parsed["text"], "Value: 20");
    EXPECT_EQ(parsed["digits"], "20");
    EXPECT_FLOAT_EQ(parsed["confidence"].get<float>(), 87.5f);
    EXPECT_EQ(parsed["language"], "eng");
}

TEST(OcrFormatter, EscapesSpecialCharactersSafely) {
    OcrResult r{
        .filename = R"(C:\path\with"quote.png)",
        .text = "line1\nline2 \"quoted\" \\back",
        .digits = "",
        .confidence = 0.0f,
    };

    const auto serialized = OcrFormatter::toJson(r, "eng");
    EXPECT_NO_THROW(json::parse(serialized));

    const auto parsed = json::parse(serialized);
    EXPECT_EQ(parsed["text"].get<std::string>(), "line1\nline2 \"quoted\" \\back");
}

TEST(OcrFormatter, PreservesUnicodeFilename) {
    OcrResult r{
        .filename = "C:/tmp/ไทย_20.png",
        .text = "20",
        .digits = "20",
        .confidence = 90.0f,
    };
    const auto serialized = OcrFormatter::toJson(r, "tha");
    const auto parsed = json::parse(serialized);

    EXPECT_EQ(parsed["filename"].get<std::string>(), "C:/tmp/ไทย_20.png");
}
