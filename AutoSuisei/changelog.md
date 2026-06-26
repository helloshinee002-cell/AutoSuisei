---
tags: [changelog, autosuisei]
updated: 2026-06-26
---

# Changelog

[[index]] · [[Home]] · รายละเอียดแต่ละครั้งดู `logs/`

> 1 บรรทัด/release|งาน · ใหม่บนสุด · เวอร์ชันปัจจุบัน = **1.0.0** ✅ [released บน GitHub](https://github.com/helloshinee002-cell/AutoSuisei/releases/tag/v1.0.0)

- **1.0.0** (2026-06-26) — **Monitor barcode-first CN serial** (Data Matrix + 1D Code128 rotation sweep, ~60%) + **version UI ซ้ายล่าง** + **GitHub auto-updater** (Check/Update, partial update-package, tokenless) + single-source version. → [[logs/2026-06-26-v1.0.0-updater-monitor]]
- **0.9.7** (2026-06-26) — Barcode-first Serial เพิ่ม **Code128** (`pyzbar`/ZBar) สำหรับ desktop + ขยายไป `donate`; lazy ZBar→DataMatrix→cv2; แก้ OCR `O↔0` (วัด 8/8 ถูก). → [[logs/2026-06-26-barcode-first-serial]]
- **0.9.6** (2026-06-26) — Barcode-first Serial: อ่าน Dell **Data Matrix** (`pylibdmtx`) ก่อน OCR (PC); remark คอลัมน์ **Src**; +10pp serial coverage; ขยาย Review image pane. → [[logs/2026-06-26-barcode-first-serial]]
- **0.9.5** (2026-06-24) — Monitor No. = crop+model **fusion** ~42% (เพดาน free/local) + แก้บั๊ก unicode-imread (path ไทย). → [[Accuracy-Results]]
- **0.9.4** (2026-06-23) — Review QOL (arrow nav, zoom, Enter=Apply) + responsive multi-resolution UI.
- **0.9.3** (2026-06-22) — แก้ Unicode-path (`u8path`): Rename + Save CSV พังบนโฟลเดอร์ชื่อไทย.
- **0.9.2** (2026-06-22) — ลบ org reader (Tesseract `tha` อ่านชื่อ รร. ลายมือมั่ว); donate เหลือ No. + Serial.
- **0.9.1** — donate/DonateMore + YOLOv8 sticker-digit model (เลขเขียนมือ).
- **0.9.0** (2026-05-18) — rebrand AutoPilot→AutoSuisei (UI-only) + GUI redesign (Claude Design, sidebar) + parser category-aware + rotation fallback.
