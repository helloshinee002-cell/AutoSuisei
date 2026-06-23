---
tags: [autosuisei, overview]
updated: 2026-06-23
---

# Overview — ปัญหา & Workflow

[[Home]]

## ปัญหาที่แก้
เวลาปลด PC/Laptop/Monitor/Accessory ออกจากการใช้งาน เครื่องจะถูก **wipe-off** (ลบข้อมูล)
แล้วถ่ายรูปด้วยมือถือเก็บไว้เป็นหลักฐาน. ไฟล์ภาพมีชื่อแบบ random จากกล้อง ต้อง map กับ
**PC No.** (เลขครุภัณฑ์) และ **Dell serial / Service Tag** ที่ปรากฏในภาพ — ทำมือทีละพันรูปไม่ไหว

AutoSuisei ทำให้อัตโนมัติ: OCR อ่านเลขจากภาพ → ให้คนตรวจ/แก้ → **rename ไฟล์** ตาม
PC No. + serial ที่ถูกต้อง

## Workflow 3 ขั้น (3 หน้าใน GUI)
1. **OCR tab** — เลือกโฟลเดอร์ภาพ + เลือก category (PC&Laptop / Monitor / Accessory)
   → bulk extract ทั้งโฟลเดอร์ → ได้ตาราง No./Serial ต่อภาพ
2. **Watch tab** — เฝ้าโฟลเดอร์แบบ live: มีภาพใหม่เข้ามาก็ OCR ทันที (KPI cards)
3. **Review tab** — ตรวจ/แก้ผลทีละแถว (progress bar) → save `ground_truth.csv` → **rename ไฟล์บนดิสก์**

ดูภาพ flow แบบเต็มที่ [[Architecture]] และตรรกะการดึงเลขที่ [[OCR-and-Parser]]

## Input / Output
- **Input**: โฟลเดอร์ภาพ (`.jpg/.jpeg/.png/.bmp/.tif/.tiff/.webp`)
  - ชื่อไฟล์อาจมี metadata: batch `(123)`, วันที่ `_YYMMDD_`, ลำดับ `_NN.jpg`,
    ช่วงเลข `Laptop 301-400` → ใช้เป็น **range hint** ([[OCR-and-Parser]])
- **Output**: CSV (`train2_paddle.csv` / `ground_truth.csv`) + ไฟล์ภาพที่ถูก rename

## หมวดเป้าหมาย (categories)
- **PC&Laptop** — Dell Service Tag 7 ตัวอักษร
- **Monitor** — Dell S/N เต็มรูป `CN-XXXXX-XXXXX-XXX-XXXX(-A00)?`
- **Accessory** — Olivetti / Verifone / Feitian ฯลฯ (รูปแบบ serial หลากหลาย)
- **donate** (เพิ่ม v0.9.1) — เครื่องบริจาคโรงเรียน: No. (เลขสติกเกอร์) + Serial. screen (Notepad/wmic) +
  chassis (Dell tag + sticker); เลขเขียนมือใช้ sticker model+fusion ([[OCR-and-Parser]] · [[Sticker-Digit-Model]])
