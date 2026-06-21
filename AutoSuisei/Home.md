---
tags: [moc, autosuisei]
updated: 2026-06-17
---

# 🏠 AutoSuisei — Knowledge Vault

> Second-brain ของโปรเจกต์ **AutoSuisei** (เดิมชื่อ AutoPilot) — Windows desktop tool
> ที่ใช้ OCR ดึง **PC No. + Dell serial** จากภาพถ่ายมือถือของ PC ที่ wipe-off แล้ว →
> review/แก้ใน GUI → rename ไฟล์ภาพอัตโนมัติ

## สถานะ (ณ 2026-06-17)
- **เวอร์ชัน**: v0.9.0 (rebrand AutoPilot → AutoSuisei, 2026-05-18)
- **Git**: branch `main`, working tree สะอาด, **sync กับ origin แล้ว** (push ขึ้น GitHub เรียบร้อย)
- **Remote**: https://github.com/helloshinee002-cell/AutoSuisei.git

## Quick facts
| | |
|---|---|
| ภาษา | C++20 (parser + UI) + Python (PaddleOCR sidecar) |
| Platform | Windows 10/11 x64 |
| Build | CMake 3.25+ + vcpkg (manifest mode), Ninja, MSVC v143 |
| GUI | Qt 6 Widgets — sidebar nav 240px + 3 หน้า OCR / Watch / Review |
| OCR engine | PaddleOCR ผ่าน `rapidocr-onnxruntime` (~97.9%) |
| Tests | GoogleTest, 106 tests (`autopilot_tests`) |
| ความแม่นรวม | 4 datasets / 2104 ภาพ — ดู [[Accuracy-Results]] |

## 🗺️ Index
- [[Overview]] — ปัญหาที่แก้ + workflow 3 ขั้น
- [[Architecture]] — pipeline + GUI↔Python IPC + data flow
- [[OCR-and-Parser]] — ⭐ หัวใจ: PaddleOCR + parser 3 category + rotation fallback
- [[Modules]] — โครงสร้าง `src/` (current vs legacy) + GUI tabs
- [[Build-and-Distribution]] — build/test commands + installer + bundle
- [[Accuracy-Results]] — ตารางความแม่นรายชุดข้อมูล
- [[Dev-History]] — phase 0-11 + session 2026-05-18 + key learnings
- [[Conventions]] — กฎบังคับ + style + STRICT gotchas

## หมายเหตุการตั้งชื่อ
Rebrand เป็น UI-only: **exe / window title / installer** = "AutoSuisei" แต่
folder source (`Documents\AutoPilot`), C++ namespace `autopilot::`, library targets,
root CMake project name, และ env var `AUTOPILOT_SCRIPTS_DIR` **ยังคงชื่อเดิม** เพื่อลด churn
