---
tags: [autosuisei, build, release]
updated: 2026-06-26
---

# Build & Distribution

[[Home]] · โมดูล [[Modules]] · กฎ/style [[Conventions]]

> ⚠️ ต้อง `vcvars64.bat` ก่อนทุกครั้งบน Windows — ไม่งั้น `cl.exe` / `ninja` ไม่อยู่ใน PATH

## First-time setup
```powershell
vcpkg install --triplet x64-windows   # ตั้ง env VCPKG_ROOT ไว้ก่อน
```

## Build / Run
```powershell
# DEBUG (พัฒนา)
cmake --preset windows-x64-debug
cmake --build --preset windows-x64-debug
.\build\windows-x64-debug\src\gui\AutoSuisei.exe

# RELEASE (distribute)
cmake --preset windows-x64-release
cmake --build --preset windows-x64-release --target AutoSuisei
.\build\windows-x64-release\src\gui\AutoSuisei.exe

# CLI OCR
.\build\windows-x64-debug\src\cli\autopilot_cli.exe ocr "C:\path\image.png"
```

## Test (debug only — tests link debug deps)
```powershell
ctest --preset windows-x64-debug --output-on-failure              # ทั้งหมด (109 tests)
ctest --preset windows-x64-debug -R "AssetExtractor.*" --output-on-failure   # ชุดเดียว
```

## Lint / Format
```powershell
clang-format -i src/**/*.{cpp,h}
clang-tidy -p build/windows-x64-debug src/**/*.cpp
```

## Installer & Bundle (end-user distribution)
- **Installer**: Inno Setup wizard — bundle embedded Python + `rapidocr` + suisei icon,
  ฝัง MSVC v143 CRT (แก้ `0xc0000142`), mirror CRT เข้า `python/` ให้ onnxruntime โหลด DLL ได้
- สคริปต์: `scripts/make_installer.ps1` (re-run bundle + iscc), `scripts/make_bundle.ps1`, `scripts/setup_embedded_python.ps1`
- **`make_bundle.ps1` copies**: `scripts/*.py` + **`models/sticker_digit.onnx`** + embedded python + Qt/vcpkg/MSVC DLLs
  (ถ้าลืม copy `models/` → donate sticker model หาย, fusion ตกเหลือ crop-only)
- ⚠️ **Qt plugins ต้อง deploy ครบ — list อยู่ 2 ที่ต้องตรงกัน**: `src/gui/CMakeLists.txt` (POST_BUILD foreach) + `make_bundle.ps1` (foreach).
  ปัจจุบัน = `platforms styles imageformats` **+ `tls`**. **`tls` จำเป็นสำหรับ HTTPS** (Updater → GitHub) — ขาด → "TLS initialization failed" (bug v1.0.0). เพิ่ม Qt feature ที่ใช้ plugin ใหม่ → ต้องเพิ่มทั้ง 2 ที่
- **Barcode libs** (v0.9.6/0.9.7) อยู่ใน embedded python → bundle อัตโนมัติ: `pylibdmtx` (`libdmtx-64.dll`, Data Matrix) +
  `pyzbar` (`libzbar-64.dll` + `libiconv.dll`, Code128/ZBar). offline ทุกเครื่อง Windows — ติดตั้งด้วย `pip install` ลง `build/embedded-python`
- embedded Python = **3.11.9** (`build/embedded-python/`, `python-embed-3.11.9.zip`)
  — แยกจาก Python ที่ใช้ตอน dev; `python._pth` ไม่ auto-add script dir → script ใช้ `sys.path.insert` ([[Conventions]])

## Version source (v1.0.0+)
- **single source = root `CMakeLists.txt` `project(... VERSION x.y.z)`** → inject C++ ผ่าน compile-def
  `APP_VERSION` (`src/gui/CMakeLists.txt`) → ใช้ใน `src/gui/Updater.cpp` + SidebarNav (version ซ้ายล่าง)
- bump แล้ว **sync ให้ตรง** 3 ที่: root CMake VERSION + `installer/AutoSuisei.iss` (`MyAppVersion`) + `scripts/make_user_guide_pdf.py` (2 จุด)

## Auto-update (v1.0.0)
- `src/gui/Updater.{h,cpp}` (Qt6::Network) — Check/Update ผ่าน **GitHub Releases REST API** (public, ไม่ต้อง token)
- **`scripts/make_update_package.ps1`** → `update-package.zip` = **bundle ลบ `python/`** (app-patch); updater
  ดาวน์โหลดอันนี้ก่อน, ไม่มี → fallback full installer. ⚠️ release ที่เปลี่ยน python deps **อย่าใส่ update-package** (ให้ลง full)
- release ต้องมี asset ชื่อ `update-package.zip` + `AutoSuisei-Setup-x.y.z.exe`; apply ผ่าน `.bat` (elevate UAC, Program Files)

## Artifacts (v1.0.0, 2026-06-26)
|                 | path                                                                             | size    |
| --------------- | -------------------------------------------------------------------------------- | ------- |
| exe             | `build/windows-x64-release/src/gui/AutoSuisei.exe` (rebuild — Updater + version) | ~800 KB |
| portable bundle | `C:\Users\hello\Backups\AutoSuisei\AutoSuisei-portable-20260626-184503` (+ .zip)  | 442 MB  |
| installer       | `C:\Users\hello\Backups\AutoSuisei\AutoSuisei-Setup-1.0.0.exe`                    | 124 MB  |
| **update-package** | `C:\Users\hello\Backups\AutoSuisei\update-package.zip` (app-patch, ไม่มี python/) | 72 MB |
| user guide      | `docs/AutoSuisei_User_Guide.pdf` (6 หน้า Thai+English, fpdf2+Tahoma)             | —       |

> ไล่รุ่น: 0.9.0 → 0.9.5 Monitor No. fusion → 0.9.6 Data Matrix serial (PC) → 0.9.7 Code128 (desktop/donate) →
> **1.0.0** monitor CN serial + version UI + GitHub auto-updater. *(push + `gh release create` = Phase 2b หลัง gh login)*

## Scripts dir resolution
- compile def `AUTOPILOT_SCRIPTS_DIR` ชี้ `source/scripts/` (override ด้วย env var ได้)
- exe ที่ bundle แล้วใช้ `<exeDir>/scripts/`
