---
tags: [autosuisei, history, learnings]
updated: 2026-06-23
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

## Session 2026-06-21 — DonateMore (1319 รูป) + sticker-digit model + v0.9.1
- **DonateMore** workload จริง 3 batch (`Donate Laptop 30` / `SN PC 1-990` / `desktop 1-40`): screen (Notepad `NO.7`
  + cmd `wmic SerialNumber`) + chassis (Dell `SERVICE TAG` + sticker "New PC Donate 677")
- **parser-first** (เลข+serial เป็น typed/printed → regex ดึงได้ ไม่ต้อง retrain): `extract_no_donate_explicit`
  (Notepad `NO.x` / "Donate N" sticker — เลขล้วนใกล้คำ "Donate", range-hint filter) + `extract_serial_donate`
  (Dell tag labeled / wmic `SerialNumber` anchor ข้าม `DESKTOP-`) → **No. ~99.6%**
- **sticker-digit model** (สำหรับ Photos-3-001 เลขเขียนมือ): YOLOv8n synthetic → `models/sticker_digit.onnx`
  (imgsz 512, onnxruntime) — ดู [[Sticker-Digit-Model]]
- **fusion** `fuse_sticker_no(model, crop)` (subsequence rule) — โมเดล high-precision/low-recall (หล่นหลัก 1/5/9)
  → fuse กับ crop_side → Photos-3-001 No. 60%→**~78%**
- build **v0.9.1** (donate + model; `make_bundle.ps1` copies `models/`)

## Session 2026-06-21/22 — ลบ org reader ทั้งหมด + v0.9.2
- user ลอง 0.9.1: Tesseract `tha` อ่านชื่อโรงเรียนลายมือ **มั่ว** → **"เอาตัวอ่านภาษาไทยออกไปเลย"**
- **ลบ org ทั้งหมด** (ไม่ใช่ซ่อน): Python `ocr_thai`/`extract_org_name`/`_clean_org_line` + C++ Org column/field
  (`AssetInfo.orgName`, `ReviewRow`, OcrTab/ReviewTab column, ReviewModel CSV) → **donate = No. + Serial เท่านั้น**
- ตัวหา *เลข* (`locate_sticker_bbox`/`_trailing_sticker_no`/`_THAI_LETTER_RE`) **เก็บไว้** (anchor เลข ไม่ output ไทย) → v0.9.2

## Session 2026-06-22 — Thai-path Unicode fix + v0.9.3
- user จัดรูปลงโฟลเดอร์ไทย (`ภาพ Donate/โรงเรียนวัดหนองคู/`) → กด **Rename / Save CSV ไม่ได้**
- **บั๊ก Unicode-path เดิม** (ไม่เกี่ยว org): `ReviewModel` load/save + `ReviewTab::onRename` ส่ง `std::string` UTF-8
  (`QString::toStdString()`) เข้า `ifstream`/`ofstream`/`fs::rename` ตรงๆ → **MSVC ตี narrow เป็น ANSI** → ไทยเพี้ยน
- แก้ด้วย **`std::filesystem::u8path()`** (idiom เดิมที่ AssetExtractor/OcrEngine/ImageMatcher ใช้ — 3 จุดนี้ลืม)
  + regression test `test_review_model.cpp` (Thai-path round-trip) → ctest **107/107** → v0.9.3

## Session 2026-06-23 — Review QOL + responsive UI + v0.9.4
- **Review QOL** (keyboard-driven verify loop): ↑/↓ เลื่อนรูป (`currentCellChanged` + `keyPressEvent`),
  **Ctrl+wheel zoom** ในที่เดิม (`QScrollArea` + `eventFilter`, dbl-click reset), **Enter = Apply + Next**
  (cursor กลับช่อง No. + select-all; Tab ข้าม read-only), **Apply = verify เสมอ** (loop เดินหน้า; checkbox live-sync)
- **Responsive multi-resolution UI**: `MainWindow` `setMinimumSize(760,520)` + clamp `availableGeometry()`
  (เดิม resize 1080×760 ตายตัว → ปุ่มล่างหลุดจอ 1366×768 / high-DPI) + ห่อทุก tab ด้วย `QScrollArea(widgetResizable)`
  กัน overlap; ReviewTab image-pane min 400×260→220×160
- **assemble v0.9.4**: commit งาน 0.9.3 ที่ค้างใน main → **merge worktree→main (auto-clean ไม่มี conflict)** →
  bump 0.9.4 → ctest **107/107** → `AutoSuisei-Setup-0.9.4.exe` (118 MB)

## Key learnings (รอบ donate → v0.9.4)
- **MSVC narrow `std::string` = ANSI codepage** ไม่ใช่ UTF-8 → path ไทย/ยูนิโค้ดพังเมื่อเข้า `std::filesystem`/`fstream`
  → **ต้อง `std::filesystem::u8path(s)` เสมอ** (STRICT rule ใหม่ → [[Conventions]])
- **DonateMore / Monitor = parser ไม่ใช่ model** — เลขเป็น typed/printed OCR อ่านได้ → regex แก้คุ้มกว่า retrain
- **fusion ต้อง fuse กับ crop_side** (`crop_no or sticker_no_from_boxes or extract_pc_no_donate`) ไม่ใช่ crop เปล่า
  — ไม่งั้นโมเดลเลขเดี่ยวบัง fallback 2 หลักที่ดีกว่า (offline 79.7% แต่ authoritative 60.9% เพราะบั๊กนี้)
- **embeddable Python** (`python._pth`) ไม่ auto-add script dir → script ต้อง `sys.path.insert(0, Path(__file__).parent)`
- **Qt responsive**: `resize()` ต้อง clamp `QScreen::availableGeometry()` (กันเปิดใหญ่เกินจอ) + `QScrollArea(widgetResizable)`
  ห่อ tab = รับประกันไม่ overlap; table-in-scrollarea ปกติ scroll ในตัว outer bar โผล่เฉพาะตอนหน้าต่างเล็กจริง
- **git worktree merge สะอาด** ได้เมื่อสองฝั่งแตะคนละ region (u8path = rename/save lines, QOL = nav/zoom/apply lines)

## ค้าง / ต่อไป
- **Monitor batch** (`Downloads/Rename/Monitor`, 119 รูป / 6 โรงเรียน): วัด+จูน No. parser — Monitor No. = เลขสติกเกอร์
  ไทยแบบ donate; current parser อ่านเลขหลักเดียว 1-9 ไม่ได้ (`PC_NO_STANDALONE_LINE_RE` จับ 2-3 หลัก) →
  reuse donate sticker-No. (ดู [[OCR-and-Parser]])
- **Photos-3-001 No. 78%→95%**: real-data fine-tune `sticker_digit.onnx` (auto-box) — ดู [[Sticker-Digit-Model]]
