---
tags: [moc, autosuisei]
updated: 2026-06-23
---

# 🏠 AutoSuisei — Knowledge Vault

> Second-brain ของโปรเจกต์ **AutoSuisei** (เดิมชื่อ AutoPilot) — Windows desktop tool
> ที่ใช้ OCR ดึง **PC No. + Dell serial** จากภาพถ่ายมือถือของ PC ที่ wipe-off แล้ว →
> review/แก้ใน GUI → rename ไฟล์ภาพอัตโนมัติ

## สถานะ (ณ 2026-06-23)
- **เวอร์ชัน**: **v0.9.4** — review QOL + responsive UI (build `AutoSuisei-Setup-0.9.4.exe`)
  - ไล่จาก v0.9.0 (rebrand) → 0.9.1 donate/DonateMore + sticker model → 0.9.2 ลบ org → 0.9.3 Thai-path fix → 0.9.4
- **Git**: branch `main`, working tree สะอาด — **commit v0.9.1–0.9.4 ค้าง push** (รอ user auth PAT)
- **Remote**: https://github.com/helloshinee002-cell/AutoSuisei.git
- **กำลังทำต่อ**: Monitor batch (จูน No. parser) — ดู [[Dev-History]] §ค้าง

## Quick facts
|             |                                                                |
| ----------- | -------------------------------------------------------------- |
| ภาษา        | C++20 (parser + UI) + Python (PaddleOCR sidecar)               |
| Platform    | Windows 10/11 x64                                              |
| Build       | CMake 3.25+ + vcpkg (manifest mode), Ninja, MSVC v143          |
| GUI         | Qt 6 Widgets — sidebar nav 240px + 3 หน้า OCR / Watch / Review (responsive, keyboard QOL) |
| OCR engine  | PaddleOCR ผ่าน `rapidocr-onnxruntime` (~97.9%) + YOLOv8 sticker-digit model (donate No.) |
| OCR หมวด    | PC&Laptop / Monitor / Accessory / **donate** (No. + Serial)    |
| Tests       | GoogleTest, **107 tests** (`autopilot_tests`)                  |
| ความแม่นรวม | 6 datasets — ดู [[Accuracy-Results]]                           |

## 🗺️ Index
- [[Overview]] — ปัญหาที่แก้ + workflow 3 ขั้น
- [[Architecture]] — pipeline + GUI↔Python IPC + data flow
- [[OCR-and-Parser]] — ⭐ หัวใจ: PaddleOCR + parser 4 category + rotation fallback
- [[Sticker-Digit-Model]] — YOLOv8 digit detector + fusion (donate เลขเขียนมือ)
- [[Modules]] — โครงสร้าง `src/` (current vs legacy) + GUI tabs
- [[Build-and-Distribution]] — build/test commands + installer + bundle
- [[Accuracy-Results]] — ตารางความแม่นรายชุดข้อมูล
- [[Dev-History]] — phase 0-11 + session 2026-05-18 + key learnings
- [[Conventions]] — กฎบังคับ + style + STRICT gotchas

## หมายเหตุการตั้งชื่อ
Rebrand เป็น UI-only: **exe / window title / installer** = "AutoSuisei" แต่
folder source (`Documents\AutoPilot`), C++ namespace `autopilot::`, library targets,
root CMake project name, และ env var `AUTOPILOT_SCRIPTS_DIR` **ยังคงชื่อเดิม** เพื่อลด churn
