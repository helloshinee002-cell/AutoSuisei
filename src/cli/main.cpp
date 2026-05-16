#include <spdlog/spdlog.h>

#include <iostream>
#include <string>

#include "ocr/OcrEngine.h"
#include "ocr/OcrFormatter.h"

namespace {

int runOcr(const std::string& imagePath, const std::string& languages) {
    try {
        autopilot::ocr::OcrEngine engine(autopilot::ocr::OcrOptions{.languages = languages});
        const auto result = engine.recognize(imagePath);
        std::cout << autopilot::ocr::OcrFormatter::toJson(result, languages) << "\n";
        return 0;
    } catch (const std::exception& e) {
        spdlog::error("OCR failed: {}", e.what());
        return 2;
    }
}

void printUsage() {
    std::cout << "AutoPilot CLI\n"
              << "  autopilot_cli ocr <image-path> [languages]   อ่านภาพ คืน JSON\n"
              << "                                               languages default: eng+tha\n"
              << "  autopilot_cli play <macro-id>                เล่น macro (TODO Phase 2)\n"
              << "  autopilot_cli record <name>                  บันทึก macro (TODO Phase 2)\n";
}

}  // namespace

int main(int argc, char* argv[]) {
    spdlog::set_level(spdlog::level::info);

    if (argc < 2) {
        printUsage();
        return 1;
    }

    const std::string cmd = argv[1];
    if (cmd == "ocr" && argc >= 3) {
        const std::string languages = (argc >= 4) ? argv[3] : "eng+tha";
        return runOcr(argv[2], languages);
    }

    printUsage();
    return 1;
}
