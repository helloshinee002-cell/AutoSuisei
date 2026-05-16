#include "AssetExtractor.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <regex>

namespace autopilot::ocr {

namespace {

std::string toUpper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

std::string basenameOf(const std::string& path) {
    return std::filesystem::u8path(path).filename().string();
}

}  // namespace

AssetExtractor::AssetExtractor(OcrEngine& engine) : engine_(engine) {}

std::string AssetExtractor::parsePcNoFromText(const std::string& text) {
    // "no.45", "No 6", "no. 45", "pc no.45", "N°45", "no-18" (OCR may misread . as -)
    // → จับเลข 1-4 หลักหลัง "no" + separator
    static const std::regex re{
        R"((?:^|[^A-Za-z])[Nn][Oo°][\.\-\s:]*([0-9]{1,4})\b)"};
    std::smatch m;
    if (std::regex_search(text, m, re)) {
        return m[1].str();
    }
    return "";
}

std::string AssetExtractor::parseSerialFromText(const std::string& text) {
    // Dell service tag = 7 chars alphanumeric ติดกัน
    // มัก label "S/N XXXXXXX" หรือ "(S/N) XXXXXXX"
    static const std::regex labeled{
        R"((?:S\s*[/\\]\s*N|SERVICE\s*TAG)\s*\)?\s*([A-Z0-9]{7})\b)"};
    const auto upper = toUpper(text);
    std::smatch m;
    if (std::regex_search(upper, m, labeled)) {
        return m[1].str();
    }

    // fallback: standalone 7-char block ที่มีทั้งตัวอักษร+ตัวเลข (กัน false-positive จากเลข)
    static const std::regex standalone{R"(\b([A-Z0-9]{7})\b)"};
    auto it = std::sregex_iterator(upper.begin(), upper.end(), standalone);
    for (; it != std::sregex_iterator(); ++it) {
        const auto candidate = (*it)[1].str();
        const bool hasAlpha = std::any_of(candidate.begin(), candidate.end(),
                                          [](char c) { return std::isalpha(static_cast<unsigned char>(c)); });
        const bool hasDigit = std::any_of(candidate.begin(), candidate.end(),
                                          [](char c) { return std::isdigit(static_cast<unsigned char>(c)); });
        if (hasAlpha && hasDigit) {
            return candidate;
        }
    }
    return "";
}

std::string AssetExtractor::parseBatchIdFromFilename(const std::string& filename) {
    static const std::regex re{R"(\((\d+)\))"};
    std::smatch m;
    const auto name = basenameOf(filename);
    if (std::regex_search(name, m, re)) {
        return m[1].str();
    }
    return "";
}

std::string AssetExtractor::parseDateFromFilename(const std::string& filename) {
    // "_260516_" → "2026-05-16" (assume 20YY)
    static const std::regex re{R"(_(\d{2})(\d{2})(\d{2})_)"};
    std::smatch m;
    const auto name = basenameOf(filename);
    if (std::regex_search(name, m, re)) {
        return "20" + m[1].str() + "-" + m[2].str() + "-" + m[3].str();
    }
    return "";
}

int AssetExtractor::parsePhotoIndexFromFilename(const std::string& filename) {
    static const std::regex re{R"(_(\d+)\.(?:jpg|jpeg|png|bmp|tif|tiff|webp)$)",
                                std::regex_constants::icase};
    std::smatch m;
    const auto name = basenameOf(filename);
    if (std::regex_search(name, m, re)) {
        try {
            return std::stoi(m[1].str());
        } catch (...) {
            return 0;
        }
    }
    return 0;
}

std::string AssetExtractor::parsePcRangeFromFilename(const std::string& filename) {
    static const std::regex re{R"(pc\s*(\d+\s*-\s*\d+))", std::regex_constants::icase};
    std::smatch m;
    const auto name = basenameOf(filename);
    if (std::regex_search(name, m, re)) {
        return m[1].str();
    }
    return "";
}

AssetInfo AssetExtractor::extract(const std::string& imagePath) {
    AssetInfo info;
    info.filename = imagePath;
    info.batchId = parseBatchIdFromFilename(imagePath);
    info.photoDate = parseDateFromFilename(imagePath);
    info.photoIndex = parsePhotoIndexFromFilename(imagePath);
    info.pcRange = parsePcRangeFromFilename(imagePath);

    try {
        const auto ocr = engine_.recognize(imagePath);
        info.ocrConfidence = ocr.confidence;
        info.pcNo = parsePcNoFromText(ocr.text);
        info.serialNo = parseSerialFromText(ocr.text);
    } catch (const std::exception& e) {
        info.warnings.push_back(std::string{"OCR failed: "} + e.what());
        return info;
    }

    if (info.pcNo.empty()) info.warnings.push_back("PC No. not found");
    if (info.serialNo.empty()) info.warnings.push_back("Serial not found");
    return info;
}

}  // namespace autopilot::ocr
