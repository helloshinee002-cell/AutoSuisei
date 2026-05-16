#pragma once

#include <string>

#include "OcrEngine.h"

namespace autopilot::ocr {

/**
 * แปลง OcrResult เป็น JSON string ที่ valid (escape ครบ)
 * แยกออกจาก CLI เพื่อให้ unit-test ได้โดยไม่ต้อง spawn process
 */
class OcrFormatter {
public:
    /**
     * @param result   ผลจาก OcrEngine::recognize
     * @param language Tesseract languages ที่ใช้ (เก็บลง JSON เพื่อ trace)
     * @return         JSON string ที่ parse ได้ด้วย nlohmann::json
     */
    [[nodiscard]] static std::string toJson(const OcrResult& result,
                                            const std::string& language);
};

}  // namespace autopilot::ocr
