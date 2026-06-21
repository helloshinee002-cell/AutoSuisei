---
tags: [autosuisei, accuracy, results]
updated: 2026-06-17
---

# Accuracy Results

[[Home]] · ตรรกะที่ทำให้ได้ตัวเลขนี้ [[OCR-and-Parser]]

ทดสอบ 4 ชุดข้อมูล รวม **2104 ภาพ** (post-session 2026-05-18, v0.9.0)

| Dataset | จำนวน | No. (PC No.) | Serial |
|---------|------:|:---:|:---:|
| **PC&Laptop** Train2 | 632 | **98.3%** | 85.1% |
| **Monitor** (Dell) | 754 | **98.8%** | **94.7%** |
| **Monitor 2** (มีภาพหมุน) | 300 | 96.0% | **96.3%** |
| **Accessory** (Olivetti/Verifone/Feitian) | 418 | 66.3% | 83.3% |

## อ่านผล
- **No. แม่นสูงมากทุกชุด** ยกเว้น Accessory (66%) — เพราะป้ายครุภัณฑ์ของ accessory
  ไม่มาตรฐาน + บางทีเป็นเลขเดี่ยวใหญ่ที่ detector skip ([[OCR-and-Parser]] §ข้อจำกัด)
- **Serial บน Monitor 2 พุ่งเป็น 96.3%** เพราะ **rotation fallback** (ชุดนี้มีภาพหมุนเยอะ)
  — ก่อนมี fallback อยู่ราว 53% ([[Dev-History]])
- PC&Laptop Serial (85%) ต่ำกว่า Monitor เพราะ Service Tag 7 ตัวสั้น OCR พลาดง่าย + คำลวง
  (กัน false positive ด้วย `SERIAL_BLOCKLIST`)

## วิธีวัด
- รัน `scripts/bulk_extract.py <folder> out.csv --category=<...>` → ได้ `*_paddle.csv`
- เทียบกับ `ground_truth.csv` ด้วย `scripts/compare_to_ground_truth.py`
- ตัวช่วยอื่น: `validate_pc_range.py`, `dedupe_serials.py`, `group_by_pc.py`
