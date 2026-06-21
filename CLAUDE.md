# Project: AutoSuisei (เดิม AutoPilot) — PC Inventory OCR

> **Note**: Product rebrand 2026-05-18 — exe + window title + installer ใช้ชื่อ AutoSuisei
> แต่โฟลเดอร์ source `Documents\AutoPilot`, C++ namespace `autopilot::`, library targets
> (`autopilot_core`, etc.), root CMake project name, และ env var `AUTOPILOT_SCRIPTS_DIR`
> ยังคงชื่อเดิมเพื่อลด churn

Windows desktop tool ที่ใช้ OCR ดึง PC No. + Dell serial จากภาพถ่ายมือถือ
ของ PC ที่ wipe-off แล้ว → review/correct ใน GUI → rename ไฟล์ภาพอัตโนมัติ

## Tech Stack
- **Language**: C++20 (parser + UI) + Python 3.14 (PaddleOCR sidecar)
- **Platform**: Windows 10/11 (x64)
- **Build**: CMake 3.25+ with **vcpkg** manifest mode
- **GUI**: Qt 6 Widgets (3 tabs: OCR / Watch / Review)
- **OCR**:
  - **PaddleOCR** via `rapidocr-onnxruntime` (Python sidecar) — 97.9% accuracy
  - Tesseract 5 ยังลิงก์ไว้สำหรับ direct OcrEngine class แต่ GUI ไม่ใช้
  - OpenCV 4 สำหรับ image I/O + template matching (Phase 5 ImageMatcher)
- **Storage**: SQLite 3 (เก็บ macro definitions ที่ Phase ก่อน — ปัจจุบัน GUI ไม่ใช้แต่ object ยังถูกสร้าง)
- **Logging**: spdlog
- **JSON**: nlohmann/json (parsing Python sidecar output)
- **Testing**: GoogleTest (106 tests in autopilot_tests)
- **IPC**: `QProcess` รัน Python scripts/{bulk_extract,ocr_worker}.py + parse JSON-per-line

## Build & Test Commands
```powershell
# First-time setup
vcpkg install --triplet x64-windows

# DEBUG build (พัฒนา)
cmake --preset windows-x64-debug
cmake --build --preset windows-x64-debug
.\build\windows-x64-debug\src\gui\AutoSuisei.exe

# RELEASE build (distribute)
cmake --preset windows-x64-release
cmake --build --preset windows-x64-release --target AutoSuisei
.\build\windows-x64-release\src\gui\AutoSuisei.exe

# Run all tests (debug only — tests link debug deps)
ctest --preset windows-x64-debug --output-on-failure

# Single test
ctest --preset windows-x64-debug -R "AssetExtractor.*" --output-on-failure

# Lint/format
clang-format -i src/**/*.{cpp,h}
clang-tidy -p build/windows-x64-debug src/**/*.cpp
```

ต้อง load `vcvars64.bat` ก่อนทุกครั้งบน Windows — ไม่งั้น cl.exe / ninja ไม่อยู่ใน PATH

## Project Rules

### Mandatory
1. **TDD** — เขียน GTest ก่อนเสมอแล้วค่อย implement
2. **Plan first** — ก่อนแก้โค้ดใหญ่ๆ ให้สร้าง/อัปเดต `docs/dev-plan.md`
3. **Conventional Commits**: `feat:`, `fix:`, `docs:`, `test:`, `refactor:`, `perf:`, `chore:`, `build:`
4. **ห้ามลบเทสต์เดิม** เว้นแต่ได้รับอนุญาตชัดเจน
5. **No raw `new`/`delete`** — ใช้ `std::unique_ptr` / `std::shared_ptr` / RAII
6. **No `using namespace std;`** ในไฟล์ header
7. **Const-correctness** — method ที่ไม่แก้ state ต้องเป็น `const`
8. **Header guards**: ใช้ `#pragma once`
9. **ห้าม hardcode secrets**

