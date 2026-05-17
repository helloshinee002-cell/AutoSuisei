#include "AssetExtractor.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <regex>
#include <string_view>

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

// killdisk / Windows UI artifacts ที่เคยหลุดผ่าน standalone-alphanum-7 fallback
// เป็น Dell serial — รายการนี้เติมจาก false positives ที่พบใน 232 ภาพ baseline
constexpr std::array<std::string_view, 8> kSerialBlocklist{{
    "PASS1OF",   // "One Pass 1 Of 1" killdisk progress
    "DISK0C1",   // "Disk 0 C1" boot label
    "DRIVE00",   // "PhysicalDrive 0 0" fragment
    "NTFSSIZ",   // "NTFS Size" header
    "BOOTXOF",   // "Boot X of" placeholder
    "ELAPSED",   // 7-char status word
    "REMOVE0",   // "Removable 0" disk header
    "FIXED00",   // "Fixed 0 0" disk header
}};

bool isBlocklistedSerial(std::string_view candidate) {
    return std::find(kSerialBlocklist.begin(), kSerialBlocklist.end(), candidate) !=
           kSerialBlocklist.end();
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
    // label มีหลายรูป: "S/N XXXXXXX", "(S/N) XXXXXXX", "SN:XXXXXXX", "SERVICE TAG XXXXXXX"
    static const std::regex labeled{
        R"((?:S\s*[/\\]?\s*N|SERVICE\s*TAG)\s*\)?\s*[:.]?\s*([A-Z0-9]{7})\b)"};
    const auto upper = toUpper(text);
    std::smatch m;
    if (std::regex_search(upper, m, labeled)) {
        const auto candidate = m[1].str();
        if (!isBlocklistedSerial(candidate)) {
            return candidate;
        }
    }

    // fallback: standalone 7-char block ที่มีทั้งตัวอักษร+ตัวเลข (กัน false-positive จากเลข)
    // ใน OK ground truth ทุก Dell tag มี digit >= 2 และ alpha >= 3 → ใช้เป็น threshold
    // กัน UI artifact 1-digit เช่น "PASS1OF"/"NTFSSIZ" และ digit-heavy เช่น "12345AB"
    static const std::regex standalone{R"(\b([A-Z0-9]{7})\b)"};
    auto it = std::sregex_iterator(upper.begin(), upper.end(), standalone);
    for (; it != std::sregex_iterator(); ++it) {
        const auto candidate = (*it)[1].str();
        const int alphas = static_cast<int>(std::count_if(
            candidate.begin(), candidate.end(),
            [](char c) { return std::isalpha(static_cast<unsigned char>(c)); }));
        const int digits = static_cast<int>(std::count_if(
            candidate.begin(), candidate.end(),
            [](char c) { return std::isdigit(static_cast<unsigned char>(c)); }));
        if (alphas >= 3 && digits >= 2 && !isBlocklistedSerial(candidate)) {
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
