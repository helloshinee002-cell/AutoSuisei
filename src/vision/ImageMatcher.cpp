#include "ImageMatcher.h"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <spdlog/spdlog.h>

namespace autopilot::vision {

namespace {

cv::Mat loadImageUnicodeSafe(const std::string& path) {
    const auto p = std::filesystem::u8path(path);
    if (!std::filesystem::exists(p)) {
        throw std::runtime_error("ไม่พบไฟล์ภาพ: " + path);
    }
    std::ifstream stream(p, std::ios::binary);
    if (!stream) throw std::runtime_error("เปิดไฟล์ภาพไม่ได้: " + path);

    const auto size = std::filesystem::file_size(p);
    std::vector<unsigned char> buf(static_cast<std::size_t>(size));
    stream.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(size));

    cv::Mat img = cv::imdecode(buf, cv::IMREAD_COLOR);
    if (img.empty()) throw std::runtime_error("ถอดรหัสภาพไม่ได้: " + path);
    return img;
}

}  // namespace

std::optional<MatchResult> ImageMatcher::matchFiles(const std::string& haystackPath,
                                                   const std::string& needlePath,
                                                   const MatchOptions& options) {
    cv::Mat haystack = loadImageUnicodeSafe(haystackPath);
    cv::Mat needle = loadImageUnicodeSafe(needlePath);

    if (needle.cols > haystack.cols || needle.rows > haystack.rows) {
        throw std::runtime_error("ImageMatcher: needle ใหญ่กว่า haystack");
    }

    if (options.grayscale) {
        cv::cvtColor(haystack, haystack, cv::COLOR_BGR2GRAY);
        cv::cvtColor(needle, needle, cv::COLOR_BGR2GRAY);
    }

    cv::Mat scores;
    cv::matchTemplate(haystack, needle, scores, cv::TM_CCOEFF_NORMED);

    double maxVal = 0;
    cv::Point maxLoc;
    cv::minMaxLoc(scores, nullptr, &maxVal, nullptr, &maxLoc);

    spdlog::debug("ImageMatcher: best score={} at ({},{})", maxVal, maxLoc.x, maxLoc.y);

    if (maxVal < static_cast<double>(options.minConfidence)) {
        return std::nullopt;
    }

    return MatchResult{
        .x = maxLoc.x,
        .y = maxLoc.y,
        .width = needle.cols,
        .height = needle.rows,
        .confidence = static_cast<float>(maxVal),
    };
}

}  // namespace autopilot::vision