### Style
- Naming: `PascalCase` สำหรับ class/struct, `camelCase` สำหรับ method/variable, `UPPER_SNAKE` สำหรับ constant
- 4-space indentation, line width 100
- Public function ต้องมี Doxygen comment (`/** ... */`)

## Directory Structure
```
src/
  core/        Macro engine + Action (legacy from Phase 0-2, not used by current GUI)
  recorder/    Input hooks (legacy)
  player/      Input synthesis (legacy)
  ocr/         OcrEngine (Tesseract direct, unused by GUI) +
               AssetExtractor (regex parser, 100% used) +
               ReviewModel (data layer for review UI)
  web/         CDP client (legacy from Phase 3)
  scripting/   Lua sandbox (legacy from Phase 4)
  storage/     SQLite repos (used by Phase 2 macro tab — still ทำ DI ใน MainWindow)
  vision/      OpenCV ImageMatcher (legacy from Phase 5)
  gui/         Qt 6 frontend — OcrTab, WatchTab, ReviewTab, MainWindow
  cli/         Headless runner: `autopilot_cli ocr <image>`
scripts/       Python sidecars (bulk_extract.py, ocr_worker.py, helpers)
tests/         GTest unit + integration (106 tests)
docs/          dev-plan.md (phase-by-phase log)
```

## Architecture (current flow)
```
                    ┌─────────────────────────────┐
   ภาพในโฟลเดอร์ ─→  │ OCR tab: Bulk Extract       │ ─→ QProcess  ─→  scripts/bulk_extract.py
                    │ Watch tab: live folder watch │   (JSON/line)    (PaddleOCR + regex)
                    └─────────────────────────────┘                          │
                                  │                                          ▼
                                  │  AssetInfo[]                       train2_paddle.csv
                                  ▼
                    ┌─────────────────────────────┐
                    │ Review tab: edit/verify     │ ─→ ground_truth.csv
                    │   + Rename images            │ ─→ rename files on disk
                    └─────────────────────────────┘
```

## Module Boundaries
- `core` ห้าม depend บน `gui` หรือ `cli`
- `storage` คุยกับ `core` ผ่าน interface (DI ผ่าน MainWindow constructor)
- `gui` ไม่เรียก WinAPI ตรง — ผ่าน `recorder`/`player` (legacy) หรือ `QProcess` (current OCR path)

## Key Conventions
- **GUI ↔ Python**: subprocess + JSON line protocol. Python scripts emit JSON ต่อบรรทัด (`{"event": "row", ...}` หรือ `{"event": "result", ...}`); Qt parse ด้วย nlohmann/json
- **Parser single source**: Python `extract_pc_no/extract_serial` (หมวด `pc`) ใน `scripts/bulk_extract.py` mirror logic ของ C++ `AssetExtractor` 1:1 — แก้ที่หนึ่งต้องแก้อีกที่. หมวดที่เพิ่มภายหลัง (`monitor`/`accessory`/`donate`) เป็น **Python-only** (C++ `AssetExtractor` ยังมีแค่ pc)
- **Thai org name (donate)**: `RapidOCR()` default อ่านไทยไม่ได้ → หมวด `donate` รัน Tesseract `tha+eng --psm 6` รอบสองเฉพาะ field ชื่อโรงเรียน/สถานที่ (`ocr_thai`+`extract_org_name`). Tesseract เปิด unicode path ไม่ได้ → copy ไป ASCII temp ก่อน
- **Range hint**: filename "Laptop 301-400" → กรอง PC No. fallback ให้อยู่ใน [301, 400]
- **Scripts dir**: `AUTOPILOT_SCRIPTS_DIR` compile def ชี้ source/scripts/ (env var override); bundled exe ใช้ `<exeDir>/scripts/`

