#include <spdlog/spdlog.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "ocr/AssetExtractor.h"
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

bool looksLikeImage(const std::filesystem::path& p) {
    static const std::vector<std::string> exts = {".jpg", ".jpeg", ".png",
                                                  ".bmp", ".tif", ".tiff", ".webp"};
    auto ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    for (const auto& e : exts) {
        if (ext == e) return true;
    }
    return false;
}

std::string csvEscape(const std::string& s) {
    if (s.find_first_of(",\"\n\r") == std::string::npos) return s;
    std::string out = "\"";
    for (char c : s) {
        if (c == '"') out += "\"\"";
        else out += c;
    }
    out += "\"";
    return out;
}

int runExtractFolder(const std::string& folder, const std::string& csvOut,
                     const std::string& languages) {
    namespace fs = std::filesystem;
    if (!fs::is_directory(folder)) {
        spdlog::error("Not a directory: {}", folder);
        return 2;
    }

    std::vector<fs::path> images;
    for (const auto& entry : fs::directory_iterator(folder)) {
        if (entry.is_regular_file() && looksLikeImage(entry.path())) {
            images.push_back(entry.path());
        }
    }
    std::sort(images.begin(), images.end());
    spdlog::info("Found {} images in {}", images.size(), folder);

    autopilot::ocr::OcrEngine engine(
        autopilot::ocr::OcrOptions{.languages = languages, .pageSegMode = 6});
    autopilot::ocr::AssetExtractor extractor(engine);

    std::ofstream out(csvOut);
    if (!out) {
        spdlog::error("Cannot open output CSV: {}", csvOut);
        return 2;
    }
    out << "photo_index,filename,pc_no,serial_no,batch_id,photo_date,pc_range,"
           "ocr_confidence,warnings\n";

    int withPcNo = 0;
    int withSerial = 0;
    int idx = 0;
    for (const auto& p : images) {
        ++idx;
        if (idx % 10 == 0) {
            std::cerr << "  [" << idx << "/" << images.size() << "] processing...\n";
        }
        const auto info = extractor.extract(p.string());
        if (!info.pcNo.empty()) ++withPcNo;
        if (!info.serialNo.empty()) ++withSerial;

        std::string warnings;
        for (const auto& w : info.warnings) {
            if (!warnings.empty()) warnings += "; ";
            warnings += w;
        }

        out << info.photoIndex << "," << csvEscape(p.filename().string()) << ","
            << csvEscape(info.pcNo) << "," << csvEscape(info.serialNo) << ","
            << csvEscape(info.batchId) << "," << csvEscape(info.photoDate) << ","
            << csvEscape(info.pcRange) << "," << info.ocrConfidence << ","
            << csvEscape(warnings) << "\n";
    }
    out.close();

    std::cerr << "\n=== Summary ===\n"
              << "Total photos: " << images.size() << "\n"
              << "No. extracted: " << withPcNo << " ("
              << (images.empty() ? 0 : 100 * withPcNo / static_cast<int>(images.size()))
              << "%)\n"
              << "Serial extracted: " << withSerial << " ("
              << (images.empty() ? 0 : 100 * withSerial / static_cast<int>(images.size()))
              << "%)\n"
              << "CSV: " << csvOut << "\n";
    return 0;
}

void printUsage() {
    std::cout << "AutoSuisei CLI\n"
              << "  autopilot_cli ocr <image-path> [languages]\n"
              << "      OCR ภาพเดียว คืน JSON\n"
              << "  autopilot_cli extract-folder <dir> <out.csv> [languages]\n"
              << "      Bulk OCR + extract No./Serial → CSV\n"
              << "  autopilot_cli play <macro-id>     (TODO Phase 2)\n"
              << "  autopilot_cli record <name>       (TODO Phase 2)\n";
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
    if (cmd == "extract-folder" && argc >= 4) {
        const std::string languages = (argc >= 5) ? argv[4] : "eng";
        return runExtractFolder(argv[2], argv[3], languages);
    }

    printUsage();
    return 1;
}
