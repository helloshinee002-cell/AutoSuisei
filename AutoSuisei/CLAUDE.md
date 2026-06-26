---
tags: [meta, rules, autosuisei, wiki-agent]
updated: 2026-06-26
---

# CLAUDE.md — กฎของ AutoSuisei Second-Brain (Wiki Agent)

> ไฟล์นี้คือ **กฎการทำงานของ AI agent** กับ vault นี้. อ่านก่อนทุกครั้งที่ทำงานในโปรเจกต์ AutoSuisei.
> vault นี้ = สมองที่สอง (second brain) ของโปรเจกต์ — เก็บความรู้ + บันทึกงานทุกครั้งที่แก้โค้ด.

---

## 🔴 Rule #0 — ห้ามเดา (สำคัญสุด, override ทุกข้อ)

**ถ้าไม่เข้าใจ ให้ถามก่อนเสมอ — ห้ามเดาเด็ดขาด ไม่ว่ากรณีใด ๆ ก็ตาม**

- ไม่ชัดเรื่อง requirement / โครงสร้าง / ชื่อไฟล์ / พฤติกรรมที่ต้องการ → **ถาม** (AskUserQuestion) ก่อนลงมือ
- ไม่รู้ว่าโค้ด/ข้อมูลเป็นยังไง → **อ่าน/วัดจริง** ก่อน ห้ามสมมติ
- เดา = ผิดกฎ. "น่าจะ…" ที่ยังไม่ยืนยัน = ต้องยืนยันก่อนใช้

## 🟠 Mandatory Wiki Update — ห้ามข้าม

**ทุกครั้งที่เขียน/แก้โค้ดเสร็จ ต้องบันทึก wiki เสมอ** (ถือเป็นส่วนหนึ่งของงาน ไม่ใช่ option):

1. สร้างไฟล์ใหม่ใน `logs/` ชื่อ `YYYY-MM-DD-<topic>.md` (1 ไฟล์ต่อ 1 ครั้งงาน)
2. เติม 1 บรรทัดสรุปใน `changelog.md` (ใหม่อยู่บนสุด)
3. ถ้าความรู้เปลี่ยน (เช่น parser/accuracy/build เปลี่ยน) → อัปเดต reference note ที่เกี่ยว ([[OCR-and-Parser]], [[Accuracy-Results]], [[Build-and-Distribution]], …) + `updated:` ใน frontmatter

### Log entry template (4 ฟิลด์บังคับ)
```markdown
---
date: YYYY-MM-DD HH:MM
idea: "[[ideas/<file>]]"
tags: [log, autosuisei]
---
# <topic>
- **Date/Time:** <เวลาที่ทำงานจริง>
- **Idea Ref:** [[ideas/<file>]]   ← ไฟล์ idea ต้นทาง (ถ้าไม่มี idea file ให้ระบุ "ad-hoc: <โจทย์>")
- **Changes Made:** <สร้าง/แก้ฟีเจอร์อะไร + ไฟล์ไหนถูกกระทบบ้าง>
- **Technical Notes:** <ทำไมเลือกวิธีนี้ / จุดที่ต้องระวังในอนาคต (ถ้ามี)>
```

## 📁 Folder Conventions

```
AutoSuisei/
  CLAUDE.md         ← ไฟล์นี้ (กฎ wiki-agent)
  index.md          ← entry-point: เวอร์ชันปัจจุบัน + folder map + ลิงก์ทางลัด
  Home.md           ← human MOC (สารบัญความรู้ ดูภาพรวม)
  changelog.md      ← 1 บรรทัด/release|งาน (ใหม่บนสุด)
  *.md (root)       ← reference notes ถาวร: Overview, Architecture, Modules,
                      OCR-and-Parser, Sticker-Digit-Model, Accuracy-Results,
                      Build-and-Distribution, Dev-History, Conventions
  logs/             ← 1 ไฟล์/ครั้งงาน  (YYYY-MM-DD-<topic>.md) — append-only history
  ideas/            ← idea/โจทย์ ต้นทางงาน; log อ้างด้วย [[ideas/<file>]]
  .obsidian/        ← config (templater, excalidraw) — ห้ามแก้มือ
```

**แยกบทบาท**: `ideas/` = "จะทำอะไร" · `logs/` = "ทำอะไรไปแล้ว (ครั้ง ๆ)" · reference notes = "ความรู้ปัจจุบัน (สถานะล่าสุด)" · `changelog.md` = ไทม์ไลน์รวบสั้น.

## 🔄 Ingest procedure (idea → code → log)

1. **รับ idea**: โจทย์ใหม่ → สร้าง `ideas/<slug>.md` (problem, ทำไม, ขอบเขต, acceptance)
2. **ทำงาน**: ออกแบบ → (ถ้าไม่ชัด **ถามก่อน** ตาม Rule #0) → implement + วัด/ทดสอบจริง
3. **บันทึก (บังคับ)**: `logs/<date>-<topic>.md` (4 ฟิลด์) อ้าง `[[ideas/<slug>]]` + เติม `changelog.md` + อัปเดต reference note ที่เกี่ยว
4. **ตัวอย่างจริง**: [[ideas/barcode-qr-serial]] → [[logs/2026-06-26-barcode-first-serial]] → บรรทัดใน [[changelog]]

## ✍️ Obsidian conventions

- ทุกโน้ตขึ้นต้นด้วย frontmatter: `tags: [...]` + `updated: YYYY-MM-DD` (logs ใช้ `date:` แทน)
- ลิงก์ภายในใช้ `[[ ]]` คร่อมชื่อไฟล์ (ไม่ใส่ `.md`) เช่น [[OCR-and-Parser]], [[ideas/barcode-qr-serial]]
- ภาษา: ไทยอธิบาย + ศัพท์เทคนิค/identifier เป็นอังกฤษ (ตามสไตล์ vault เดิม)
- กฎโค้ดของโปรเจกต์ (TDD, mirror parser, u8path ฯลฯ) อยู่ที่ [[Conventions]] (mirror `Documents/AutoPilot/CLAUDE.md`)

> กฎ Rule #0 "ห้ามเดา" ถูก mirror ไว้ที่ [[Conventions]] และ project `Documents/AutoPilot/CLAUDE.md` ด้วย (ใช้กับงานโค้ดทั้งหมด ไม่ใช่แค่ wiki)