## Project Status (post-session 2026-05-18)
- **Product**: Rebrand AutoPilot → AutoSuisei (exe/window/installer); namespace + libs เก็บชื่อเดิม
- **GUI**: Claude Design dark theme + sidebar nav (240px) แทน QTabWidget — `theme.qss`, `SidebarNav`, ADLaM Display font bundled
- **Parser**: Category-aware (`pc`/`monitor`/`accessory`/`donate`) + rotation fallback (0°/90°/270°/180°)
- **Tested accuracy** (4 datasets, 2104 ภาพรวม):
  - **PC&Laptop** Train2 (632): No. **98.3%** / Serial **85.1%**
  - **Monitor** (754 Dell): No. **98.8%** / Serial **94.7%**
  - **Monitor 2** (300 มีภาพหมุน): No. **96.0%** / Serial **96.3%**
  - **Accessory** (418 Olivetti/Verifone/Feitian): No. **66.3%** / Serial **83.3%**
- **Build artifacts** (rebuilt 2026-06-21 w/ donate: Org column + DonateMore parser + sticker model):
  - exe: `build/windows-x64-release/src/gui/AutoSuisei.exe` (780 KB)
  - bundle: `C:\Users\hello\Backups\AutoSuisei\AutoSuisei-portable-20260621-213838` (440 MB) + .zip (173 MB)
  - installer: `C:\Users\hello\Backups\AutoSuisei\AutoSuisei-Setup-0.9.0.exe` (118 MB)
  - **`make_bundle.ps1` now copies `models/sticker_digit.onnx` → `<bundle>/models/`** (sticker_digit.py
    resolve `<scripts-parent>/models/`); ไม่งั้น donate sticker model หายจาก bundle
- **Docs**: `docs/AutoSuisei_User_Guide.pdf` (6 pages, Thai+English, fpdf2+Tahoma)
- **Remote**: ตั้ง `origin = https://github.com/helloshinee002-cell/AutoSuisei.git` แล้ว
  — local commit `e3b8c58` ค้าง push (รอ user auth ผ่าน PAT)
- ดู `docs/dev-plan.md` สำหรับ phase 0-11, `docs/CHAT_HISTORY_2026-05-18.md` สำหรับ session นี้,
  `docs/SKILL.md` สำหรับ pattern ที่นำกลับมาใช้ใหม่ได้

## Session 2026-05-18 summary (v0.9.0)

Phases:
- **Rebrand**: AutoPilot → AutoSuisei (16+ files touched, namespace เก็บ)
- **OCR restructure**: 3 category buttons (PC&Laptop/Monitor/Accessory) แทน "Bulk Extract Folder"
- **Parser**: เพิ่ม `extract_serial_monitor` (CN-…-A00) + `extract_serial_accessory` (flexible: labeled / numeric+dashes / 8-15 digit, skip CBA barcode) + `_merge_cn_fragments` line-merge helper
- **Rotation fallback**: `ocr_with_rotation()` ลอง 4 มุม, เลือกผลตาม `(has_sn, has_pc, mean_conf)` → +43pp Serial บน Monitor 2
- **Watch fix**: `ocr_worker.py` รับ `--category` flag (was crashing on `extract_serial(text)` post-refactor)
- **GUI redesign**: Claude Design template — dark theme `#0B0F0E`/emerald `#10B981`, sidebar 240px, KPI cards (Watch), progress bar (Review), bundled ADLaM Display font + Thai fallback
- **OCR controls**: ลบ "PC No." → "No." ทุก label, sequential `#` column, Stop button (kill QProcess)
- **Review**: เพิ่ม Clear button + Batch/Date read-only fields + Date/OK columns
- **User Guide PDF**: 6 หน้า, screenshots ของ 3 tabs, FAQ + tested accuracy

## Session 2026-06-19 — หมวด `donate` (No. + Service Tag + ชื่อสถานที่ไทย)

