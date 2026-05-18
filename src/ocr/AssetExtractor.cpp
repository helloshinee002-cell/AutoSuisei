#include "AssetExtractor.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <regex>
#include <sstream>
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
    return parsePcNoFromText(text, "");
}

std::pair<int, int> AssetExtractor::parsePcRangeBounds(const std::string& rangeHint) {
    static const std::regex re{R"(\s*(\d+)\s*-\s*(\d+)\s*)"};
    std::smatch m;
    if (!std::regex_match(rangeHint, m, re)) return {0, 0};
    try {
        return {std::stoi(m[1].str()), std::stoi(m[2].str())};
    } catch (...) {
        return {0, 0};
    }
}

std::string AssetExtractor::parsePcNoFromText(const std::string& text,
                                              const std::string& rangeHint) {
    const auto [lo, hi] = parsePcRangeBounds(rangeHint);
    const bool haveRange = lo > 0 && hi >= lo;

    auto inRange = [&](const std::string& digits) -> bool {
        if (!haveRange) return true;
        try {
            const int n = std::stoi(digits);
            return n >= lo && n <= hi;
        } catch (...) { return false; }
    };

    // Primary: "no.45", "No 6", "no. 45", "pc no.45", "N°45", "no-18"
    // OCR เพี้ยนเช่น "Dell Inc. 1.21.0" → "Dell/no7.27.0" ทำให้ regex จับ "no7"
    // ก่อนที่จะเจอ "No.317" จริงๆ — Phase 9.5 iterate ทุก match แล้ว prefer in-range
    static const std::regex primary{
        R"((?:^|[^A-Za-z])[Nn][Oo°][\.\-\s:]*([0-9]{1,4})\b)"};
    std::string firstPrimary;
    auto pit = std::sregex_iterator(text.begin(), text.end(), primary);
    for (; pit != std::sregex_iterator(); ++pit) {
        const auto digits = (*pit)[1].str();
        if (firstPrimary.empty()) firstPrimary = digits;
        if (inRange(digits)) return digits;
    }
    if (!firstPrimary.empty() && !haveRange) return firstPrimary;
    // ถ้ามี range hint แต่ไม่มี primary match in-range → ลอง lone-digit ต่อ
    // (อย่ารีบ return firstPrimary ที่ out-of-range — lone-digit อาจมี in-range)

    // Fallback (Phase 9.2 + 9.5): บรรทัดที่เป็น 2-3 digit ล้วน
    static const std::regex standaloneDigit{R"(^\s*([0-9]{2,3})\s*$)"};
    std::string firstLone;
    std::stringstream ss(text);
    std::string line;
    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::smatch lm;
        if (!std::regex_match(line, lm, standaloneDigit)) continue;
        const auto digits = lm[1].str();
        if (firstLone.empty()) firstLone = digits;
        if (inRange(digits)) return digits;
    }

    // ลำดับ fallback ถ้าไม่มี in-range:
    //   1. primary match (เก่งกว่า lone-digit เพราะเห็น "no" prefix)
    //   2. lone-digit match
    if (!firstPrimary.empty()) return firstPrimary;
    return firstLone;
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
    // รองรับทั้ง "pc 1-110" (batch แรก) และ "Laptop 301-400" (Train2)
    static const std::regex re{R"((?:pc|laptop)\s*(\d+\s*-\s*\d+))",
                                std::regex_constants::icase};
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
        info.pcNo = parsePcNoFromText(ocr.text, info.pcRange);
        info.serialNo = parseSerialFromText(ocr.text);
    } catch (const std::exception& e) {
        info.warnings.push_back(std::string{"OCR failed: "} + e.what());
        return info;
    }

    if (info.pcNo.empty()) info.warnings.push_back("No. not found");
    if (info.serialNo.empty()) info.warnings.push_back("Serial not found");
    return info;
}

}  // namespace autopilot::ocr
