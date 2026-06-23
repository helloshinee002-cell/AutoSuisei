---
tags: [autosuisei, modules, codebase]
updated: 2026-06-23
---

# Modules — โครงสร้าง `src/`

[[Home]] · pipeline [[Architecture]] · build [[Build-and-Distribution]]

โปรเจกต์ผ่านมาหลาย phase — โมดูลส่วนใหญ่เป็น **legacy** (ยัง compile/link แต่ GUI ปัจจุบันไม่ใช้)
ส่วนที่ **current** จริงๆ คือ `ocr` + `gui` + `cli`

| โมดูล | สถานะ | บทบาท |
|-------|-------|-------|
| `ocr/` | **current** ⭐ | `AssetExtractor` (regex parser, ใช้ 100%) + `OcrEngine` (Tesseract direct, GUI ไม่ใช้) + `OcrFormatter` + `ReviewModel` |
| `gui/` | **current** | Qt 6 frontend — ดูตารางด้านล่าง |
| `cli/` | **current** | headless runner: `autopilot_cli ocr <image>` |
| `core/` | legacy (Phase 0-2) | Macro engine + `Action` |
| `recorder/` | legacy | Input hooks (keyboard/mouse) |
| `player/` | legacy | Input synthesis (`SendInput`) |
| `web/` | legacy (Phase 3) | Chrome DevTools Protocol client |
| `scripting/` | legacy (Phase 4) | Lua sandbox |
| `vision/` | legacy (Phase 5) | OpenCV `ImageMatcher` (template matching) |
| `storage/` | legacy | SQLite repos (Phase 2 macro tab — ยัง DI ใน `MainWindow`) |

## GUI tabs (`src/gui/`)
- **Current**: `OcrTab`, `WatchTab`, `ReviewTab` + `MainWindow` + `SidebarNav` (240px nav)
  + `theme.qss` (Claude Design dark theme — ดู [[Dev-History]])
  - **ReviewTab QOL** (v0.9.4): ↑/↓ nav (`currentCellChanged`+`keyPressEvent`), Ctrl+wheel zoom
    (`QScrollArea`+`eventFilter`), Enter=Apply+Next (focus กลับ No.), Apply=verify
  - **MainWindow responsive** (v0.9.4): `setMinimumSize` + clamp `availableGeometry()` + ห่อ tab ด้วย
    `QScrollArea(widgetResizable)` กัน overlap ทุก resolution
- **Legacy**: `MacrosTab`, `WebTab`, `ImageTab`
- entry: `main.cpp`; resources: `resources.qrc`, `AutoSuisei.rc` (icon/version)

> **org field ลบแล้ว** (v0.9.2): `AssetInfo.orgName` / `ReviewRow.orgName` / Org column ใน OcrTab·ReviewTab·ReviewModel
> ออกหมด. **โมเดล/เทรน** อยู่นอก `src/`: `scripts/train/`, `scripts/sticker_digit.py`, `models/sticker_digit.onnx`
> ([[Sticker-Digit-Model]])

## Module boundaries (กฎ)
- `core` **ห้าม** depend บน `gui` หรือ `cli`
- `storage` คุยกับ `core` ผ่าน **interface** (DI ผ่าน `MainWindow` constructor)
- `gui` **ไม่เรียก WinAPI ตรง** — ผ่าน `recorder`/`player` (legacy) หรือ `QProcess` (current OCR path)

## Dependencies (vcpkg)
Qt 6 · OpenCV 4 · Tesseract 5 · SQLite 3 · spdlog · nlohmann/json · GoogleTest · Lua

## หมายเหตุ legacy
storage/SQLite object ยังถูกสร้างใน `MainWindow` (DI เดิม) แต่ flow OCR ปัจจุบันไม่ได้ใช้ —
อย่าเพิ่งรื้อถ้าไม่จำเป็น (อาจกระทบ macro tab ที่ยังลิงก์อยู่)