เพิ่มหมวดที่ 4 `donate` สำหรับภาพบริจาค 2 แบบ (ถ่ายจอ cmd/Notepad + ถ่ายหลังเครื่อง/sticker) —
3 ฟิลด์แยกกัน **SN + เลข + ไทย**, best-effort ("อ่านได้ก็อ่าน อ่านไม่ได้ก็ไม่เป็นไร"):
- **Service Tag (SN)**: `extract_serial_pc` (7-char Dell) — แม่นเกือบ 100%
- **เลข (No.)**: **position-aware** `sticker_no_from_boxes(raw)` ก่อน → fallback `extract_pc_no_donate(joined)`.
  donate ใช้ `ocr_donate()` ดึง OCR รอบเดียว**เก็บ box** (เลิกทิ้ง). สติกเกอร์อยู่**ฝั่งซ้ายเสมอ** (x<45%),
  ส่วน Express code/Service Tag/IO-noise อยู่กลาง-ขวา → กรองเฉพาะ box ซ้าย แล้วดึงเลข 1-2 หลักจาก
  token ซ้ายสุด (digit-boundary จับเลขฟิวส์อักษรได้ เช่น `Hainn2`→`2`). *Express code ไม่อ่าน* (ตัดด้วยเรขาคณิต)
  ⚠️ **คอขวดจริง = RapidOCR detector** ข้ามเลขเดี่ยวบนกระดาษขาว + **non-deterministic** (box สติกเกอร์
  detect ได้บ้างไม่ได้บ้างข้ามรอบรัน) → recall เพิ่มยากด้วย extraction; ที่หลุด No. ว่าง ให้คนกรอก Review.
  ลด det threshold เพิ่ม recall ได้แต่ได้เลขผิด (`#5#1`→หยิบผิด) → **ไม่ลด** (best-effort)
- **donate ข้าม rotation** — `ocr_donate` รอบเดียว (ภาพ donate ตั้งตรง); การหมุนไล่ mean-conf ทิ้งเลขสติกเกอร์
- **org_name (ชื่อโรงเรียน/สถานที่ ภาษาไทย)**: field ใหม่. `ocr_thai()` shell out `tesseract -l tha+eng --psm 6`
  (copy ไป ASCII temp ก่อน เพราะ unicode path) + `extract_org_name()` heuristic (marker `โรงเรียน/รร./เรียน` หรือบรรทัดไทยยาวสุด)
- **ไหลผ่าน**: `bulk_extract.py`/`ocr_worker.py` (+ คอลัมน์/event `org_name`) → `AssetInfo.orgName` →
  OcrTab (ปุ่ม Donate, default `Downloads/Train Donate`, คอลัมน์ "Org / สถานที่") →
  `ReviewRow.orgName` (loadCsv/saveCsv) → ReviewTab (คอลัมน์ + ฟอร์มแก้ไขได้) → `ground_truth.csv`
- **วัดบน 8 ภาพตัวอย่าง**: org 8/8 (จอเป๊ะ, sticker หยาบ—ให้คนแก้ใน Review), Serial 7/8, No. 4/4 (เฉพาะภาพจอที่มี No.)
- **วัดบน `Photos-3-001` (131 รูป sticker)**: **Serial 128/131 (97.7%)**, Org-ไทย 130/131 (99.2%, หยาบ→คนแก้),
  **No.-สติกเกอร์ 83/131 (63%)** — *position-aware (rev 4)* เพิ่ม precision: เลข 3-หลัก noise เหลือ 1 ตัว (จากเดิม 5),
  recall ติดเพดาน detector. ดันสูงกว่านี้ต้อง crop สติกเกอร์ก่อน OCR (งานใหญ่กว่า)
- **Self-check**: `scripts/test_donate_parser.py` (pure-text, ไม่ต้อง OCR). C++ tests ยังผ่าน 106/106
- ⚠️ rotation fallback (`cv2.imread`) ยัง fail กับ unicode filename (ปัญหาเดิม ทุกหมวด) — `ocr_thai` เลี่ยงได้เพราะ copy temp
- bundle/installer ยังไม่รวม Tesseract (dev-path ใช้ system tesseract 5.4 ที่มี tha) — ถ้าจะแจกต้อง bundle เพิ่ม

