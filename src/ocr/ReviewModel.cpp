#include "ReviewModel.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace autopilot::ocr {

namespace {

std::string trimCr(std::string s) {
    if (!s.empty() && s.back() == '\r') s.pop_back();
    return s;
}

// header name → column index (lower-cased lookup)
std::unordered_map<std::string, std::size_t> indexHeader(
    const std::vector<std::string>& cols) {
    std::unordered_map<std::string, std::size_t> map;
    map.reserve(cols.size());
    for (std::size_t i = 0; i < cols.size(); ++i) {
        std::string key;
        key.reserve(cols[i].size());
        for (char c : cols[i]) {
            key.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
        map.emplace(std::move(key), i);
    }
    return map;
}

std::string fieldAt(const std::vector<std::string>& fields,
                    const std::unordered_map<std::string, std::size_t>& header,
                    const std::string& name) {
    auto it = header.find(name);
    if (it == header.end() || it->second >= fields.size()) return "";
    return fields[it->second];
}

bool parseBool(const std::string& s) {
    if (s.empty()) return false;
    const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(s[0])));
    return c == 't' || c == '1' || c == 'y';
}

}  // namespace

void ReviewModel::clear() { rows_.clear(); }

std::string ReviewModel::escapeCsv(const std::string& field) {
    const bool needs = field.find(',') != std::string::npos ||
                       field.find('"') != std::string::npos ||
                       field.find('\n') != std::string::npos ||
                       field.find('\r') != std::string::npos;
    if (!needs) return field;
    std::string out;
    out.reserve(field.size() + 2);
    out.push_back('"');
    for (char c : field) {
        if (c == '"') out.push_back('"');
        out.push_back(c);
    }
    out.push_back('"');
    return out;
}

std::vector<std::string> ReviewModel::parseCsvLine(const std::string& line) {
    std::vector<std::string> out;
    std::string field;
    field.reserve(line.size());
    bool inQuotes = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (inQuotes) {
            if (c == '"') {
                if (i + 1 < line.size() && line[i + 1] == '"') {
                    field.push_back('"');
                    ++i;
                } else {
                    inQuotes = false;
                }
            } else {
                field.push_back(c);
            }
        } else {
            if (c == '"') {
                inQuotes = true;
            } else if (c == ',') {
                out.push_back(std::move(field));
                field.clear();
            } else {
                field.push_back(c);
            }
        }
    }
    out.push_back(std::move(field));
    return out;
}

bool ReviewModel::loadCsv(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) return false;

    std::string line;
    if (!std::getline(in, line)) return false;  // empty file
    line = trimCr(std::move(line));
    if (line.empty()) return false;

    const auto headerFields = parseCsvLine(line);
    const auto header = indexHeader(headerFields);

    // required: ต้องมี filename อย่างน้อย — pc_no / serial_no อาจเป็น "" ก็ได้
    if (header.find("filename") == header.end()) return false;

    rows_.clear();
    while (std::getline(in, line)) {
        line = trimCr(std::move(line));
        if (line.empty()) continue;
        const auto fields = parseCsvLine(line);

        ReviewRow r;
        r.filename = fieldAt(fields, header, "filename");
        r.pcNo = fieldAt(fields, header, "pc_no");
        r.serialNo = fieldAt(fields, header, "serial_no");

        // resume case: original_* columns ที่เคย save ออกมา
        auto orig = fieldAt(fields, header, "original_pc_no");
        r.originalPcNo = orig.empty() ? r.pcNo : orig;
        auto origSn = fieldAt(fields, header, "original_serial_no");
        r.originalSerialNo = origSn.empty() ? r.serialNo : origSn;

        r.notes = fieldAt(fields, header, "notes");
        r.verified = parseBool(fieldAt(fields, header, "verified"));
        rows_.push_back(std::move(r));
    }
    return true;
}

bool ReviewModel::saveCsv(const std::string& path) const {
    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open()) return false;

    out << "filename,pc_no,serial_no,original_pc_no,original_serial_no,verified,notes\n";
    for (const auto& r : rows_) {
        out << escapeCsv(r.filename) << ','
            << escapeCsv(r.pcNo) << ','
            << escapeCsv(r.serialNo) << ','
            << escapeCsv(r.originalPcNo) << ','
            << escapeCsv(r.originalSerialNo) << ','
            << (r.verified ? "true" : "false") << ','
            << escapeCsv(r.notes) << '\n';
    }
    return out.good();
}

std::optional<ReviewRow> ReviewModel::at(std::size_t idx) const {
    if (idx >= rows_.size()) return std::nullopt;
    return rows_[idx];
}

bool ReviewModel::setRow(std::size_t idx, const ReviewRow& row) {
    if (idx >= rows_.size()) return false;
    rows_[idx] = row;
    return true;
}

std::optional<std::size_t> ReviewModel::nextUnverified(std::size_t from) const {
    for (std::size_t i = from; i < rows_.size(); ++i) {
        if (!rows_[i].verified) return i;
    }
    return std::nullopt;
}

std::size_t ReviewModel::verifiedCount() const noexcept {
    return static_cast<std::size_t>(
        std::count_if(rows_.begin(), rows_.end(),
                      [](const ReviewRow& r) { return r.verified; }));
}

}  // namespace autopilot::ocr
