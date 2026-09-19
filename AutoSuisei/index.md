---
tags: [index, moc, autosuisei]
updated: 2026-09-19
---

# 📇 index — AutoSuisei Second-Brain (entry-point)

> จุดเริ่มของ vault. อ่าน [[CLAUDE]] ก่อนทำงาน (กฎ wiki-agent + Rule #0 "ห้ามเดา").

| | |
|---|---|
| **เวอร์ชันปัจจุบัน** | **v1.0.5** ✅ released (AI assist) ([releases](https://github.com/helloshinee002-cell/AutoSuisei/releases)) |
| **โปรเจกต์** | AutoSuisei (เดิม AutoPilot) — OCR ดึง No. + Dell serial จากภาพถ่าย → review → rename |
| **source repo** | เครื่องเดิม `C:\Users\hello\Documents\AutoPilot` · เครื่องใหม่ `D:\WorkSpace\AutoSuisei` (2026-09-18) · remote `github.com/helloshinee002-cell/AutoSuisei` |

## 🗺️ Folder map
- [[CLAUDE]] — **กฎ wiki-agent** (Rule #0 ห้ามเดา · Mandatory Wiki Update · conventions · ingest)
- [[Home]] — human MOC (สารบัญความรู้เต็ม + quick facts)
- [[changelog]] — ไทม์ไลน์รวบ 1 บรรทัด/release
- `logs/` — บันทึกราย session (ล่าสุด: [[logs/2026-09-18-v1.0.5-ai-assist]])
- `ideas/` — โจทย์/ไอเดียต้นทาง (เช่น [[ideas/barcode-qr-serial]])
- **reference notes** (root): [[Overview]] · [[Architecture]] · [[Modules]] · [[OCR-and-Parser]] · [[Sticker-Digit-Model]] · [[Accuracy-Results]] · [[Build-and-Distribution]] · [[Dev-History]] · [[Conventions]]

## ▶️ เริ่มงานใหม่ยังไง
1. อ่าน [[CLAUDE]] (กฎ) → ถ้าโจทย์ไม่ชัด **ถามก่อน** (Rule #0)
2. มี idea → จด `ideas/<slug>.md` · ทำงาน + วัด/ทดสอบจริง
3. **เสร็จแล้วบังคับ**: `logs/<date>-<topic>.md` (4 ฟิลด์) + เติม [[changelog]] + อัปเดต reference note ที่เกี่ยว