## Session 2026-06-21 — donate No. : YOLO digit model + fusion (~75%, ยังไม่ถึง 95%)
ดันเลขสติกเกอร์เป้า 95% แบบ offline → เทรน **YOLOv8n digit-detector** (synthetic) → `models/sticker_digit.onnx`
(imgsz 512, mAP50 0.978), inference `scripts/sticker_digit.py` (onnxruntime). วัดด้วย gt อ่านเองจากภาพ 65 รูป
(`build/donate_ground_truth.csv`):
- โมเดล **high-precision/low-recall** — detect ถูกแต่ **หล่นหลัก** (มองไม่เห็นเลข **1/5/9** บนสติกเกอร์ขาว =
  synthetic→real gap) → model-primary regress เลข 2 หลัก
- แก้ด้วย **`fuse_sticker_no(model_no, crop_no)`** (`bulk_extract.py` + `ocr_worker.py`): model ⊆ crop ที่ยาวกว่า
  = หล่นหลัก → ใช้ crop; ไม่งั้นใช้ model. **24/31** บนเคสที่สองวิธีต่างกัน (crop 12 / model 12)
- **บั๊กสำคัญ**: fuse ต้องใช้ **crop_side** (`crop_no or sticker_no_from_boxes or extract_pc_no_donate`)
  ไม่ใช่ crop_no เปล่า — แก้แล้ว fusion labeled-65 ขยับ **60%→78%** (win จริงของวันนี้)
- **ผล**: crop ~55% → model ~64% → **fusion ~78% labeled-65 / ~75% proj-131**
- **retrain hardened-synthetic = plateau** (model-only 31→35 แต่ fusion 51→50, crop เก็บไว้แล้ว) → revert
- **95% ยังไม่ถึง** → Phase B round 2: auto-box ภาพจริง + fine-tune (เน้น 1/5/9); gt 65 รูป = seed
- fix: embeddable python ไม่เติม script dir → `sys.path.insert(0, here)` ใน bulk_extract.py; train OOM จาก
  `cache='ram'` (kill ตอน epoch 21 แต่ best.pt เซฟแล้ว) → ใช้ `'disk'` ถ้า retrain
- ดู [[donate-sticker-number-95]] + `docs/dev-plan.md` Phase 12 rev 5

### DonateMore (1319 รูปจริง) — แก้ด้วย parser regex ไม่ต้องเทรน → No. 99.6%
`C:\Users\hello\Downloads\DonateMore\PC` = workload donate จริง (screen Notepad "NO.x"+wmic / chassis Dell tag+
สติกเกอร์ "New PC Donate 677"). **เลข+serial อยู่ใน RapidOCR text ครบ** (typed/printed) → เพิ่ม
`extract_no_donate_explicit` ("NO.x" + เลขหลังคำ "Donate", กรอง range hint) + `extract_serial_donate`
(wmic anchor "SerialNumber" ข้าม DESKTOP-) ใน `bulk_extract.py`+`ocr_worker.py`. **fast-path**: เจอ explicit →
ข้าม model+Thai-Tesseract (เร็ว 5x). ผล `build/donatemore.csv`: No. **99.6% / 100% (15/15 sample)**, Serial 84%.
ไม่ regress Photos-3-001 (explicit คืน '' บนสติกเกอร์ไทย). **บทเรียน: ดู raw OCR ก่อน — typed text แก้ด้วย regex
ไม่ใช่ retrain** (retrain จำเป็นเฉพาะสติกเกอร์เขียนมือไทย Photos-3-001). gt: `build/donatemore_gt_sample.csv`

Key learnings (เพิ่ม):
- **Rotation fallback** ใช้ cv2.rotate ก่อน OCR — ช่วย Serial 53% → 96% บน batch ที่มีภาพหมุน
- **Pause/Resume**: Win32 `NtSuspendProcess` (ntdll, undocumented) ไม่เสถียร — user ขอลบออก
- **PaddleOCR limitations**: ตัวเลขเดี่ยวขนาดใหญ่บนกระดาษขาว (เช่น "1", "2") OCR detector อาจ skip
- **QSS + setItemWidget**: `padding` บน `::item` ทำให้ widget ใน setItemWidget แคบจนตัดข้อความ — ลบออก
- **Native window frame**: ตัดสินใจไม่ทำ frameless titlebar (รักษา Windows Snap / accessibility)
