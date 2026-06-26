---
tags: [autosuisei, conventions, rules]
updated: 2026-06-26
---

# Conventions & Rules

[[Home]] · กฎ mirror อยู่ที่ [[OCR-and-Parser]] · workflow wiki + กฎ agent ที่ [[CLAUDE]]

แหล่ง: `Documents/AutoPilot/CLAUDE.md` (single source of truth ของโปรเจกต์โค้ด) + vault [[CLAUDE]] (wiki-agent)

## Mandatory
0. 🔴 **ห้ามเดา** — ถ้าไม่เข้าใจ requirement/โครงสร้าง/โค้ด ให้ **ถามก่อนเสมอ** (ห้ามเดาเด็ดขาดไม่ว่ากรณีใด); ไม่รู้ว่าโค้ด/ข้อมูลเป็นยังไง → อ่าน/วัดจริงก่อน
1. **TDD** — เขียน GTest ก่อนเสมอ แล้วค่อย implement
2. **Plan first** — ก่อนแก้ใหญ่ สร้าง/อัปเดต `docs/dev-plan.md` รอ approval
3. **Conventional Commits** — `feat: fix: docs: test: refactor: perf: chore: build:`
4. **ห้ามลบเทสต์เดิม** เว้นแต่ได้รับอนุญาตชัดเจน
5. **No raw `new`/`delete`** — ใช้ `std::unique_ptr` / `std::shared_ptr` / RAII
6. **No `using namespace std;`** ในไฟล์ header
7. **Const-correctness** — method ที่ไม่แก้ state ต้องเป็น `const`
8. **Header guards** = `#pragma once`
9. **ห้าม hardcode secrets**

## Style
- Naming: `PascalCase` (class/struct), `camelCase` (method/var), `UPPER_SNAKE` (constant)
- 4-space indent, line width 100
- Public function ต้องมี Doxygen `/** ... */`
- บังคับด้วย `.clang-format` + `.clang-tidy`

## ⚠️ STRICT gotchas
- **Parser single-source** — `scripts/bulk_extract.py` ↔ `src/ocr/AssetExtractor.cpp`
  ต้อง mirror 1:1 แก้ที่หนึ่งต้องแก้อีกที่ ([[OCR-and-Parser]])
- **`$args` สงวนใน PowerShell** — ใช้ `$nodeArgs` ฯลฯ แทน (เจอในสคริปต์ build)
- **Scripts dir** — `AUTOPILOT_SCRIPTS_DIR` ชี้ source; bundled exe ใช้ `<exeDir>/scripts/`
  ([[Build-and-Distribution]])
- **Rebrand UI-only** — exe/window/installer = "AutoSuisei"; namespace `autopilot::`,
  libs, CMake project, env var = ชื่อเดิม ([[Home]])
- **vcvars64 ก่อนทุก build** ([[Build-and-Distribution]])
- **Tests link debug deps** — รัน `ctest` ได้เฉพาะ debug preset
- **Unicode path (MSVC)** ⭐ — narrow `std::string` ที่ MSVC ตีเป็น **ANSI codepage** ไม่ใช่ UTF-8 → path ไทย/ยูนิโค้ดพัง.
  **ต้อง `std::filesystem::u8path(s)` ก่อนเข้า `fstream` / `fs::rename` / `fs::*` เสมอ** (เจอตอน Rename/Save CSV โฟลเดอร์ไทย v0.9.3)
- **Parser single-source (Python)** — `scripts/bulk_extract.py` ↔ `scripts/ocr_worker.py` (watch path) ต้อง mirror
  ด้วย (donate/monitor logic อยู่ฝั่ง Python เท่านั้น ไม่ได้อยู่ใน C++ `AssetExtractor`)
- **Embeddable Python** — `python._pth` ไม่ auto-add script dir → script ต้อง `sys.path.insert(0, Path(__file__).resolve().parent)`

## Module boundaries
ดู [[Modules]] — `core` ⊄ `gui`/`cli`; `storage` ผ่าน interface; `gui` ไม่เรียก WinAPI ตรง
