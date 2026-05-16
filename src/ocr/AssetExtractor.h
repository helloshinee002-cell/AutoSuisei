#pragma once

#include <optional>
#include <string>
#include <vector>

#include "OcrEngine.h"

namespace autopilot::ocr {

/**
 * ข้อมูลที่ดึงจากภาพ PC asset (เคส killdisk wipe-off documentation)
 *
 * บางช่องอาจว่าง — extractor พยายามดึงทุกแหล่งที่หาได้:
 *   - filename → batchId, photoDate, photoIndex, pcRange
 *   - OCR text → pcNo (จาก "pc no.NN"), serialNo (Dell S/N pattern)
 */
struct AssetInfo {
    std::string filename;
    std::string pcNo;        ///< จาก "no.45" / "No 6" ใน Notepad
    std::string serialNo;    ///< 7-char alphanumeric (Dell service tag pattern)
    std::string batchId;     ///< จาก filename "(9269)"
    std::string photoDate;   ///< "YYYY-MM-DD" จาก filename "_YYMMDD_"
    int photoIndex{0};       ///< suffix "_NNN.jpg"
    std::string pcRange;     ///< "1-110" จาก filename "pc 1-110"
    float ocrConfidence{0};
    std::vector<std::string> warnings;
};

/**
 * AssetExtractor = OcrEngine + regex post-processing
 * Pure helpers static เพื่อ unit-test ได้โดยไม่ต้อง spin OCR
 */
class AssetExtractor {
public:
    explicit AssetExtractor(OcrEngine& engine);

    /** OCR ภาพ + parse filename → AssetInfo */
    [[nodiscard]] AssetInfo extract(const std::string& imagePath);

    // ---------------- pure helpers ----------------
    /** หา "no.NN" / "No 6" / "pc no.45" ใน text — case-insensitive, ผ่อนผัน whitespace */
    [[nodiscard]] static std::string parsePcNoFromText(const std::string& text);

    /** Dell service tag = 7 ตัว alphanumeric ตามหลัง "S/N" หรือ "SERVICE TAG" */
    [[nodiscard]] static std::string parseSerialFromText(const std::string& text);

    /** Batch id ใน "(9269)" pattern จาก filename */
    [[nodiscard]] static std::string parseBatchIdFromFilename(const std::string& filename);

    /** YYMMDD pattern → "YYYY-MM-DD" (2020s) */
    [[nodiscard]] static std::string parseDateFromFilename(const std::string& filename);

    /** "..._50.jpg" → 50 */
    [[nodiscard]] static int parsePhotoIndexFromFilename(const std::string& filename);

    /** "pc 1-110" → "1-110" */
    [[nodiscard]] static std::string parsePcRangeFromFilename(const std::string& filename);

private:
    OcrEngine& engine_;
};

}  // namespace autopilot::ocr
