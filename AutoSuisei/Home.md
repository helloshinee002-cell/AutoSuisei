---
tags: [moc, autosuisei]
updated: 2026-06-26
---

# 🏠 AutoSuisei — Knowledge Vault

> Second-brain ของโปรเจกต์ **AutoSuisei** (เดิมชื่อ AutoPilot) — Windows desktop tool
> ที่ใช้ OCR ดึง **PC No. + Dell serial** จากภาพถ่ายมือถือของ PC ที่ wipe-off แล้ว →
> review/แก้ใน GUI → rename ไฟล์ภาพอัตโนมัติ

## สถานะ (ณ 2026-06-26)
- **เวอร์ชัน**: **v1.0.0** — Monitor barcode CN serial + version UI (ซ้ายล่าง) + GitHub auto-updater (Check/Update) (build `AutoSuisei-Setup-1.0.0.exe`)
  - ไล่จาก v0.9.0 (rebrand) → 0.9.5 Monitor No. fusion → 0.9.6 Data Matrix serial (PC) → 0.9.7 Code128 (desktop/donate) → **1.0.0** monitor CN + updater
- **Git**: branch `main` — **commit v0.9.1–1.0.0 ค้าง push** (Phase 2b: `gh auth login` → push + `gh release create v1.0.0`)
- **Remote**: https://github.com/helloshinee002-cell/AutoSuisei.git
- **กำลังทำต่อ (ส่วนที่ 2)**: commit/push · bump 1.0.0 · monitor barcode/QR · version UI ซ้ายล่าง + Check/Update Version. Monitor No. ยังรอ retrain (recurring-loop)
- **Wiki**: [[index]] (entry-point) · [[CLAUDE]] (กฎ + Mandatory Wiki Update) · [[changelog]] · `logs/`

## Quick facts
|             |                                                                |
| ----------- | -------------------------------------------------------------- |
| ภาษา        | C++20 (parser + UI) + Python (PaddleOCR sidecar)               |
| Platform    | Windows 10/11 x64                                              |
| Build       | CMake 3.25+ + vcpkg (manifest mode), Ninja, MSVC v143          |
| GUI         | Qt 6 Widgets — sidebar nav 240px + 3 หน้า OCR / Watch / Review (responsive, keyboard QOL) |
| OCR engine  | PaddleOCR ผ่าน `rapidocr-onnxruntime` (~97.9%) + YOLOv8 sticker-digit model (donate No.) |
| OCR หมวด    | PC&Laptop / Monitor / Accessory / **donate** (No. + Serial)    |
| Tests       | GoogleTest, **109 tests** (`autopilot_tests`)                  |
| ความแม่นรวม | 6 datasets — ดู [[Accuracy-Results]]                           |

## 🗺️ Index
- [[index]] — entry-point (เวอร์ชัน + folder map) · [[CLAUDE]] — กฎ wiki-agent (ห้ามเดา) · [[changelog]] — ไทม์ไลน์
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
