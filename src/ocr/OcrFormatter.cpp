#include "OcrFormatter.h"

#include <nlohmann/json.hpp>

namespace autopilot::ocr {

std::string OcrFormatter::toJson(const OcrResult& result, const std::string& language) {
    nlohmann::json j;
    j["filename"] = result.filename;
    j["text"] = result.text;
    j["digits"] = result.digits;
    j["confidence"] = result.confidence;
    j["language"] = language;
    return j.dump(2);
}

}  // namespace autopilot::ocr
