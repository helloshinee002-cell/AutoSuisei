# Chat History — Session 2026-05-18

ประวัติการสนทนา + decisions ของเซสชัน 2026-05-18 (AutoSuisei v0.9.0 ship-ready)

> เซสชันนี้ต่อจาก `docs/tomorrow.md` ที่เขียนไว้ตอนจบ session 2026-05-17

## เป้าหมายเริ่มเซสชัน

จาก `docs/tomorrow.md`:
1. Rename AutoPilot → AutoSuisei (product name)
2. OCR tab restructure (3 category buttons)
3. Monitor + Accessory parsers
4. Test bulk extraction บน Train Monitor + Train Accessory
5. Rebuild bundle + installer

## ลำดับการตัดสินใจ (chronological)

### Phase 1: Rename + OCR tab restructure
- User: "ต่อ autosuisei" → ผมตอบ AutoPilot folder ไม่มี ขอ clarify
- User ส่ง `docs/tomorrow.md` → เริ่มจาก rename
- เก็บ `autopilot::` namespace, `autopilot_*` lib targets, env `AUTOPILOT_SCRIPTS_DIR`, folder `Documents\AutoPilot` ตามที่ user สั่งใน tomorrow.md
- เปลี่ยน: exe target, window title, installer, scripts, README, .clauderules

### Phase 2: Category-aware parser
- User clarify: **"Monitor ไม่ได้ใช้ Service Tag 7-char"** → ต้องใช้ S/N เต็ม `CN-...-A00` (บันทึก feedback memory)
- เขียน `extract_serial_monitor` / `extract_serial_accessory` ใน bulk_extract.py
- 8/8 unit smoke test pass

### Phase 3: Test + iterate
- Subset 20 ภาพ: Monitor 100%/100%, Accessory 90%/100% (PC No.)
- Full 754 Monitor: PC 95.6% / Serial 72.7% — Serial ต่ำเพราะภาพหมุน
- เพิ่ม `_merge_cn_fragments` + strip CBA barcode + strip 7+ digit noise → ช่วยขึ้นเล็กน้อย

### Phase 4: Rotation fallback (ตัวสำคัญที่สุด)
- User: "Train Monitor 2 เพิ่ม 300 รูปเพิ่มความแม่นยำ"
- ทดสอบ → 76% / 53% (มีภาพหมุนเยอะ)
- Debug: ~87% ของ Serial failures = OCR ไม่เจอ "CN-" เลย (ภาพหมุน)
- **เพิ่ม `ocr_with_rotation()`** — cv2.rotate 4 มุม + เลือกผลที่ดีที่สุด
- Train Monitor 2: 76% → **96.0% / 96.3%** (+20pp / +43pp!)
- Re-run Train Monitor (754): 95.6% → **98.8% / 94.7%**

### Phase 5: Watch tab fix
- User: "ฟังชั่น watching ไม่ทำงาน"
- Root cause: `ocr_worker.py` import `extract_serial` แต่หลัง refactor signature เป็น `(text, category)` → TypeError ทันทีบนภาพแรก
- Fix: ocr_worker.py รับ `--category` flag + ใช้ `ocr_with_rotation`

### Phase 6: GUI redesign (Claude Design template)
- User: "แก้ GUI ใช้ Templet จาก C:\Users\hello\Documents\Claude Design\claude-design"
- Plan mode → ถาม 2 อย่าง:
  - Native vs frameless titlebar? → **Native** (รักษา Windows Snap)
  - ADLaM Display font: bundle? → **bundle .qrc**
- สร้าง: `SidebarNav` widget (240px), `theme.qss`, `resources.qrc`
- แก้: MainWindow (QTabWidget → sidebar+QStackedWidget), main.cpp (font load + QSS apply)
- 3 tabs: header section, KPI cards (Watch), progress bar (Review), Batch/Date fields
- Fix: sidebar text truncation — ลบ `padding` ใน QSS `::item`

### Phase 7: PC No. → No. rename
- User: "แก้จาก PC No. เหลือแค่ No. เพื่อไม่ให้สับสน"
- แก้ 13 user-visible labels (table headers, status messages, tooltips, CLI output)
- ไม่แตะ internal field names (`pc_no` CSV/JSON, `pcNo` C++ var) — protocol stays stable

