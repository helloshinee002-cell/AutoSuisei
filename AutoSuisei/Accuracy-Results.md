---
tags: [autosuisei, accuracy, results]
updated: 2026-06-26
---

# Accuracy Results

[[Home]] · ตรรกะที่ทำให้ได้ตัวเลขนี้ [[OCR-and-Parser]]

เดิม 4 ชุด รวม **2104 ภาพ** (v0.9.0) + **donate** (v0.9.1) + **Monitor batch** (2026-06-24, วัดแล้ว)

| Dataset                                    |   จำนวน |            No.             |     Serial      |
| ------------------------------------------ | ------: | :------------------------: | :-------------: |
| **PC&Laptop** Train2                       |     632 |         **98.3%**          |      85.1%      |
| **Monitor** (Dell)                         |     754 |         **98.8%**          |    **94.7%**    |
| **Monitor 2** (มีภาพหมุน)                  |     300 |           96.0%            |    **96.3%**    |
| **Accessory** (Olivetti/Verifone/Feitian)  |     418 |           66.3%            |      83.3%      |
| **donate** Photos-3-001 (เลขเขียนมือ)      | 65 (gt) |    ~78% (model+fusion)     |        —        |
| **DonateMore** (typed/printed)             |    1319 |         **~99.6%**         | Dell tag / wmic |
| **Monitor batch** (6 รร., เลขสติกเกอร์ไทย) |     119 | **42%** (เพดาน free/local) |    **97.5%**    |

## อ่านผล
- **No. แม่นสูงมากทุกชุด** ยกเว้น Accessory (66%) — เพราะป้ายครุภัณฑ์ของ accessory
  ไม่มาตรฐาน + บางทีเป็นเลขเดี่ยวใหญ่ที่ detector skip ([[OCR-and-Parser]] §ข้อจำกัด)
- **Serial บน Monitor 2 พุ่งเป็น 96.3%** เพราะ **rotation fallback** (ชุดนี้มีภาพหมุนเยอะ)
  — ก่อนมี fallback อยู่ราว 53% ([[Dev-History]])
- PC&Laptop Serial (85%) ต่ำกว่า Monitor เพราะ Service Tag 7 ตัวสั้น OCR พลาดง่าย + คำลวง
  (กัน false positive ด้วย `SERIAL_BLOCKLIST`)
- **donate** เลขเขียนมือบนสติกเกอร์ขาว — parser crop ~55% → +sticker model+fusion ~78% ([[Sticker-Digit-Model]]);
  **DonateMore** เลข typed/printed (Notepad `NO.x` / "Donate N") → parser ~99.6% (model ไม่จำเป็น)
- **org reader (ชื่อโรงเรียนไทย) ลบทิ้งแล้ว** (v0.9.2) — donate เหลือ No. + Serial ([[Dev-History]])
- **Monitor batch** (`Downloads/Rename/Monitor`, 119 รูป/6 รร., ชื่อไฟล์=No. ที่ rename แล้ว = gt): ดูบทสรุปเต็มที่
  [[Dev-History]] §"Monitor No. survey". สั้น ๆ: **Serial 97.5%** ใช้ได้จริง, **No. เพดาน ~42%** (free/local) —
  RapidOCR อ่านไทยไม่ได้ + ข้ามเลขเดี่ยวบนกระดาษขาว + RoHS "⑩"=10 หลอก + crop หากระดาษขาวพลาด ~40%.
  พบ+แก้บั๊ก unicode-imread (`ocr_with_rotation` เปิด path ไทยไม่ได้ → ผลว่าง). **เลื่อนไป recurring-loop**: รอ batch เพิ่ม
  → retrain โมเดลจิ๋ว ([[Sticker-Digit-Model]]) ดีขึ้นเรื่อย ๆ

## Barcode-first Serial (v0.9.6/0.9.7) — แก้ OCR misread
อ่าน Dell barcode ก่อน OCR (กลไก: [[OCR-and-Parser]] §Barcode-first). วัดจริง:
- **PC Train2** (laptop, **Data Matrix**/pylibdmtx): serial coverage OCR-only **74.7% → 84.7%** (+10pp, 150-img sample),
  0 wrong reads — barcode ที่ต่างจาก OCR = barcode ถูก (ป้าย `6X0F453` แต่ OCR/รีวิวเดิม `6XOF463`)
- **DonateMore/PC** (desktop+laptop, **Code128**/ZBar): decode rate ~40% (ที่เหลือ = screenshot `wmic` ไม่มีโค้ด → OCR),
  agreement กับ OCR 90%; **disagreement 8/8 barcode ถูก OCR ผิด** — `O↔0` (`4N0TZL2`/`F0RPWZ2`) + misread (`JMWLWZ2`≠`JMIVLWZ`, `BL0CXZ2`≠`P97G001`)
- ค่าหลัก = **ความถูกต้อง** (donate OCR coverage 99% อยู่แล้ว) ไม่ใช่ coverage. บทสรุปเต็ม: [[logs/2026-06-26-barcode-first-serial]]

## วิธีวัด
- รัน `scripts/bulk_extract.py <folder> out.csv --category=<...>` → ได้ `*_paddle.csv`
- เทียบกับ `ground_truth.csv` ด้วย `scripts/compare_to_ground_truth.py`
- ตัวช่วยอื่น: `validate_pc_range.py`, `dedupe_serials.py`, `group_by_pc.py`
