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

---

## Suggested order (tomorrow)

1. **เช็คภาพตัวอย่าง** ใน Train Monitor + Train Accessory — ดูว่ามีอะไรให้ extract บ้าง
2. **Rename** AutoPilot → AutoSuisei (ตั้งใจให้เสร็จก่อน — บน UI / windows title / installer)
3. **OCR tab restructure** — เอา 2 ปุ่มออก + เพิ่ม 2 ปุ่ม + rename Bulk Extract
4. **ทดสอบ extraction บน Monitor / Accessory** ด้วย parser ปัจจุบัน — ดู accuracy
5. **เพิ่ม category-specific extraction** ถ้า parser เดิมไม่พอ
6. **Rebuild bundle + installer** ทั้งหมดด้วย product name ใหม่
