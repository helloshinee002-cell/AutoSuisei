---
tags: [autosuisei, ocr, model, training, second-brain]
updated: 2026-06-23
---

# 🔢 Sticker-Digit Model — YOLOv8 + fusion

[[Home]] · parser [[OCR-and-Parser]] · ผล [[Accuracy-Results]] · ประวัติ [[Dev-History]]

> ตัวอ่าน **เลขสติกเกอร์เขียนมือ** บนกระดาษขาว (donate Photos-3-001, เลข 0-99). generic OCR
> (RapidOCR/Tesseract) ชนเพดาน + non-deterministic เพราะ detector ข้ามเลขเดี่ยวใหญ่บนพื้นขาว
> → เทรนโมเดลเฉพาะงาน ครั้งเดียวใช้ยาว (offline + recurring)

## สถาปัตยกรรม
- **YOLOv8n digit detector** (class 0-9): detect + อ่านเลขในภาพ → No. = เรียง digit ซ้าย→ขวา.
  localization + recognition ในโมเดลเดียว, **deterministic** (ต่างจาก OCR)
- export → **ONNX รันด้วย onnxruntime** (มีใน embedded python อยู่แล้ว — ไม่ต้อง bundle torch)
- inference **imgsz 512** ต้องตรงกับตอน train (เคยพลาด train 512 / infer 640 → recall ตก)

## ไฟล์
| ไฟล์ | บทบาท |
|---|---|
| `scripts/train/synth_stickers.py` | gen สติกเกอร์ปลอม (ฟอนต์ไทย + composite ทับ chassis จริง + augment glare/blur/scale) → dataset YOLO labeled **โดยไม่ต้อง label มือ** |
| `scripts/train/train_digit.py` | train YOLOv8n (transfer COCO) → export onnx (`cache='disk'` กัน OOM, `simplify=False`) |
| `scripts/sticker_digit.py` | onnxruntime inference → `read_sticker_number()` (letterbox + NMS + เรียงซ้าย-ขวา) |
| `models/sticker_digit.onnx` | โมเดล ~12 MB (commit ในรีโป; `make_bundle.ps1` copy เข้า bundle) |

## Fusion — `fuse_sticker_no(model, crop)`
โมเดล **high-precision / low-recall** (detect ถูกแต่ **หล่นหลัก** เช่น 1/5/9 บนพื้นขาว = synthetic→real gap)
→ ห้าม model-primary เดี่ยว (จะ regress). ใช้ **subsequence rule**:
- ถ้า model ⊆ crop **และ** crop ยาวกว่า → model หล่นหลัก → ใช้ crop
- ไม่งั้นใช้ model
- ⚠️ ต้อง fuse กับ **crop_side** = `crop_no or sticker_no_from_boxes or extract_pc_no_donate` (ไม่ใช่ crop เปล่า)
  — ไม่งั้นโมเดลเลขเดี่ยวบัง fallback 2 หลักที่ดีกว่า (เคยทำ authoritative ตก **60.9%** ทั้งที่ offline 79.7%)

## ผล
- Photos-3-001 (65 gt): crop ~55% / model ~64% / **fusion ~78%** (No.)
- **synthetic retrain plateaued** (51→50/65) → revert โมเดลเดิม (วินัย: ไม่ deploy การเปลี่ยนที่ไม่ขยับ metric)

## ค้าง / future (→ 95%)
- **real-data fine-tune** (Phase B รอบ 2): auto-box ภาพจริง (model detect + reconcile กับ gt + interpolate หลักที่หาย)
  → fine-tune real + synthetic แบบ split กัน leakage
- **recurring loop**: หลัง Review แต่ละ batch → เลขที่ verify แล้ว + box → เพิ่ม real set → retrain เป็นระยะ
- **Monitor batch**: No. = เลขสติกเกอร์ไทยเหมือนกัน → ถ้า parser-only ไม่พอ พิจารณา reuse โมเดลนี้ ([[Dev-History]] §ค้าง)

## ตรงไปตรงมา (ponytail)
- DonateMore เลข typed → **parser พอ ไม่ต้องใช้โมเดล** (99.6%). โมเดลเป็น lever เฉพาะ **เลขเขียนมือ** (Photos-3-001)
- torch ใช้แค่ตอน train (~2 GB, dev); inference ใช้ onnxruntime ที่มีอยู่แล้ว
- โมเดล deterministic + fusion เก็บ crop ไว้ → ไม่มีอะไรแย่ลงระหว่างทาง
