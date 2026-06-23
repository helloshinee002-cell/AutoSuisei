---
tags: [autosuisei, architecture]
updated: 2026-06-23
---

# Architecture — Pipeline & Data Flow

[[Home]] · ดูโมดูลที่ [[Modules]] · ตรรกะ parser ที่ [[OCR-and-Parser]]

## Flow ปัจจุบัน
```
                    ┌─────────────────────────────┐
   ภาพในโฟลเดอร์ ─→  │ OCR tab: Bulk Extract        │ ─→ QProcess ─→ scripts/bulk_extract.py
                    │ Watch tab: live folder watch │   (JSON/line)   (PaddleOCR + regex)
                    └─────────────────────────────┘                        │
                                  │                                        ▼
                                  │  AssetInfo[]                     train2_paddle.csv
                                  ▼
                    ┌─────────────────────────────┐
                    │ Review tab: edit/verify      │ ─→ ground_truth.csv
                    │   + Rename images            │ ─→ rename files on disk
                    └─────────────────────────────┘
```

## GUI ↔ Python IPC
- Qt รัน Python sidecar ด้วย **`QProcess`** (`scripts/bulk_extract.py` สำหรับ bulk,
  `scripts/ocr_worker.py` สำหรับ Watch tab)
- โปรโตคอล = **JSON ต่อบรรทัด (line protocol)**: Python emit
  `{"event": "start"|"ready"|"row"|"done", ...}` ทีละบรรทัด, flush ทุกครั้ง
- Qt parse ด้วย **nlohmann/json** แล้วอัปเดต UI (ตาราง / progress / KPI)
- **Stop** = kill QProcess ([[Dev-History]])

## ตัวอย่าง event `row` (จาก bulk_extract.py)
```json
{"event":"row","i":12,"total":632,"filename":"IMG_0042.jpg",
 "pc_no":"317","serial_no":"7C2R4X1","mean_confidence":0.91,
 "with_pc":11,"with_sn":9}
```

## ไฟล์ output
- `train2_paddle.csv` — ผล OCR ดิบจาก bulk extract (คอลัมน์: `photo_index, filename,
  pc_no, serial_no, batch_id, photo_date, pc_range, mean_confidence, line_count, warnings`)
- `ground_truth.csv` — ผลที่คนตรวจ/แก้แล้วจาก Review tab (ใช้เทียบความแม่น)
- ไฟล์ภาพถูก rename ตาม PC No. + serial ที่ยืนยัน

## donate — sub-path เพิ่ม (v0.9.1)
- เลข typed/printed → parser (`extract_no_donate_explicit`); เลขเขียนมือ → **YOLOv8 `models/sticker_digit.onnx`
  รันด้วย onnxruntime** (มีใน embedded python อยู่แล้ว) + `fuse_sticker_no` — ดู [[Sticker-Digit-Model]]
- Serial: Dell tag / wmic. **rotation ข้าม** สำหรับ donate (ภาพตั้งตรง)

## GUI (v0.9.4)
- **responsive**: `MainWindow` clamp ขนาดตาม `QScreen::availableGeometry()` + ห่อทุก tab ด้วย `QScrollArea`
  → ไม่ overlap ทุก resolution. **Review QOL**: keyboard verify loop (↑/↓ / Ctrl+wheel zoom / Enter=Apply)

## หลักการ
- **Parser อยู่ฝั่ง Python** (เร็วในการ iterate, อยู่กับ PaddleOCR) — `extract_pc_no`/`extract_serial_pc` **mirror 1:1
  กับ C++ `AssetExtractor`**; donate/monitor/sticker-model = Python เท่านั้น ([[OCR-and-Parser]] / [[Conventions]])
- C++ ฝั่ง GUI ทำหน้าที่ orchestrate + แสดงผล + จัดการไฟล์ ไม่ทำ OCR เอง
