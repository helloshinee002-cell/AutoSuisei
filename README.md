# AutoPilot

**Windows Automation Suite** — Macro Recorder + Web Recorder + OCR ในตัวเดียว เขียนด้วย C++20

## Highlights
- บันทึก/เล่น keyboard + mouse + window focus
- ควบคุม browser ผ่าน Chrome DevTools Protocol
- อ่านข้อความ/เลขจากภาพ (Tesseract + OpenCV) — เหมาะกับเคสไฟล์ภาพชื่อ random ที่ต้องการ map กับเนื้อหาในภาพ
- เขียน logic ใน macro ด้วย Lua (sandboxed)
- เก็บ macro + OCR result ใน SQLite (WAL)
- GUI ด้วย Qt 6 + headless CLI mode

## Prerequisites (Windows)
- Visual Studio 2022 Build Tools (MSVC v143)
- CMake ≥ 3.25
- Ninja
- vcpkg (ตั้ง env `VCPKG_ROOT`)
- Qt 6.7+ (จะลงผ่าน vcpkg ก็ได้)
- Tesseract traineddata (`eng.traineddata`, `tha.traineddata`) วางใน `%TESSDATA_PREFIX%`

## Quick Start
```powershell
# 1. ลง dependency
vcpkg install --triplet x64-windows

# 2. Configure + build (debug)
cmake --preset windows-x64-debug
cmake --build --preset windows-x64-debug

# 3. รัน test
ctest --preset windows-x64-debug --output-on-failure

# 4. รัน GUI
.\build\windows-x64-debug\src\gui\AutoPilot.exe

# 5. OCR ผ่าน CLI
.\build\windows-x64-debug\src\cli\autopilot_cli.exe ocr "C:\path\to\image.png"
```

## Project Structure
ดู [CLAUDE.md](./CLAUDE.md) — เป็น single source of truth สำหรับ stack/rules/workflow

## Workflow
อ้างอิง [docs/SKILLS_AUTOMATION_WORKFLOW.md](./docs/SKILLS_AUTOMATION_WORKFLOW.md)
ทุก feature ต้องผ่าน 5 step: Explore → Plan → Test → Implement → Verify → Report

## License
TBD
