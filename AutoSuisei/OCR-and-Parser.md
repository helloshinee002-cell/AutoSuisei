---
tags: [autosuisei, ocr, parser, core]
updated: 2026-06-23
---

# ⭐ OCR & Parser — หัวใจของระบบ

[[Home]] · ผลความแม่น [[Accuracy-Results]] · กฎ mirror [[Conventions]]

แหล่งโค้ด: `scripts/bulk_extract.py` (Python, source of truth ฝั่งรัน) ↔
`src/ocr/AssetExtractor.{h,cpp}` (C++, mirror 1:1)

## OCR engine
- **PaddleOCR** ผ่าน `rapidocr-onnxruntime` (Python sidecar) — ความแม่นป้ายเลข ~97.9%
- **Tesseract 5** ยังลิงก์ไว้ใน `OcrEngine` (C++ direct) แต่ **GUI ไม่ใช้**
- **OpenCV 4** — image I/O + rotation + (Phase 5) template matching

## Parser แยกตาม category
ฟังก์ชันรวม: `extract_serial(text, category)` แตกเป็น **4 ทาง** (pc / monitor / accessory / donate) + `extract_pc_no()` ใช้ร่วม
> donate parser + monitor serial + sticker model = **ฝั่ง Python เท่านั้น** (GUI ใช้ sidecar); C++ `AssetExtractor`
> mirror เฉพาะ `extract_pc_no` + `extract_serial_pc`

### `extract_pc_no(text, range_hint)` — เลขครุภัณฑ์ (ทุก category)
1. หา `No.NN` ด้วย `PC_NO_RE` — วน finditer ทุก match, **เลือกตัวที่อยู่ใน range ก่อน** (Phase 9.5)
2. fallback: บรรทัดที่เป็นเลขโดดๆ 2-3 หลัก (`PC_NO_STANDALONE_LINE_RE`)
3. ถ้าไม่มีใน range → คืน match แรกที่เจอ
- **Range hint**: ชื่อไฟล์ `Laptop 301-400` → กรองให้ PC No. อยู่ใน `[301,400]`
- ⚠️ **gap (เจอ Monitor batch 2026-06-23)**: สติกเกอร์ "โรงเรียน… `<N>`" ไม่มีคำ "No." + `PC_NO_STANDALONE_LINE_RE`
  จับเฉพาะ **2-3 หลัก** → **เลขหลักเดียว 1-9 อ่านไม่ได้เลย**. แก้ที่วางแผน: reuse donate sticker-No.
  (เลขท้ายบรรทัดที่มีอักษรไทย) เป็น last-resort fallback ([[Dev-History]] §ค้าง)

### `extract_serial_pc()` — PC&Laptop (Dell Service Tag 7 ตัว)
- labeled `S/N:` / `SERVICE TAG` → 7-char (`SERIAL_LABELED_RE`)
- fallback standalone 7-char ที่มี alpha≥3 และ digit≥2
- **`SERIAL_BLOCKLIST`** กันคำลวง: `PASS1OF, DISK0C1, DRIVE00, NTFSSIZ, BOOTXOF, ELAPSED, REMOVE0, FIXED00`

### `extract_serial_monitor()` — Dell monitor `CN-…-A00`
- รูป: `CN-07C2R4-72872-2BD-A8MM` / `CN-0JF27G-FCC00-76M-AKNB-A00` (5-6 segment, segment แรก = `CN`)
- **ไม่ fallback ไป 7-char Service Tag เด็ดขาด** (user requirement 2026-05-18)
- pre-process ก่อน match:
  1. ลบบรรทัด **CBA asset barcode** (`CBA1000…` = ครุภัณฑ์ธนาคาร ไม่ใช่ serial)
  2. ลบบรรทัด **numeric noise** ≥7 หลัก (Express SVC code / เบอร์โทร ที่แทรกกลาง serial)
  3. collapse `-\n` ให้ segment ข้ามบรรทัด
  4. **`_merge_cn_fragments()`** — รวมบรรทัด CN-fragment ที่ OCR ตัดเป็นท่อนๆ (เผลอทิ้ง `-` ท้าย)
