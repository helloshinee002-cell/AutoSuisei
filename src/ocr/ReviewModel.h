#pragma once

#include <optional>
#include <string>
#include <vector>

namespace autopilot::ocr {

/**
 * หนึ่งแถวใน review — สถานะของภาพหนึ่งใบ
 *
 * โหลดจาก bulk_extract.csv (filename, pc_no, serial_no, ...) ผู้ใช้แก้ค่า
 * pcNo / serialNo / notes แล้ว set verified=true เมื่อยืนยัน
 * ค่า original* เก็บค่า OCR เดิมไว้สำหรับ audit
 */
struct ReviewRow {
    std::string filename;
    std::string pcNo;
    std::string serialNo;
    std::string serialSource;     ///< "barcode" | "ocr" — ที่มาของ serialNo (remark)
    std::string originalPcNo;     ///< ค่าเดิมจาก OCR — ห้ามแก้
    std::string originalSerialNo; ///< ค่าเดิมจาก OCR — ห้ามแก้
    std::string notes;
    bool verified{false};

    bool operator==(const ReviewRow&) const = default;
};

/**
 * ReviewModel = pure data layer — โหลด/บันทึก/แก้ไข `ReviewRow` ทั้งกอง
 * ไม่ขึ้นกับ Qt เพื่อทดสอบได้ด้วย GTest
 */
class ReviewModel {
public:
    /** ลบทุก row + reset state */
    void clear();

    /** โหลด CSV ที่ผลิตจาก scripts/bulk_extract.py (มีคอลัมน์ filename, pc_no, serial_no อย่างน้อย)
     *  ถ้ามีคอลัมน์ verified/notes ก็จะอ่านด้วย (สำหรับ resume)
     *  คืน true ถ้าเปิดไฟล์ + parse header สำเร็จ */
    [[nodiscard]] bool loadCsv(const std::string& path);

    /** บันทึก rows ทั้งหมดเป็น CSV ใหม่ — header:
     *  filename,pc_no,serial_no,original_pc_no,original_serial_no,verified,notes
     *  คืน true ถ้าเขียนสำเร็จ */
    [[nodiscard]] bool saveCsv(const std::string& path) const;

    /** จำนวน row ทั้งหมด */
    [[nodiscard]] std::size_t size() const noexcept { return rows_.size(); }

    /** อ่าน row ที่ index — out-of-range คืน nullopt */
    [[nodiscard]] std::optional<ReviewRow> at(std::size_t idx) const;

    /** เขียนทับ row ที่ index — out-of-range ไม่ทำอะไร, คืน false */
    bool setRow(std::size_t idx, const ReviewRow& row);

    /** หา index ของ row ถัดไปที่ verified=false (>=from) — ไม่เจอคืน nullopt */
    [[nodiscard]] std::optional<std::size_t> nextUnverified(std::size_t from = 0) const;

    /** นับจำนวน verified */
    [[nodiscard]] std::size_t verifiedCount() const noexcept;

    // ---------------- pure helpers สำหรับ unit test CSV escaping ----------------

    /** Escape CSV field ถ้ามี , " หรือขึ้นบรรทัด */
    [[nodiscard]] static std::string escapeCsv(const std::string& field);

    /** Parse 1 บรรทัด CSV → vector<string> (รองรับ quoted field) */
    [[nodiscard]] static std::vector<std::string> parseCsvLine(const std::string& line);

private:
    std::vector<ReviewRow> rows_;
};

}  // namespace autopilot::ocr
