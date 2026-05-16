#include "OcrEngine.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <leptonica/allheaders.h>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <spdlog/spdlog.h>
#include <tesseract/baseapi.h>

namespace autopilot::ocr {

namespace {

/** Pipeline preprocess default: grayscale → adaptive threshold → median blur */
cv::Mat preprocess(const cv::Mat& src) {
    cv::Mat gray;
    if (src.channels() == 1) {
        gray = src.clone();
    } else {
        cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    }

    cv::Mat thresh;
    cv::adaptiveThreshold(gray, thresh, 255, cv::ADAPTIVE_THRESH_GAUSSIAN_C,
                          cv::THRESH_BINARY, 31, 10);
    cv::Mat denoised;
    cv::medianBlur(thresh, denoised, 3);
    return denoised;
}

std::string trim(std::string s) {
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    return s;
}

std::string extractDigits(const std::string& text) {
    std::string digits;
    digits.reserve(text.size());
    for (char c : text) {
        if (std::isdigit(static_cast<unsigned char>(c))) {
            digits.push_back(c);
        }
    }
    return digits;
}

constexpr std::size_t kMaxImageBytes = 50ull * 1024ull * 1024ull;

/**
 * Load image with unicode-safe path handling
 * cv::imread บน Windows รับ std::string ที่ตีความเป็น ANSI ทำให้ filename
 * ภาษาไทย/จีน ฯลฯ เปิดไม่ได้ — ใช้ std::filesystem::path + ifstream + imdecode แทน
 */
cv::Mat loadImage(const std::string& imagePath) {
    const std::filesystem::path path = std::filesystem::u8path(imagePath);
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("ไม่พบไฟล์ภาพ: " + imagePath);
    }

    const auto fileSize = std::filesystem::file_size(path);
    if (fileSize == 0) {
        throw std::runtime_error("ไฟล์ภาพว่าง: " + imagePath);
    }
    if (fileSize > kMaxImageBytes) {
        throw std::runtime_error("ไฟล์ภาพใหญ่เกิน 50MB: " + imagePath);
    }

    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("เปิดไฟล์ภาพไม่ได้: " + imagePath);
    }

    std::vector<unsigned char> buffer(static_cast<std::size_t>(fileSize));
    stream.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(fileSize));
    if (!stream) {
        throw std::runtime_error("อ่านไฟล์ภาพไม่ครบ: " + imagePath);
    }

    cv::Mat decoded = cv::imdecode(buffer, cv::IMREAD_COLOR);
    if (decoded.empty()) {
        throw std::runtime_error("ถอดรหัสภาพไม่ได้ (รูปแบบไม่รองรับหรือไฟล์เสีย): " + imagePath);
    }
    return decoded;
}

}  // namespace

struct OcrEngine::Impl {
    OcrOptions options;
    tesseract::TessBaseAPI api;
    bool initialized{false};

    void ensureInit() {
        if (initialized) return;
        if (api.Init(nullptr, options.languages.c_str()) != 0) {
            throw std::runtime_error("Tesseract init failed for langs=" + options.languages);
        }
        api.SetPageSegMode(static_cast<tesseract::PageSegMode>(options.pageSegMode));
        initialized = true;
    }
};

OcrEngine::OcrEngine(OcrOptions options) : impl_(std::make_unique<Impl>()) {
    impl_->options = std::move(options);
}

OcrEngine::~OcrEngine() {
    if (impl_ && impl_->initialized) {
        impl_->api.End();
    }
}

OcrEngine::OcrEngine(OcrEngine&&) noexcept = default;
OcrEngine& OcrEngine::operator=(OcrEngine&&) noexcept = default;

OcrResult OcrEngine::recognize(const std::string& imagePath) {
    impl_->ensureInit();

    cv::Mat raw = loadImage(imagePath);
    cv::Mat processed = impl_->options.autoPreprocess ? preprocess(raw) : raw;

    impl_->api.SetImage(processed.data,
                        processed.cols,
                        processed.rows,
                        static_cast<int>(processed.elemSize()),
                        static_cast<int>(processed.step));

    std::unique_ptr<char[]> outText(impl_->api.GetUTF8Text());
    const float confidence = static_cast<float>(impl_->api.MeanTextConf());
    const std::string text = trim(outText ? outText.get() : "");

    spdlog::info("OCR {}: '{}' (confidence={})", imagePath, text, confidence);

    return OcrResult{
        .filename = imagePath,
        .text = text,
        .digits = extractDigits(text),
        .confidence = confidence,
    };
}

OcrResult OcrEngine::recognizeBytes(const unsigned char* data,
                                    int width,
                                    int height,
                                    int bytesPerPixel,
                                    const std::string& sourceLabel) {
    impl_->ensureInit();

    if (!data || width <= 0 || height <= 0 || bytesPerPixel <= 0) {
        throw std::runtime_error("recognizeBytes: invalid arguments");
    }

    const int cvType = (bytesPerPixel == 1) ? CV_8UC1
                       : (bytesPerPixel == 3) ? CV_8UC3
                       : (bytesPerPixel == 4) ? CV_8UC4
                                              : -1;
    if (cvType < 0) {
        throw std::runtime_error("recognizeBytes: unsupported bytesPerPixel");
    }

    cv::Mat raw(height, width, cvType, const_cast<unsigned char*>(data));
    cv::Mat processed = impl_->options.autoPreprocess ? preprocess(raw) : raw;

    impl_->api.SetImage(processed.data,
                        processed.cols,
                        processed.rows,
                        static_cast<int>(processed.elemSize()),
                        static_cast<int>(processed.step));

    std::unique_ptr<char[]> outText(impl_->api.GetUTF8Text());
    const float confidence = static_cast<float>(impl_->api.MeanTextConf());
    const std::string text = trim(outText ? outText.get() : "");

    return OcrResult{
        .filename = sourceLabel,
        .text = text,
        .digits = extractDigits(text),
        .confidence = confidence,
    };
}

}  // namespace autopilot::ocr
