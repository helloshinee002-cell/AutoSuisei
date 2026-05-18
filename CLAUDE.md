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
- **Parser single source**: Python `extract_pc_no/extract_serial` ใน `scripts/bulk_extract.py` mirror logic ของ C++ `AssetExtractor` 1:1 — แก้ที่หนึ่งต้องแก้อีกที่
- **Range hint**: filename "Laptop 301-400" → กรอง PC No. fallback ให้อยู่ใน [301, 400]
- **Scripts dir**: `AUTOPILOT_SCRIPTS_DIR` compile def ชี้ source/scripts/ (env var override); bundled exe ใช้ `<exeDir>/scripts/`

## Project Status (post-session 2026-05-18)
- **Product**: Rebrand AutoPilot → AutoSuisei (exe/window/installer); namespace + libs เก็บชื่อเดิม
- **GUI**: Claude Design dark theme + sidebar nav (240px) แทน QTabWidget — `theme.qss`, `SidebarNav`, ADLaM Display font bundled
- **Parser**: Category-aware (`pc`/`monitor`/`accessory`) + rotation fallback (0°/90°/270°/180°)
- **Tested accuracy** (4 datasets, 2104 ภาพรวม):
  - **PC&Laptop** Train2 (632): No. **98.3%** / Serial **85.1%**
  - **Monitor** (754 Dell): No. **98.8%** / Serial **94.7%**
  - **Monitor 2** (300 มีภาพหมุน): No. **96.0%** / Serial **96.3%**
  - **Accessory** (418 Olivetti/Verifone/Feitian): No. **66.3%** / Serial **83.3%**
- **Build artifacts**:
  - exe: `build/windows-x64-release/src/gui/AutoSuisei.exe` (776 KB)
  - bundle: `C:\Users\hello\Backups\AutoSuisei\AutoSuisei-portable-20260518-201724` (428 MB)
  - installer: `C:\Users\hello\Backups\AutoSuisei\AutoSuisei-Setup-0.9.0.exe` (108 MB)
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

Key learnings (เพิ่ม):
- **Rotation fallback** ใช้ cv2.rotate ก่อน OCR — ช่วย Serial 53% → 96% บน batch ที่มีภาพหมุน
- **Pause/Resume**: Win32 `NtSuspendProcess` (ntdll, undocumented) ไม่เสถียร — user ขอลบออก
- **PaddleOCR limitations**: ตัวเลขเดี่ยวขนาดใหญ่บนกระดาษขาว (เช่น "1", "2") OCR detector อาจ skip
- **QSS + setItemWidget**: `padding` บน `::item` ทำให้ widget ใน setItemWidget แคบจนตัดข้อความ — ลบออก
- **Native window frame**: ตัดสินใจไม่ทำ frameless titlebar (รักษา Windows Snap / accessibility)