### Phase 8: # column + Stop button + Clear button
- User: "หน้า OCR ตรงนี้แก้ให้ขึ้นเป็นลำดับของภาพเลย 1,2,3,4"
- `#` column = `row + 1` แทน `photoIndex` (filename suffix sorted alphabetically)
- เพิ่ม Pause/Stop ใน OCR — implement Win32 `NtSuspendProcess` แล้ว Toolhelp32 SuspendThread
- User: "กด pause แล้วไม่หยุด" → switch จาก ntdll → Toolhelp32 + diagnostic
- User: "เอา Pause ออก เหลือแค่ Stop" → ลบ Pause ทั้งหมด, simplify
- เพิ่ม Clear button ใน Review tab

### Phase 9: User Guide PDF
- User: "ทำวิธีใช้เป็น PDF รูปภาพที่ใช้เป็นตัวอย่างใช้ 3 รูปนี้"
- ใช้ fpdf2 + Tahoma (Thai support) — 6 หน้า A4
- Cover / Overview / 3 tabs sections / FAQ
- User: "ผลที่ทดสอบ เหมือนจะลืมของ PC ไปนะ"
- ทดสอบ Train2 (632 PC&Laptop) ด้วย parser ใหม่ → No. 98.3% / Serial 85.1%
- อัปเดต PDF

### Phase 10: GitHub push (held)
- User: "อัพขึ้น github C:\Users\hello\Downloads\Github Push.txt"
- Auth ล้มเหลว — bash session non-interactive ไม่ทำ device code flow ได้
- ลอง PAT — user เลือก "Hold ไว้ก่อน ทำคู่มือก่อน"
- Local commit ครบ: `e3b8c58 feat: v0.9.0 — rebrand AutoSuisei + GUI redesign + parser upgrades`
- Remote ตั้งค่าแล้ว: `origin = https://github.com/helloshinee002-cell/AutoSuisei.git`
- รอ user push เอง หรือให้ PAT

## Deliverables (Session 2026-05-18)

| File | Status |
|------|--------|
| `installer/AutoSuisei.iss/.ico` | renamed from AutoPilot.* |
| `src/gui/SidebarNav.{h,cpp}` | new |
| `src/gui/theme.qss` | new (350 lines) |
| `src/gui/resources.qrc` + fonts/ADLaMDisplay-Regular.ttf | new |
| `src/gui/{Main,Ocr,Watch,Review}Tab.*` | restructured for sidebar + Claude Design |
| `src/gui/main.cpp` | + font load + Thai fallback + QSS apply |
| `scripts/bulk_extract.py` | category dispatcher + `ocr_with_rotation` + CBA strip + line-merge |
| `scripts/ocr_worker.py` | `--category` flag + ocr_with_rotation |
| `scripts/make_user_guide_pdf.py` | new — fpdf2 + Tahoma |
| `docs/AutoSuisei_User_Guide.pdf` | new (6 pages, 538 KB) |
| `docs/screenshots/{ocr_single,folder_watch,review_rename}.png` | new |
| `build/windows-x64-release/.../AutoSuisei.exe` | rebuilt (776 KB) |
| `Backups/AutoSuisei/AutoSuisei-Setup-0.9.0.exe` | installer (108 MB) |

## งานค้างถัดไป

1. **Push GitHub** — รอ user PAT หรือทำเองที่เครื่อง
2. **WatchTab category selector** — ปัจจุบัน default `pc`; ถ้า watch โฟลเดอร์ Monitor/Accessory ต้อง UI selector
3. **Accessory parser improvement** — No. 66.3% ค่อนข้างต่ำ (handwriting บนกล่อง/สลาก) อาจต้องเพิ่ม image preprocessing

## Key file paths (สำหรับ session ถัดไป)

- Code: `C:\Users\hello\Documents\AutoPilot\src\gui\`
- Scripts: `C:\Users\hello\Documents\AutoPilot\scripts\`
- Build: `C:\Users\hello\Documents\AutoPilot\build\windows-x64-release\src\gui\AutoSuisei.exe`
- Backups: `C:\Users\hello\Backups\AutoSuisei\`
- Test data: `C:\Users\hello\Downloads\{Train,Train2,Train3,Train Monitor,Train Monitor 2,Train Accessory}\`
