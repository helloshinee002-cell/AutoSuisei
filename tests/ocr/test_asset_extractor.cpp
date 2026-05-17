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

// =================== Phase 9.2: lone-digit fallback ===================
// ก่อนหน้านี้ AssetExtractor จับ PC No. ได้แค่เมื่อมี "no" prefix (เช่น "pc no.45")
// Sticker photos แสดงเลขใหญ่ไม่มี prefix (45, 48, 49, 93, 103 ใน Downloads\OK)
// และ Notepad dark mode (22, 28, 54) ที่ user พิมพ์แค่เลขก็ขาด prefix
// → ต้อง fallback ดึงเลขจาก line ที่เป็นตัวเลขล้วน 1-3 หลัก

TEST(AssetExtractor, ParsesStandaloneDigitFromStickerOcr) {
    // เลียนแบบ PaddleOCR output ของภาพ sticker "315" ใน Train2_10.jpg
    const std::string ocr = "Latitude 5290 2-in-1\n315\nS/N 6YW8RV2\nDell Latitude E5290";
    EXPECT_EQ(AssetExtractor::parsePcNoFromText(ocr), "315");
}

TEST(AssetExtractor, ParsesStandaloneDigitFromDarkNotepad) {
    // ground truth จาก Downloads\OK\22-1.png / 28-1.png / 54-1.png
    EXPECT_EQ(AssetExtractor::parsePcNoFromText("File Edit View\n22"), "22");
    EXPECT_EQ(AssetExtractor::parsePcNoFromText("File Edit View\n28"), "28");
    EXPECT_EQ(AssetExtractor::parsePcNoFromText("File Edit View\n54"), "54");
}

TEST(AssetExtractor, PrimaryPatternStillWinsOverFallback) {
    // ห้ามให้ fallback ทับ primary — ถ้ามี "no.NN" ต้องเอา NN ของ primary
    EXPECT_EQ(AssetExtractor::parsePcNoFromText("999\npc no.45\n111"), "45");
}

TEST(AssetExtractor, RejectsFourDigitLineAsLoneDigit) {
    // 4 หลักมักเป็นรุ่น (Latitude 5290), ปี (2026), หรือเลข version — ไม่ใช่ PC No.
    EXPECT_EQ(AssetExtractor::parsePcNoFromText("Latitude\n5290\n2-in-1"), "");
}

TEST(AssetExtractor, RejectsKilldiskProgressPercent) {
    // "52% complete" — PaddleOCR อาจแยก "52" เป็น line เดียว แต่บรรทัดเดิมต้องมี "%"
    // เคสจริง: ทั้งบรรทัดเป็น "52% complete" → ไม่ match standalone digit
    EXPECT_EQ(AssetExtractor::parsePcNoFromText("Erasing\n52% complete\n00:40:52 elapsed"), "");
}

TEST(AssetExtractor, RejectsHexOffsetAsLoneDigit) {
    // "0x00000000" ไม่ใช่ pure digit (มี 'x') — ไม่ match
    EXPECT_EQ(AssetExtractor::parsePcNoFromText("One Pass\n0x00000000"), "");
}

TEST(AssetExtractor, RejectsZeroAndSingleDigitArtifacts) {
    // เลข 0 เดี่ยว มักเป็น OCR noise (จุดหรือสัญลักษณ์)
    // single digit เป็นไปได้แต่ noise สูง — ผ่อนผันที่ 1-3 หลัก (ไม่กรอง single digit)
    // เคสนี้: "0" เดี่ยวไม่ควรเป็น PC No.
    EXPECT_EQ(AssetExtractor::parsePcNoFromText("foo bar\n0\nbaz"), "");
}

TEST(AssetExtractor, AcceptsTwoDigitFromOkGroundTruth) {
    // OK folder: 10, 14, 16, 22, 28, 29, 36, 45, 48, 49, 51, 54, 57, 93, 95
    // ทุกเลขในนี้ 2-3 หลัก
    EXPECT_EQ(AssetExtractor::parsePcNoFromText("45"), "45");
    EXPECT_EQ(AssetExtractor::parsePcNoFromText("103"), "103");
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

// =================== false-positive blocklist (Phase 9.1) ===================
// killdisk UI text เคยถูกจับเป็น Dell serial ผิด — ภาพ baseline จาก Train
// แสดง "Erasing PhysicalDrive0 One Pass 1 of 1" + "Disk 0 C1 boot"
// ทำให้ standalone-alphanum-7 fallback ดึง "PASS1OF" และ "DISK0C1" ออกมา

TEST(AssetExtractor, RejectsKilldiskPassNOfArtifact) {
    EXPECT_EQ(AssetExtractor::parseSerialFromText(
                  "Erasing PhysicalDrive0 One Pass 1 of 1 (0x00000000)"),
              "");
}

TEST(AssetExtractor, RejectsKilldiskDiskLabelArtifact) {
    EXPECT_EQ(AssetExtractor::parseSerialFromText("Disk 0 C1 boot NTFS"), "");
}

TEST(AssetExtractor, RequiresMinTwoDigitsInStandaloneFallback) {
    // Real Dell tags มี digit อย่างน้อย 2 ตัวเสมอ ("8N6QQ02"=3, "7VSP9M2"=2, "6JXXFL2"=2)
    // 1-digit candidates มักเป็น English words ที่ OCR สลับตัวเลข
    EXPECT_EQ(AssetExtractor::parseSerialFromText("ABCDEF1 floating"), "");
    EXPECT_EQ(AssetExtractor::parseSerialFromText("PASS1OF junk"), "");
}

TEST(AssetExtractor, RequiresMinThreeAlphasInStandaloneFallback) {
    // กัน "1234ABC"-style ที่ digit เกินครึ่งและไม่ใช่ pattern Dell
    // (Dell tags จริงมี alpha >= 3 เสมอ จาก sample: 8N6QQ02=4, 7VSP9M2=4, 37GFH32=3)
    EXPECT_EQ(AssetExtractor::parseSerialFromText("12345AB random"), "");
}

TEST(AssetExtractor, StillAcceptsRealDellTagsFromOkFolder) {
    // ground truth จาก Downloads\OK\ — sticker photos
    EXPECT_EQ(AssetExtractor::parseSerialFromText("SERVICE TAG (S/N) 7VSP9M2"), "7VSP9M2");
    EXPECT_EQ(AssetExtractor::parseSerialFromText("SERVICE TAG (S/N) 6JXXFL2"), "6JXXFL2");
    // SN:XXXXXXX จาก Notepad screen (113-1.png / 120-1.png)
    EXPECT_EQ(AssetExtractor::parseSerialFromText("SN:7DBWSF2"), "7DBWSF2");
    EXPECT_EQ(AssetExtractor::parseSerialFromText("SN:37GFH32"), "37GFH32");
}
