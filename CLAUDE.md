# Project: AutoPilot — PC Inventory OCR

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
.\build\windows-x64-debug\src\gui\AutoPilot.exe

# RELEASE build (distribute)
cmake --preset windows-x64-release
cmake --build --preset windows-x64-release --target AutoPilot
.\build\windows-x64-release\src\gui\AutoPilot.exe

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

## Project Status
- Phase 9.5 stable: **98.4%** PC No. accuracy บน Train2 (632 ภาพ vs user ground truth)
- Phase 11: Watch folder live extract เสร็จ
- Release build: `build/windows-x64-release/src/gui/AutoPilot.exe` (459 KB)
- **Installer (เสร็จ)**: `C:\Users\hello\Backups\AutoPilot\AutoPilot-Setup-0.9.0.exe` (108 MB, self-contained — มี Python + rapidocr + MSVC CRT bundled)
- Git tag: `v0.9.0-stable`
- ดู `docs/dev-plan.md` สำหรับประวัติ phase-by-phase
- **ดู `docs/tomorrow.md`** สำหรับงานค้าง session ถัดไป (rename → AutoSuisei + Monitor/Accessory tabs)

## Session 2026-05-17 summary

Started PC No. accuracy: ~50% / Ended: **98.4%** บน 632 ภาพ Train2

Phases done this session:
- 9.1 false-positive blocklist (commit `cb6e852`)
- 9.2 lone-digit fallback (commit `42d811c`)
- 9.3 Manual Review UI (commit `c48fec0`) — user verified 622/632 ground truth
- 9.4 serial dedup (commit `e631f18`)
- 9.5 range-guided extraction (commits `4ce1f25`, `5df5cca`) → 98.4%
- 9.6/9.7/9.8 GUI overhaul (commit `e5c1f73`) — QProcess pipeline, ลบ 3 tabs, Rename feature
- 11 Watch folder live extract (commit `70184a5`)
- Release build + Inno Setup installer + embedded Python + Suisei icon + MSVC CRT bundling
  (commits `252b2e5`, `405808d`, `28cded3`, `687e048`, `03c12b9`, `2cac371`)

Key learnings:
- Tesseract 7.8% PC No. hit → PaddleOCR (rapidocr-onnxruntime) 97.9% — use Python sidecar via QProcess
- Range hint from filename ("Laptop 301-400") + iterate primary matches → +4.6 pp
- DLL bundling for installer: vcpkg DLLs ✓ + Qt plugins ✓ + MSVC CRT next to exe AND
  ในโฟลเดอร์ python/ ด้วย (Python loader ค้นโฟลเดอร์ของ python.exe ไม่ใช่ของ main exe)
