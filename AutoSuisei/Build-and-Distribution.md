---
tags: [autosuisei, build, release]
updated: 2026-06-17
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
ctest --preset windows-x64-debug --output-on-failure              # ทั้งหมด (106 tests)
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
- สคริปต์: `scripts/make_installer.ps1`, `scripts/make_bundle.ps1`, `scripts/setup_embedded_python.ps1`
- embedded Python = **3.11.9** (`build/embedded-python/`, `python-embed-3.11.9.zip`)
  — แยกจาก Python ที่ใช้ตอน dev

## Artifacts (v0.9.0, 2026-05-18)
| | path | size |
|---|---|---|
| exe | `build/windows-x64-release/src/gui/AutoSuisei.exe` | ~776 KB |
| portable bundle | `C:\Users\hello\Backups\AutoSuisei\AutoSuisei-portable-20260518-201724` | 428 MB |
| installer | `C:\Users\hello\Backups\AutoSuisei\AutoSuisei-Setup-0.9.0.exe` | 108 MB |
| user guide | `docs/AutoSuisei_User_Guide.pdf` (6 หน้า Thai+English, fpdf2+Tahoma) | — |

## Scripts dir resolution
- compile def `AUTOPILOT_SCRIPTS_DIR` ชี้ `source/scripts/` (override ด้วย env var ได้)
- exe ที่ bundle แล้วใช้ `<exeDir>/scripts/`
