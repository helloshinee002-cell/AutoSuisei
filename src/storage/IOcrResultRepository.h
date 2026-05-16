#pragma once

#include <optional>
#include <string>
#include <vector>

namespace autopilot::storage {

/** ผลลัพธ์ OCR ที่เก็บลง DB — ตรงกับเคสที่ผู้ใช้ขอ: ผูกชื่อไฟล์กับข้อความที่อ่านได้ */
struct StoredOcrResult {
    std::int64_t id{0};
    std::string filename;       ///< absolute path ของไฟล์ภาพ
    std::string extractedText;  ///< ข้อความ/เลขที่ OCR อ่านได้
    float confidence{0.0f};     ///< 0..100 จาก Tesseract
    std::int64_t timestampMs{0};
};

class IOcrResultRepository {
public:
    virtual ~IOcrResultRepository() = default;

    virtual std::int64_t insert(const StoredOcrResult& result) = 0;
    [[nodiscard]] virtual std::vector<StoredOcrResult> findByFilename(
        const std::string& filename) const = 0;
    [[nodiscard]] virtual std::vector<StoredOcrResult> findAll() const = 0;
};

}  // namespace autopilot::storage
