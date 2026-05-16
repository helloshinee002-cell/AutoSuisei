# Project: AutoPilot — Windows Automation Suite

Macro Recorder + Web Recorder + OCR ในตัวเดียว เขียนด้วย C++20 บน Windows

## Tech Stack
- **Language**: C++20
- **Platform**: Windows 10/11 (x64)
- **Build**: CMake 3.25+ with **vcpkg** manifest mode
- **GUI**: Qt 6 (Widgets + QML สำหรับ flow editor)
- **OCR**: Tesseract 5 + Leptonica + OpenCV 4 (preprocess)
- **Storage**: SQLite 3 (เก็บ macro definitions + OCR results)
- **Scripting**: Lua 5.4 + sol2 (ให้ผู้ใช้เขียน logic ใน macro)
- **Web**: Chrome DevTools Protocol via WebSocket (libwebsockets)
- **Logging**: spdlog
- **JSON**: nlohmann/json
- **Testing**: GoogleTest + GoogleMock
- **Input synthesis**: WinAPI (`SendInput`, `SetWindowsHookEx`)

## Build & Test Commands
```powershell
# First-time setup
vcpkg install --triplet x64-windows

# Configure + build
cmake --preset windows-x64-debug
cmake --build --preset windows-x64-debug

# Run all tests
ctest --preset windows-x64-debug --output-on-failure

# Single test
ctest --preset windows-x64-debug -R "OcrEngine.*" --output-on-failure

# Run app
.\build\windows-x64-debug\src\gui\AutoPilot.exe

# Lint/format
clang-format -i src/**/*.{cpp,h}
clang-tidy -p build/windows-x64-debug src/**/*.cpp
```

## Project Rules

### Mandatory
1. **IMPORTANT**: ใช้ Test-Driven Development (TDD) — เขียน GTest ก่อนเสมอแล้วค่อย implement
2. **Plan Mode**: ก่อนแก้โค้ดจริงทุกครั้ง ให้สร้าง/อัปเดต `docs/dev-plan.md` และรออนุมัติ
3. **Conventional Commits**: `feat:`, `fix:`, `docs:`, `test:`, `refactor:`, `perf:`, `chore:`
4. **ห้ามลบเทสต์เดิม** เว้นแต่ได้รับอนุญาตชัดเจน
5. **No raw `new`/`delete`** — ใช้ `std::unique_ptr` / `std::shared_ptr` / RAII เท่านั้น
6. **No `using namespace std;`** ในไฟล์ header
7. **Const-correctness** — method ที่ไม่แก้ state ต้องเป็น `const`
8. **Header guards**: ใช้ `#pragma once`
9. **ห้าม hardcode secrets** — ใช้ environment variables หรือ Windows DPAPI

### Style
- Naming: `PascalCase` สำหรับ class/struct, `camelCase` สำหรับ method/variable, `UPPER_SNAKE` สำหรับ constant
- 4-space indentation, line width 100
- ทุก public function ต้องมี Doxygen comment (`/** ... */`) อธิบาย params + return + throws

## Directory Structure
```
src/
  core/        Macro engine, event loop, action types
  recorder/    Input hooks (keyboard, mouse, window)
  player/      Input synthesis (SendInput) + replay
  ocr/         Tesseract wrapper + OpenCV preprocessing
  web/         Chrome DevTools Protocol client
  scripting/   Lua bridge (sol2)
  storage/     SQLite repositories
  gui/         Qt 6 frontend
  cli/         Headless runner
tests/         GTest unit + integration
examples/      Sample macros + OCR demos
docs/          dev-plan.md, ADRs, architecture
```

## Module Boundaries
- `core` ห้าม depend บน `gui` หรือ `cli`
- `storage` คุยกับ `core` ผ่าน interface เท่านั้น (dependency inversion)
- `gui` ห้ามเรียก WinAPI ตรง — ผ่าน `recorder`/`player`

## Workflow (ทุก feature)
อ้างอิง `docs/SKILLS_AUTOMATION_WORKFLOW.md`:
1. **Explore** — อ่านโค้ดที่เกี่ยวข้อง
2. **Plan** — เขียน/อัปเดต `docs/dev-plan.md` รออนุมัติ
3. **Test first** — เขียน GTest ที่ fail ก่อน
4. **Implement** — เขียนโค้ดให้ test ผ่าน + รัน clang-format + clang-tidy
5. **Verify** — `ctest` ทั้งหมด ต้องผ่าน 100%
6. **Report** — สรุปการเปลี่ยนแปลง + ร่างข้อความ commit

## MCP Servers
ดู `.mcp.json` — มี `github` (PR management) และ `filesystem` (file ops scoped)
