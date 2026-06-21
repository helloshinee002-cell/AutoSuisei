---
tags: [autosuisei, conventions, rules]
updated: 2026-06-17
---

# Conventions & Rules

[[Home]] · กฎ mirror อยู่ที่ [[OCR-and-Parser]]

แหล่ง: `CLAUDE.md` (single source of truth ของโปรเจกต์)

## Mandatory
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

## Module boundaries
ดู [[Modules]] — `core` ⊄ `gui`/`cli`; `storage` ผ่าน interface; `gui` ไม่เรียก WinAPI ตรง
