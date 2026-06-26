---
date: 2026-06-26 17:56
idea: "[[ideas/barcode-qr-serial]]"
tags: [log, autosuisei, ocr, barcode]
---

# Barcode-first Serial — Data Matrix (PC) + Code128 (desktop/donate)

- **Date/Time:** 2026-06-26 17:56 (วันศุกร์)
- **Idea Ref:** [[ideas/barcode-qr-serial]]
- **Changes Made:**
  - **v0.9.6** — อ่าน Dell **Data Matrix** ด้วย `pylibdmtx` ก่อน OCR (scope = `pc`). เพิ่ม `read_serial_barcode`,
    `_decode_datamatrix`, `_white_sticker_crop` ใน `scripts/bulk_extract.py`; thread `serial_source` (barcode/ocr)
    ผ่าน JSON → `AssetInfo.serialSource` → Review **คอลัมน์ "Src"** + persist ลง `ground_truth.csv`.
    ไฟล์ C++: `src/ocr/AssetExtractor.h`, `src/ocr/ReviewModel.{h,cpp}`, `src/gui/{OcrTab,WatchTab,ReviewTab}.cpp`.
    + ขยาย Review image pane (splitter 2:3). +2 GTest (serial_source round-trip).
  - **v0.9.7** — เพิ่ม `pyzbar`/ZBar อ่าน **Code128** (desktop/laptop service tag) + ขยาย barcode-first ไป `donate`.
    เพิ่ม `_decode_zbar` (จำกัด symbols = Code128/Code39/QR ตัด PDF417 spam) + helper `_pick_serial`;
    เรียง decoder **lazy: ZBar → Data Matrix → cv2** (ZBar เร็ว 0.03s เจอก่อนข้าม Data Matrix 2s).
    ไฟล์: `scripts/bulk_extract.py`, `scripts/ocr_worker.py`. bundle `pylibdmtx`+`pyzbar` (+ DLL).
  - dataset วัด: `Downloads/Train2` (PC), `Downloads/DonateMore/PC` (desktop+laptop donate)
- **Technical Notes:**
  - **OCR misread = ปัญหาจริง**: ป้าย `6X0F453` แต่ OCR/รีวิวเดิมอ่าน `6XOF463`; donate `4N0TZL2`→`4NOTZL2`.
    Dell tag **ไม่เคยมีตัว `O`/`I`** (มีแต่ `0`/`1`) → barcode ถูกเสมอเมื่อต่างจาก OCR (วัด **8/8 disagreements barcode ถูก**).
  - **ชนิดโค้ดต่างกันตามอุปกรณ์**: laptop/monitor = **Data Matrix** (ต้อง `pylibdmtx`, cv2 อ่านไม่ได้);
    desktop = **Code128** (ZBar). cv2.barcode + QRCodeDetector อ่าน 2 อย่างนี้**ไม่ได้** → ต้อง 2 lib.
  - **ต้องกรอง CBA bank-asset tag** (`CBA1000…`) — เป็นโค้ดที่อ่านง่ายสุดบนป้ายแต่**ไม่ใช่** serial เครื่อง;
    `_valid_dell_tag` (7-char, ≥1 alpha+≥1 digit) ปัด IMEI/Express-code ออกด้วย.
  - **ZBar-first ปลอดภัยกับ laptop**: ZBar คืน CBA/QR-string ที่ไม่ผ่าน `_valid_dell_tag` → fall through ไป
    Data Matrix (พิสูจน์บน Train2). PDF417 ของ libzbar assert spam บน noise → จำกัด `symbols=` ตัดทิ้ง.
  - **distributable**: `pylibdmtx`(libdmtx-64.dll) + `pyzbar`(libzbar-64.dll+libiconv.dll) ฝังใน bundle, offline.
  - **ระวังอนาคต**: donate ที่ ZBar อ่านไม่ออกจะตก Data Matrix timeout ~2s/รูป (batch ใหญ่ช้าขึ้น) — ปรับ/ตัดได้;
    accessory ตัดออก (S/N พิมพ์/สลัก ไม่อยู่ในโค้ด → วัด 0/40). measurement gate ต้องมีเสมอกับ Code39/128 (ไม่มี ECC แรงเท่า Data Matrix).
  - งานนี้ build เป็น 0.9.7 แล้วแต่ **ยังไม่ commit/push** (เป็นส่วนที่ 2 ของงาน — รวมถึง bump 1.0.0).

## เกี่ยวข้อง
[[OCR-and-Parser]] §Barcode-first · [[Accuracy-Results]] · [[Build-and-Distribution]] · [[changelog]]
