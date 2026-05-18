# Tomorrow's Task List — 2026-05-18

User instructions (จาก session 2026-05-17):

## 1. Rename: AutoPilot → AutoSuisei

ที่ต้องแก้:
- `installer/AutoPilot.iss` → `installer/AutoSuisei.iss` (หรือเปลี่ยน `#define MyAppName` + `OutputBaseFilename`)
- `src/gui/CMakeLists.txt` → `qt_add_executable(AutoSuisei ...)` (target name)
- `src/gui/MainWindow.cpp` → `setWindowTitle("AutoSuisei")`
- `src/gui/AutoPilot.rc` → `src/gui/AutoSuisei.rc`
- `scripts/make_bundle.ps1` → ทุกที่ที่อ้าง AutoPilot.exe
- `scripts/make_installer.ps1` → ทุกที่ที่อ้าง AutoPilot-Setup
- `CLAUDE.md` → header section
- Memory `project_autopilot.md` → คง name เดิม (ประวัติ) แต่ note ว่า rebrand

**Decision**: เก็บโฟลเดอร์ `Documents\AutoPilot` ไว้เหมือนเดิม (เปลี่ยน workspace path = pain) — แค่ rename exe + product name

## 2. OCR tab menu restructure

**Buttons ตอนนี้** (จาก screenshot):
```
[ Browse files… ] [ Bulk Extract Folder (PC No. / Serial) ] [ Export JSON… ] [ Send to Review → ] [ Clear ]
```

**Buttons ที่ต้องการ**:
```
[ PC&Laptop ] [ Monitor ] [ Accessory ] [ Send to Review → ] [ Clear ]
```

การเปลี่ยนแปลง:
- เอา **Browse files…** ออก
- เปลี่ยนชื่อ **Bulk Extract Folder (PC No. / Serial)** → **PC&Laptop**
- เอา **Export JSON…** ออก
- เพิ่ม **Monitor** → bulk extract บน `C:\Users\hello\Downloads\Train Monitor`
- เพิ่ม **Accessory** → bulk extract บน `C:\Users\hello\Downloads\Train Accessory`

**Implementation note**: Monitor + Accessory ใช้ flow เดียวกับ PC&Laptop (bulk_extract.py) แต่อาจต้องการ extraction logic ที่ต่างกัน — ต้องดูภาพตัวอย่างก่อนตัดสินใจว่า:
- (a) ใช้ regex เดิมพอ (มี "no.NN" / serial เหมือนกัน)
- (b) เพิ่ม parser ใหม่สำหรับ Monitor (model/size?) + Accessory (มี item name? serial?)

ก่อนเขียนโค้ดจริง: เปิดภาพ 3-5 ใบจากแต่ละโฟลเดอร์ดูก่อน

## 3. Training data paths (รอ user ใส่ภาพ)

- **Monitor**: `C:\Users\hello\Downloads\Train Monitor` ← ภาพ Train ของ Monitor
- **Accessory**: `C:\Users\hello\Downloads\Train Accessory` ← ภาพ Train ของ Accessory

User จะใส่ภาพไว้ก่อนเริ่ม session

## 4. Serial extraction รายหมวด — สำคัญ

**สถานะปัจจุบัน** (PC&Laptop): regex หา `SERVICE TAG (S/N) XXXXXXX` หรือ `S/N XXXXXXX`
ลงท้ายด้วย 7-char alphanumeric (Dell pattern + ≥3 alpha + ≥2 digit)

**Monitor — ใช้ S/N เท่านั้น (ห้ามใช้ Service Tag 7-char)**
- ภาพ Dell monitor มี **ทั้ง** "Service Tag: XXXXXXX" (7-char) และ "S/N: CN-XXXXX-XXXXX-XXX-XXXX-A00" (Dell CN format ยาว)
- **ต้องใช้ S/N ตัวเต็ม** เช่น `CN-07C2R4-72872-2BD-A8MM`, `CN-0JF27G-FCC00-76M-AKNB-A00`
- **ห้าม** fall back ไปใช้ Service Tag 7-char (ผู้ใช้ระบุชัดเจน 2026-05-18)
- Parser ปัจจุบันรองรับ 7-char Dell Service Tag เป็น primary → ต้องเขียน
  `parseMonitorSerial` แยก: หา pattern `CN-[A-Z0-9]+-[A-Z0-9]+-...-A00`
  หลัง label "S/N" (ตัวที่ 2 ใน 2 ตัว — Service Tag มาก่อน S/N เสมอ)
- Dell-specific blocklist (PASS1OF/DISK0C1/...) ไม่จำเป็นในตัว Monitor parser
  เพราะ CN- pattern เฉพาะเจาะจงพอ

**Accessory — 2 รูปแบบ**:
- **(a) มี "S/N"** → parser คล้าย Monitor
- **(b) แค่ barcode (ไม่มี S/N text)** → ต้องอ่าน **ตัวเลขใต้บาโค้ด**
  - บาโค้ดมาตรฐาน (Code128/EAN/UPC) ใต้แท่งจะมีตัวเลข human-readable
  - PaddleOCR น่าจะเห็นตัวเลขนี้ได้ปกติ (ตัวพิมพ์ใหญ่ไม่มีอะไรพิเศษ)
  - **เขียน fallback**: ถ้าไม่เจอ "S/N" → หาบรรทัดที่เป็นเลขล้วน 8-15 หลัก
    (บาโค้ด consumer มักเป็น 8/12/13 หลัก; industrial บางตัว 14+)
  - ระวัง false positive จาก timestamp ในภาพ EXIF / IMEI / etc.
  - ดี-ไซน์ idea: collect ทุก `^\s*\d{8,15}\s*$` line → คืนตัวที่ยาวที่สุด
    หรือมี confidence สูงสุด

**Architecture suggestion**: เพิ่ม `enum Category { PCLaptop, Monitor, Accessory }` →
parser per category → `AssetExtractor::extractFor(Category, image)` แทน 1 parser ทั่วไป
- แต่ละ category เรียก PaddleOCR ตัวเดียวกัน แต่ post-process regex ต่างกัน
- Python sidecar (`bulk_extract.py`) เพิ่ม `--category=monitor|accessory|pc` flag

---

## Suggested order (tomorrow)

1. **เช็คภาพตัวอย่าง** ใน Train Monitor + Train Accessory — ดูว่ามีอะไรให้ extract บ้าง
2. **Rename** AutoPilot → AutoSuisei (ตั้งใจให้เสร็จก่อน — บน UI / windows title / installer)
3. **OCR tab restructure** — เอา 2 ปุ่มออก + เพิ่ม 2 ปุ่ม + rename Bulk Extract
4. **ทดสอบ extraction บน Monitor / Accessory** ด้วย parser ปัจจุบัน — ดู accuracy
5. **เพิ่ม category-specific extraction** ถ้า parser เดิมไม่พอ
6. **Rebuild bundle + installer** ทั้งหมดด้วย product name ใหม่
