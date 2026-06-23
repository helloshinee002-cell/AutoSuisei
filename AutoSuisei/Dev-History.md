---
tags: [autosuisei, history, learnings]
updated: 2026-06-24
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

## Session 2026-06-24 — Monitor No. survey (เพดาน free/local) + unicode fix
**Batch**: `Downloads/Rename/Monitor` 119 รูป / 6 รร. (ชื่อไฟล์ = No. ที่ rename แล้ว = ground truth ฟรี).
สติกเกอร์กระดาษขาว "โรงเรียน… `<N>`" (**พิมพ์**) + Dell S/N label. เป้า user: อ่านเลขกระดาษขาว + S/N.

- **พบ+แก้บั๊กจริง** — `ocr_with_rotation` ส่ง `str(path)` เข้า RapidOCR + ใช้ `cv2.imread` → **เปิด path ไทยไม่ได้** →
  Monitor/PC/Accessory บนโฟลเดอร์ไทย **ได้ผลว่าง**. แก้ใช้ `_imread_unicode` → commit **`16a9f6f`** (บั๊กคลาสเดียวกับ
  u8path C++ v0.9.3 แต่ฝั่ง Python; donate path ใช้ `_imread_unicode` อยู่แล้ว — `ocr_with_rotation` ลืม)
- **Serial = 97.5%** present (ใช้ได้จริง). **No. = ลอง 7 วิธี เพดาน ~42%**:
  RapidOCR full **1.7%** · crop+RapidOCR **24%** · Tesseract crop **28.6%** · model full **29%** · deskew+digit **36%**
  · **fusion (crop+RapidOCR + model) 42%** ← best free/local
- **สาเหตุที่ตัน** (ข้อจำกัด stack ไม่ใช่ bug): RapidOCR **อ่านไทยไม่ได้** + **ข้ามเลขเดี่ยวบนกระดาษขาว** + **RoHS "⑩"=10 หลอก**
  + **crop หากระดาษขาวพลาด ~40%** (หมุน/แสง/สะท้อน/ตำแหน่ง). แม้ crop เป๊ะ digit-whitelist ก็เพี้ยน (บังคับอักษรไทยเป็นเลข)
  → ต้อง `tha+eng`
- **ทำไมไม่ทะลุ >90%** = trade-off triangle (ดู Key learnings). big VLM (Qwen2.5-VL/Ollama, เครื่องนี้มี+RAM 32GB)
  น่าจะ >90% **แต่ไม่ distributable** (6-8GB, RAM 8GB+, CPU ช้า) → ตกข้อ user "ต้องลงเครื่องอื่นสเปคต่ำได้"; cloud ~99% แต่เสียเงิน
- **ตัดสินใจ (user)**: ไม่ฝืน — Serial 97.5% พอ, No. เติม Review (sequential). **เลื่อนไป recurring-loop**: รอ batch ใหม่
  → retrain โมเดลจิ๋ว 12MB ([[Sticker-Digit-Model]]). Monitor **พิมพ์** (≠ donate ลายมือ) → synth→real gap เล็ก → synthetic มีลุ้น
- **Pipeline พร้อมใน `build/`** (gitignored): `measure_monitor.py`, `autolabel_monitor.py` (Tesseract crop→YOLO box, match 28/119),
  `hi_read.py`/`probe_crop.py` (วิธีที่ลอง); + `synth_stickers.py`/`train_digit.py` พร้อม retarget Monitor

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
- ⭐ **OCR trade-off triangle** (accuracy ↔ distributable ↔ free/offline — เลือกได้ ~2): big VLM แม่นแต่ไม่ distributable
  (โมเดลหลาย GB + RAM สูง); cloud แม่นแต่เสียเงิน; free+distributable+offline (PaddleOCR/Tesseract/tiny-model) **เพดาน ~42%**
  บนเลขกระดาษขาวจริง. → high accuracy แบบ **distributable** = **เทรนโมเดลจิ๋วเอง (synthetic)** ไม่ใช่สลับเป็น engine ใหญ่
- **เลขกระดาษขาว = ปัญหา data ไม่ใช่ code** — คนอ่านออก ~100% แต่ pipeline OCR เก่าตัน เพราะ detector ข้าม + crop เปราะ.
  วัดก่อนเสมอ (อย่า extrapolate: donate Tesseract 78% **ไม่** transfer มา Monitor = 28.6%)

## ค้าง / ต่อไป (recurring-loop)
- **Monitor No.** (survey 2026-06-24 ↑): เพดาน free/local **42%** → **รอ batch รูปใหม่ → retrain โมเดลจิ๋ว** เข้าหา >90%.
  ทางที่ฟิต distributable: enhance `synth_stickers.py` เป็น Monitor-style (printed + bg Monitor จริง) + 28 real → train
  → `sticker_digit.onnx` 12MB. (interim ถ้าอยาก: wire fusion 42% — bundle-compatible; >90% **now** ต้อง bundled VLM
  เล็ก Florence-2 ~1GB ที่หนักกว่า หรือ cloud ที่เสียเงิน)
- **Photos-3-001 No. 78%→95%**: real-data fine-tune `sticker_digit.onnx` (auto-box) — ดู [[Sticker-Digit-Model]]