- ทน photo ที่ถูกตัด: ขั้นต่ำ 3 segment หลัง `CN`

### `extract_serial_accessory()` — flexible (Olivetti/Verifone/Feitian)
ลองตามลำดับ:
1. labeled `S/N:` / `Serial:` + ค่า (alnum + dash, 5-20 ตัว)
2. บรรทัดตัวเลข+dash เช่น `261-535-477` (Verifone)
3. บรรทัดเลขล้วน 8-15 หลัก เช่น `20210202` ปั๊มบนการ์ด
- เลือก candidate **ยาวสุด**, เสมอกันเลือกตัวที่มี dash
- ข้าม `CBA…` asset barcode เสมอ

### `donate` — No. (เลขสติกเกอร์) + Serial เท่านั้น (org reader ลบแล้ว v0.9.2)
2 แบบ: **screen** (ถ่ายจอ — Notepad `NO.7` / cmd `wmic … SerialNumber`) + **chassis** (ถ่ายหลังเครื่อง —
Dell `SERVICE TAG(S/N):` + sticker "New PC Donate 677" หรือเลขเขียนมือ)
- **explicit No.** `extract_no_donate_explicit` (DonateMore): Notepad `NO.x` (`PC_NO_RE`) หรือ "Donate N"
  (เลขล้วนใกล้คำ "Donate", range-hint filter) — **priority สูงสุด**; fast-path: เจอแล้ว **ข้าม model + crop + Tesseract**
- **Serial** `extract_serial_donate`: Dell tag labeled / wmic `SerialNumber` anchor (ข้าม `DESKTOP-`, ผ่อน ≥1 alpha)
- **handwritten No.** (Photos-3-001): sticker model `read_sticker_number` (YOLOv8 onnx) + `fuse_sticker_no`
  กับ **crop_side** (subsequence rule) → ~78% — ดู [[Sticker-Digit-Model]]
- **donate ข้าม rotation** (ภาพตั้งตรง — หมุนไล่ conf จะทิ้งเลขสติกเกอร์)
- ❌ **org reader (Tesseract `tha`) ลบทั้งหมด** v0.9.2 (user: "เอาตัวอ่านภาษาไทยออกไปเลย" — ลายมือไทยอ่านมั่ว);
  ตัว anchor เลข (`_THAI_LETTER_RE` / `_trailing_sticker_no` / `locate_sticker_bbox`) **เก็บไว้** (หา *เลข* ไม่ output ไทย)
- *ponytail: DonateMore เลข typed → parser ~99.6% (model ไม่จำเป็น); Photos-3-001 เลขเขียนมือ → model+fusion เป็น lever*

## Rotation fallback — `ocr_with_rotation()`
ภาพถ่ายมือถือมักเอียง/กลับหัว → ลองหลายมุมแล้วเลือกผลดีสุด
1. OCR ที่ **0°** ก่อน — ถ้าได้ทั้ง PC No. **และ** serial → จบ ไม่ต้องหมุน
2. ถ้าขาดอย่างใดอย่างหนึ่ง → ลอง **90° / 270° / 180°** (cv2.rotate)
3. ให้คะแนนแต่ละมุมด้วย tuple `(has_serial, has_pc_no, mean_conf)` — **สูงกว่าชนะ**
4. early-exit ถ้ามุมไหนได้ครบทั้งสอง
- ผล: Serial บน batch ที่มีภาพหมุน **53% → 96%** (+43pp) — ดู [[Dev-History]]

## ข้อจำกัดที่รู้
- เลขเดี่ยวขนาดใหญ่บนกระดาษขาว (เช่น "1", "2") — PaddleOCR detector อาจ **skip** ทั้งกล่อง
- Accessory category แม่นต่ำสุด (No. ~66%) เพราะป้ายไม่มาตรฐาน — ดู [[Accuracy-Results]]

## ⚠️ กฎทอง
แก้ parser **ที่เดียวต้องแก้อีกที่**: `bulk_extract.py` ↔ `AssetExtractor.cpp` ต้อง mirror กันเสมอ
