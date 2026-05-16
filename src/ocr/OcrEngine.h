#pragma once

#include <memory>
#include <string>

namespace autopilot::ocr {

struct OcrOptions {
    std::string languages{"eng"};  ///< Tesseract codes คั่นด้วย "+" (เช่น "eng+tha")
    int pageSegMode{6};            ///< default = "Assume a single uniform block of text"
    bool autoPreprocess{true};     ///< OpenCV grayscale + adaptive threshold + denoise
};

/**
 * ผลลัพธ์ของการอ่านภาพหนึ่งไฟล์
 *   - filename: path ของไฟล์ที่ส่งเข้ามา (เก็บไว้สำหรับเคส "ชื่อไฟล์แปลก เนื้อในรูปเป็นเลข")
 *   - text:     ข้อความทั้งหมดที่ Tesseract อ่านได้ trim whitespace แล้ว
 *   - digits:   เฉพาะตัวเลขที่กรองออกมา (สะดวกตามเคส request)
 *   - confidence: 0..100
 */
struct OcrResult {
    std::string filename;
    std::string text;
    std::string digits;
    float confidence{0.0f};
};

/**
 * OcrEngine = facade ครอบ Tesseract + OpenCV preprocess
 *   - thread-safe ในระดับ instance เดียวห้ามใช้พร้อมกันจากหลาย thread (Tesseract API ไม่ใช่ thread-safe)
 *   - ใช้ object pool ถ้าต้องการ parallel (ดู OcrEnginePool — TODO)
 */
class OcrEngine {
public:
    explicit OcrEngine(OcrOptions options = {});
    ~OcrEngine();

    OcrEngine(const OcrEngine&) = delete;
    OcrEngine& operator=(const OcrEngine&) = delete;
    OcrEngine(OcrEngine&&) noexcept;
    OcrEngine& operator=(OcrEngine&&) noexcept;

    /**
     * อ่านภาพจาก path คืน OcrResult
     * @throws std::runtime_error ถ้าไฟล์เปิดไม่ได้หรือ Tesseract init fail
     */
    [[nodiscard]] OcrResult recognize(const std::string& imagePath);

    /** อ่านภาพในหน่วยความจำ (raw RGB bytes) — สำหรับ screenshot pipeline */
    [[nodiscard]] OcrResult recognizeBytes(const unsigned char* data,
                                           int width,
                                           int height,
                                           int bytesPerPixel,
                                           const std::string& sourceLabel = "<memory>");

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace autopilot::ocr
