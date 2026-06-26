---
tags: [idea, autosuisei, ocr, barcode]
updated: 2026-06-26
status: done
---

# Idea — อ่าน Barcode/QR แทน/เสริม OCR สำหรับ Serial

[[index]] · [[CLAUDE]] · log ที่ทำจริง: [[logs/2026-06-26-barcode-first-serial]]

## Problem
OCR (RapidOCR) อ่าน **Serial / Dell Service Tag** ได้ % เยอะแต่ **อ่านผิดบ่อย** โดยเฉพาะ
สับสน `O`↔`0`, `5`↔`6`, `I`↔`1` — ป้าย Dell มี **บาร์โค้ด** ที่ encode serial มาเป๊ะอยู่แล้ว

## ทำไม (motivation)
- บาร์โค้ด = ground-truth ของ serial (encode มาเป๊ะ ไม่มีปัญหารูปตัวอักษร)
- Data Matrix มี ECC → decode สำเร็จ = ถูกแน่; Code128 = ZBar conservative อ่านชัดถึงคืน
- ต้อง **distributable + ฟรี + offline** (ติดตั้งเครื่องอื่นสเปคต่ำได้) — ดู [[Dev-History]] trade-off triangle

## ขอบเขต
- **Barcode-first**: ลอง barcode ก่อน → อ่านไม่ออกค่อย fallback OCR → remark ที่มา (คอลัมน์ Src)
- ครอบคลุม: PC laptop (Data Matrix), desktop/donate (Code128) — ตามชนิดโค้ดบนป้ายจริง
- **No. คง OCR** (กระดาษขาว/เขียนมือ ไม่มีบาร์โค้ด)
- กรอง **CBA bank-asset tag** ทิ้ง (ไม่ใช่ serial เครื่อง)

## Acceptance
- บาร์โค้ดอ่านได้ต้อง **ถูกกว่าหรือเท่า OCR** (วัดบน batch จริง, spot-check เทียบป้าย)
- ไม่ regress: อ่านไม่ออก → OCR เดิม; ไม่มี wrong-read มา override OCR ที่ถูก
- ship ใน bundle ได้ (lib + DLL offline)

## ผลลัพธ์ (สรุป — ดู log)
ทำเสร็จ v0.9.6 (Data Matrix/pylibdmtx, PC) + v0.9.7 (Code128/pyzbar, desktop+donate).
วัดจริง: PC laptop serial coverage +10pp; donate disagreements **8/8 barcode ถูก OCR ผิด** (O↔0).
