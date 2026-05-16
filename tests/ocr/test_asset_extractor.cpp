#include <gtest/gtest.h>

#include "ocr/AssetExtractor.h"

using autopilot::ocr::AssetExtractor;

// =================== filename parsers ===================

TEST(AssetExtractor, ParsesBatchIdFromParenthesisInFilename) {
    EXPECT_EQ(AssetExtractor::parseBatchIdFromFilename(
                  "LINE_ALBUM_killdisk set2 (9269) pc 1-110_260516_50.jpg"),
              "9269");
    EXPECT_EQ(AssetExtractor::parseBatchIdFromFilename("noparens.jpg"), "");
}

TEST(AssetExtractor, ParsesDateFromYYMMDDInFilename) {
    EXPECT_EQ(AssetExtractor::parseDateFromFilename(
                  "LINE_ALBUM_killdisk set2 (9269) pc 1-110_260516_50.jpg"),
              "2026-05-16");
}

TEST(AssetExtractor, ParsesPhotoIndexFromFilename) {
    EXPECT_EQ(AssetExtractor::parsePhotoIndexFromFilename(
                  "LINE_ALBUM_killdisk set2 (9269) pc 1-110_260516_50.jpg"),
              50);
    EXPECT_EQ(AssetExtractor::parsePhotoIndexFromFilename("foo.JPG"), 0);
}

TEST(AssetExtractor, ParsesPcRangeFromFilename) {
    EXPECT_EQ(AssetExtractor::parsePcRangeFromFilename(
                  "LINE_ALBUM_killdisk set2 (9269) pc 1-110_260516_50.jpg"),
              "1-110");
}

// =================== PC No. regex (the headline use case) ===================

TEST(AssetExtractor, ParsesPcNoFromHandwrittenNotation) {
    EXPECT_EQ(AssetExtractor::parsePcNoFromText("pc no.45 done"), "45");
    EXPECT_EQ(AssetExtractor::parsePcNoFromText("No 6"), "6");
    EXPECT_EQ(AssetExtractor::parsePcNoFromText("no. 109"), "109");
    EXPECT_EQ(AssetExtractor::parsePcNoFromText("PC NO 7"), "7");
    // PaddleOCR เคยอ่าน handwriting "no.18" เป็น "no-18" — รองรับด้วย
    EXPECT_EQ(AssetExtractor::parsePcNoFromText("no-18 in notepad"), "18");
}

TEST(AssetExtractor, ParsesPcNoBuriedInNoise) {
    // Tesseract output ของจริงเต็มไปด้วย noise — pattern ต้องทนทาน
    const std::string noisy =
        "12.45 PM total progress\n... pc no.45 ... lol garbage\nfoo bar 9269";
    EXPECT_EQ(AssetExtractor::parsePcNoFromText(noisy), "45");
}

TEST(AssetExtractor, ReturnsEmptyWhenNoPcNoPresent) {
    EXPECT_EQ(AssetExtractor::parsePcNoFromText("nothing to see here"), "");
}

// =================== Dell serial / service tag ===================

TEST(AssetExtractor, ParsesSerialAfterSlashNLabel) {
    EXPECT_EQ(AssetExtractor::parseSerialFromText(
                  "SERVICE TAG (S/N) 8N6QQ02 EXPRESS SERVICE CODE 18816305186"),
              "8N6QQ02");
}

TEST(AssetExtractor, ParsesSerialWithLowercaseInput) {
    EXPECT_EQ(AssetExtractor::parseSerialFromText("service tag s/n 8n6qq02"), "8N6QQ02");
}

TEST(AssetExtractor, FallsBackToStandaloneAlphaNumeric7Chars) {
    EXPECT_EQ(AssetExtractor::parseSerialFromText("random 8N6QQ02 floating"), "8N6QQ02");
}

TEST(AssetExtractor, RejectsAllDigit7CharBlock) {
    // ต้องไม่ match "1234567" เพราะ Dell tag ต้องมีตัวอักษรปน
    EXPECT_EQ(AssetExtractor::parseSerialFromText("plain 1234567 digits"), "");
}
