---
tags: [autosuisei, history, learnings]
updated: 2026-06-17
---

# Dev History & Key Learnings

[[Home]] · parser [[OCR-and-Parser]] · ผล [[Accuracy-Results]]

แหล่ง: `docs/dev-plan.md` (phase log 391 บรรทัด), `docs/CHAT_HISTORY_2026-05-18.md`, `docs/SKILL.md`

## Phase log (ย่อ)
- **Phase 0** Scaffold — โครงโฟลเดอร์, CMake+vcpkg, module headers, GTest seed
- **Phase 1** OCR MVP — `OcrEngine` (Tesseract+OpenCV), unicode path (`imdecode`), `OcrFormatter::toJson`
- **Phase 2** Macro Recorder — hooks/SendInput, `MacroSerializer`, SQLite repo *(legacy)*
- **Phase 3** Web — Chrome DevTools Protocol client *(legacy)*
- **Phase 4** Scripting — Lua sandbox *(legacy)*
- **Phase 5** Vision — OpenCV `ImageMatcher` template matching *(legacy)*
- **Phase 6-9** OCR pivot — bulk extract, watch-folder, PaddleOCR (`rapidocr`), category parser,
  range-hint, ground-truth tooling
- **Phase 10-11** Distribution — Inno Setup installer, embedded Python, CRT bundling
- **2026-05-18 / v0.9.0** — rebrand + GUI redesign (ดูด้านล่าง)

## Session 2026-05-18 (v0.9.0)
- **Rebrand** AutoPilot → AutoSuisei (16+ ไฟล์; namespace/libs/env เก็บชื่อเดิม)
- **OCR restructure** — 3 ปุ่ม category (PC&Laptop/Monitor/Accessory) แทน "Bulk Extract Folder"
- **Parser** — เพิ่ม `extract_serial_monitor` (CN-…-A00) + `extract_serial_accessory` (flexible)
  + `_merge_cn_fragments` line-merge
- **Rotation fallback** — `ocr_with_rotation()` ลอง 4 มุม → +43pp Serial บน Monitor 2
- **Watch fix** — `ocr_worker.py` รับ `--category` flag (เคย crash หลัง refactor)
- **GUI redesign** — Claude Design dark theme `#0B0F0E` / emerald `#10B981`, sidebar 240px,
  KPI cards (Watch), progress bar (Review), ฟอนต์ ADLaM Display + Thai fallback
- **OCR controls** — "PC No." → "No." ทุก label, sequential `#` column, ปุ่ม Stop (kill QProcess)
- **Review** — เพิ่ม Clear button + Batch/Date read-only + Date/OK columns
- **User Guide PDF** 6 หน้า (screenshots 3 tabs + FAQ + accuracy)

## Key learnings
- **Rotation fallback** ใช้ `cv2.rotate` ก่อน OCR — Serial **53% → 96%** บน batch ที่มีภาพหมุน
- **Pause/Resume** — Win32 `NtSuspendProcess` (ntdll, undocumented) ไม่เสถียร → **user ขอลบออก**
- **PaddleOCR limit** — เลขเดี่ยวขนาดใหญ่บนกระดาษขาว ("1","2") detector อาจ skip ทั้งกล่อง
- **QSS + `setItemWidget`** — `padding` บน `::item` ทำให้ widget แคบจนตัดข้อความ → ลบออก
- **Native window frame** — ตัดสินใจ **ไม่ทำ** frameless titlebar (รักษา Windows Snap / accessibility)

## Session 2026-06-19 — หมวด `donate`
- เพิ่มหมวดที่ 4 `donate`: No. + Service Tag (reuse `pc` parser) + **org_name ชื่อโรงเรียน/สถานที่ (ไทย)**
- ไทยอ่านด้วย **Tesseract `tha`** รอบสอง (`ocr_thai`) เพราะ RapidOCR default อ่านไทยไม่ได้ — copy ไป ASCII temp กัน unicode-path trap
- ฟิลด์ `org_name` ใหม่ไหลตลอดสาย: Python CSV/event → `AssetInfo`/`ReviewRow` → OcrTab/ReviewTab → `ground_truth.csv`
- วัด 8 ภาพ: org 8/8, Serial 7/8, No. 4/4 (ภาพจอ). C++ 106/106 ผ่าน. ดู [[OCR-and-Parser]]
- **learning**: Tesseract เปิด unicode/space path ไม่ได้ (เหมือน `cv::imread`) → copy ASCII temp ก่อน
- **rev 3** (ชุด `Photos-3-001` 131 รูป): ฟิลด์ "เลข" จับ**เลขสติกเกอร์** ด้วย (`extract_pc_no_donate` +
  `_STICKER_NO_RE`), Express code ไม่อ่าน. **donate ข้าม rotation** (ภาพตั้งตรง — หมุนไล่ conf ทิ้งเลขสติกเกอร์)
- **learning rev 3**: PaddleOCR ข้ามเลขเดี่ยวใหญ่บนกระดาษขาวจริง → No. เลขหลักเดียวอาจหลุด (best-effort, คนกรอก Review)

## ค้าง / ต่อไป
ดู `docs/tomorrow.md` (91 บรรทัด) สำหรับ task ที่ค้าง — เน้น Monitor/Accessory serial parsing
