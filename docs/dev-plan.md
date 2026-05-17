# Development Plan — AutoPilot

> ไฟล์นี้ตามที่ระบุใน CLAUDE.md ทุก feature ต้องอัปเดตที่นี่ก่อนเขียนโค้ดจริงและรอ approval

## Status: Phase 0 — Scaffold (เสร็จ)
- [x] โครงสร้างโฟลเดอร์
- [x] CMake + vcpkg manifest + preset
- [x] CLAUDE.md + .clauderules + MCP config + GitHub workflow
- [x] Module headers (core/recorder/player/ocr/web/scripting/storage/gui/cli)
- [x] OCR engine implementation (Tesseract + OpenCV)
- [x] GTest skeleton + OCR TDD seed

---

## Phase 1 — OCR MVP (in_progress, started 2026-05-16)
**เป้าหมาย**: รัน `autopilot_cli ocr image.png` แล้วได้ JSON `{filename, text, digits, confidence, language}` ที่ valid

### Sub-tasks
- [x] โครง `OcrEngine` + 3 test เดิม (จาก Phase 0)
- [ ] **1.2** เพิ่ม failing test: unicode filename (ภาษาไทย) + JSON output schema
- [ ] **1.3** Fix unicode path support — ใช้ `std::ifstream` + `cv::imdecode` แทน `cv::imread` ตรง (cv::imread บน Windows ไม่ handle UTF-8 path)
- [ ] **1.4** Extract `OcrFormatter::toJson(OcrResult)` ใช้ `nlohmann::json` — ห้าม hand-roll JSON ใน CLI อีก
- [ ] **1.5** Unit test `OcrFormatter` (pure function ไม่ต้อง spawn process)
- [ ] User: เพิ่ม `tests/data/number_20.png` + traineddata + `vcpkg install`

### Acceptance Criteria (measurable)
1. `OcrEngine::recognize("ไทย.png")` ไม่ throw — เปิดไฟล์ unicode path ได้
2. `OcrFormatter::toJson(result)` คืน JSON ที่:
   - `nlohmann::json::parse()` ได้สำเร็จ
   - มี keys: `filename, text, digits, confidence, language`
   - escape special chars (`"`, `\n`, `\`) ได้ถูกต้อง
3. `ctest` ผ่าน 100% (3 เดิม + 4 ใหม่)
4. Confidence ≥ 60 บน `number_20.png` ที่ render ด้วย ImageMagick

### Risks
- `cv::imdecode` รับ buffer ขนาดใหญ่ → ใส่ guard 50MB max
- Tesseract page seg mode 6 อาจไม่เหมาะกับเลขเดี่ยว → ใช้ PSM 8 (single word) เป็น fallback ถ้า digit-only mode

### Decision
- ยังไม่ทำ CLI subprocess test ใน phase นี้ (brittle) — ทำเฉพาะ formatter unit test แทน
- Watch-folder + batch mode เลื่อนไป Phase 6

---

## Phase 2 — Macro Recorder (in_progress, started 2026-05-16)
**เป้าหมาย**: record keyboard+mouse, save → SQLite, replay reproduces input

### Sub-tasks
- [x] **2.1** Plan update
- [ ] **2.2** `SpscRingBuffer<T,Cap>` — single-producer/single-consumer ring buffer (hook→worker)
- [ ] **2.3** `WindowsRecorder : IRecorder` — install hooks บน internal "hook thread" ที่มี message loop + worker thread แปลง raw → `core::Action` → callback
- [ ] **2.4** `WindowsPlayer : IPlayer` — `SendInput` + relative-timing scheduler + speedMultiplier + cooperative stop
- [ ] **2.5** `MacroSerializer` — pure `Action ↔ JSON` ใช้ `std::visit` กับ `std::variant`
- [ ] **2.6** `SqliteMacroRepository : IMacroRepository` — schema with WAL + FK; serialized actions in TEXT column
- [ ] **2.7** GTest: serializer round-trip + repository CRUD (in-memory DB)
- [ ] **2.8** CMake wire-up + ensure compose with Phase 1 build
- [ ] **2.9 (deferred)** GUI buttons — wait for Phase 1 GUI scaffold to validate

### Acceptance
1. `WindowsRecorder` instantiate + start + stop ไม่ throw, ไม่มี handle leak
2. `MacroSerializer::fromJson(toJson(m))` คืนค่าเทียบเท่า `m` (deep equality)
3. `SqliteMacroRepository::save → findById → findAll → remove` ทำงานบน `":memory:"` DB
4. ทุก hook callback ต้อง **return ภายใน 100µs** (เป้า) — enqueue เป็น POD ห้าม allocate

### Risks + Mitigation
- **LL hooks ถูก disable เมื่อ proc ใช้เกิน timeout** (LowLevelHooksTimeout ใน registry, default 300ms): mitigate ด้วย enqueue ทันที + worker thread ทำ heavy work
- **UAC**: hooks captures event จาก process elevated > เราต้องการ — ระบุใน README
- **DPI scaling**: เก็บ `GetDpiForWindow(GetForegroundWindow())` ตอน record; playback แปลงพิกัดตาม target window DPI
- **C-style callback ต้อง static** — ใช้ static instance pointer; protect ด้วย atomic + assert single recorder global

### Decisions
- ใช้ SPSC ring buffer ของเราเอง (header-only ~50 บรรทัด) แทน `concurrentqueue` lib — ลด dep, hook เป็น SPSC อยู่แล้ว
- เก็บ actions เป็น JSON string ใน 1 column (denormalized) แทน normalized `actions` table — Macro เป็น unit of update อยู่แล้ว
- GUI button defer ไป Phase 2.9 — verify ส่วนหลังบ้านก่อน เพื่อ unblock Phase 3 (web)

---

## Phase 3 — Web Recorder/Player (CDP) (in_progress, started 2026-05-16)
**Scope ปรับใหม่**: Replay-first สำหรับ MVP — Browser-side recording เลื่อนไป Phase 3.5
  - **เหตุผล**: CDP ไม่มี "Input recorded" event ในตัว ต้อง inject JS via `Page.addScriptToEvaluateOnNewDocument` + `Runtime.bindingCalled` ซึ่งซับซ้อนกว่าควรทำใน MVP edge

### Sub-tasks
- [x] **3.1** Plan
- [ ] **3.2** `CdpMessage` — pure serdes ของ Request/Response/Event (testable ไม่มี IO)
- [ ] **3.3** `CdpClient : ICdpClient` — libwebsockets transport: service thread + pending map + event broadcast
- [ ] **3.4** `WebPlayer` — แปลง `core::Action` → CDP `Input.dispatch*` คำสั่ง
- [ ] **3.5** GTest: round-trip CdpMessage + id assignment + bad payload
- [ ] **3.6** CMake wire + verify ไม่ break Phase 1/2 build

### Acceptance
1. `CdpMessage::serialize(Request)` คืน JSON ที่มี `id` auto-increment + `method` + `params`
2. `CdpMessage::parse(jsonStr)` แยก Response (มี `id`) ออกจาก Event (ไม่มี `id`) ได้
3. `CdpClient::send("Page.navigate", {{"url":"..."}})` คืน `std::future<json>` ที่ resolve เมื่อ response กลับ
4. `WebPlayer::play(macro)` ส่ง CDP command ทุก action โดยไม่ block service thread

### Deferred (Phase 3.5)
- Browser-side recording: inject JS listener + `Runtime.bindingCalled` → ส่ง keystroke event กลับ
- Endpoint discovery via HTTP `/json/version` (ตอนนี้ user ระบุ ws:// URL เอง)
- Chrome process launcher (`--remote-debugging-port=9222 --user-data-dir=...`)

### Risks
- libwebsockets เป็น C API ค่อนข้าง low-level — bug ใน threading model อาจ deadlock; mitigate ด้วย service thread แยก + non-blocking outgoing queue
- Chrome version ใหม่อาจเปลี่ยน CDP — pin CDP commands ที่เป็น stable เท่านั้น (Input domain)

---

## Phase 4 — Scripting + Conditional/Loop
- Lua sandbox (sol2) + whitelist API
- Action kinds: Conditional, Loop, LuaScript

---

## Phase 5 — Image-based clicking (สำคัญสำหรับ web reliability)
- OpenCV template matching
- Find image on screen → คลิกตำแหน่ง center
- ใช้เป็น fallback ตอน DOM selector เปลี่ยน

---

## Phase 6 — Scheduler + Watch folder + Encrypted credentials
- Cron-like scheduler (cron-style string parser)
- File watcher trigger (`ReadDirectoryChangesW`)
- DPAPI encrypted credential store

---

## Phase 7 — Plugin system + Polish
- DLL plugin loader
- Multi-monitor enumeration
- Replay speed control + breakpoints
- Export/import JSON

---

## Phase 9.1 — AssetExtractor false-positive cleanup (in_progress, started 2026-05-17)

**Context**: หลังจาก Phase 9 PaddleOCR baseline บน 232 ภาพแรก พบ false positive 2 ประเภทที่ทำให้ pc_inventory เปื้อน:
- `PASS1OF` — มาจาก killdisk "One Pass 1 of 1" UI text ที่ผ่าน standalone-alphanum-7 fallback
- `DISK0C1` — มาจาก disk label "Disk 0 C1" ที่ผ่าน fallback เช่นกัน

**OK folder ground truth** (`Downloads\OK` 18 ภาพ) เปิดดูแล้วพบ category extraction 4 แบบ:
1. Notepad ดำ + lone digits (22, 28, 54) — ปัจจุบัน parser **ไม่จับ** เพราะไม่มี "no" prefix
2. Notepad ขาว + "pc no.NN" — จับได้
3. Notepad + "pc no.NN" + "SN:XXXXXXX" + status — จับ pc+serial ได้
4. Sticker / handwriting บนเครื่อง — Dell tag visible (e.g. 7VSP9M2, 6JXXFL2)

**Scope Phase 9.1** (focused, ~30 นาที):
- [ ] **9.1.1** เขียน failing test: `parseSerialFromText("Erasing One Pass 1 of 1 PhysicalDrive0") == ""` (blocklist PASS1OF)
- [ ] **9.1.2** เขียน failing test: `parseSerialFromText("Disk 0 C1 boot...") == ""` (blocklist DISK0C1)
- [ ] **9.1.3** เขียน failing test: Dell tag fallback ต้อง require min 2 digits + min 3 alphas → reject "PASSWORD" (7 char all alpha), reject "ABCDEF1" (1 digit)
- [ ] **9.1.4** Implement: เพิ่ม blocklist set + tighten alphanum threshold ใน fallback
- [ ] **9.1.5** Verify: 60/60 ctest pass + เพิ่ม 3 test ใหม่ → 63/63

**Acceptance**:
1. Tests ที่มีอยู่ทั้งหมดยังผ่าน (no regression)
2. False positive blocklist ทดสอบ explicit
3. Re-run `scripts\bulk_extract.py` บน 232 ภาพเดิม → PASS1OF/DISK0C1 หายไป

**Out of scope** (จะแยก phase):
- "Lone digit" PC No. extraction (Notepad dark case 22/28/54) — Phase 9.2
- Manual review UI — Phase 9.3
- Training pipeline บน Train2 — Phase 10

---

## Phase 9.2 — Lone-digit PC No. fallback (in_progress, started 2026-05-17)

**Context**: หลัง Phase 9.1 false-positive fix รัน PaddleOCR sidecar บน 632 ภาพ `Downloads\Train2` พบ:
- PC No. hit เพียง 49.8% (315/632) — ครึ่งหนึ่งของภาพเป็น sticker photo มีเลขใหญ่ไม่มี "no." prefix
- Sample miss: `Train2_10.jpg` มี sticker "315" + Dell tag "6YW8RV2" → extractor จับ serial ได้แต่พลาด pc

**Ground truth จาก `Downloads\OK\`** (18 ภาพที่ user label) ระบุ pattern 4 แบบ ที่ "lone-digit" คือเลข 2-3 หลักโดดๆ:
- Sticker number (45, 48, 49, 93, 103)
- Dark Notepad fullscreen "22"/"28"/"54"
- Handwriting green marker บนเทป (36)

**Implementation**:
- [x] **9.2.1** เพิ่ม 8 failing test ใน `test_asset_extractor.cpp`:
  - ParsesStandaloneDigitFromStickerOcr (315 ใน multi-line text)
  - ParsesStandaloneDigitFromDarkNotepad (22, 28, 54)
  - PrimaryPatternStillWinsOverFallback (กัน regression)
  - RejectsFourDigitLineAsLoneDigit (5290 model, 2026 year)
  - RejectsKilldiskProgressPercent ("52% complete")
  - RejectsHexOffsetAsLoneDigit ("0x00000000")
  - RejectsZeroAndSingleDigitArtifacts
  - AcceptsTwoDigitFromOkGroundTruth
- [x] **9.2.2** Implement `parsePcNoFromText` fallback ที่ split text เป็นบรรทัด แล้ว match `^\s*([0-9]{2,3})\s*$` (ทั้งบรรทัดเป็น 2-3 digit)
- [x] **9.2.3** Mirror Python regex ใน `scripts/bulk_extract.py`
- [x] **9.2.4** Verify ctest 73/73 pass (was 65/65)
- [x] **9.2.5** Validate บน OK ground truth: 17/18 (94.4%) PC No. hit, miss แค่ 36.png (handwriting จาง)
- [ ] **9.2.6** Re-run Train2 บน v2 → expected PC No. hit 80%+

**Acceptance**:
1. Primary "no.NN" pattern ยังทำงานเป็น regression test ✓
2. False positive guards: 4-digit lines, %/x/letters → rejected ✓
3. OK ground truth: PC No. hit ≥ 90% (achieved 94.4%) ✓
4. Train2 PC No. hit ≥ 80% (target — pending v2 run)

**Train2 v2 result (post-9.2)**: 619/632 (97.9%) PC No. hit — เพิ่มจาก 49.8% baseline เป็น 97.9% (+48 pp) — เกิน target อย่างชัดเจน

---

## Phase 9.3 — Manual Review UI (in_progress, started 2026-05-17)

**Context**: หลัง Phase 9.2 ยังเหลือ ~13 false positives ใน Train2 (PC #7/10/14/023/025/32/62/291) + miss 13 ภาพที่ไม่ได้ค่า ต้องการ tool ให้ user คลิก review + correct เพื่อ build ground truth dataset สำหรับ Phase 10 (ML training)

**Architecture decision**: แยก data layer (`autopilot_ocr::ReviewModel` — pure C++, testable via GTest) ออกจาก Qt UI (`autopilot::gui::ReviewTab`) ตาม CLAUDE.md module boundary

### Sub-tasks
- [x] **9.3.1** Design + write 19 failing test ใน `tests/ocr/test_review_model.cpp` ครอบคลุม:
  - CSV escape/parse (special chars, quoted fields, embedded commas/quotes)
  - Load จาก bulk_extract.py format (filename/pc_no/serial_no minimum)
  - Edit row preserves `originalPcNo`/`originalSerialNo`
  - Save → reload preserves all fields
  - `nextUnverified` skipping + return nullopt when all done
  - Resume from previously-saved CSV with verified+notes columns
- [x] **9.3.2** Implement `ReviewModel` (header `ReviewModel.h`, impl `ReviewModel.cpp`) — pure C++
- [x] **9.3.3** Wire `autopilot_ocr` lib + tests/CMakeLists.txt (19/19 ReviewModel tests pass)
- [x] **9.3.4** Implement `ReviewTab` Qt widget — left QTableWidget + right preview/edit form + Save button
- [x] **9.3.5** เพิ่ม "Review" เป็น tab ที่ 5 ใน MainWindow
- [x] **9.3.6** Build AutoPilot.exe + ctest 92/92 pass
- [ ] **9.3.7** Manual smoke test (user) — เปิด exe + load `build/train2_paddle_v2.csv` + folder `Downloads\Train2` + review + save → ตรวจ ground_truth.csv ออกถูก format

### Acceptance
1. ReviewModel ผ่าน 19 unit tests ✓
2. ไม่ regression — ctest 92/92 (was 73/73 + 19 new) ✓
3. AutoPilot.exe build สำเร็จ + มี tab Review โผล่ขึ้นมา (need user smoke test)
4. CSV roundtrip: load → edit → save → reload ค่าเดิม + verified state ติดมา ✓ (covered by SaveAndReload_PreservesAllFields)

---

## Phase 9.4 — Serial deduplication via majority voting (in_progress, started 2026-05-17)

**Context**: User verified 622/632 (98.4%) of Train2 ผ่าน Phase 9.3 Review UI → `build/ground_truth.csv` แต่ serial column ยังไม่ได้ touch เพราะ user ปล่อย OCR reading ไว้ ทำให้ PC ที่ถ่ายหลายภาพมี serial หลายแบบจาก OCR misread:
- PC 303: `21GG1F3 / C7WS9M2 / C7WS9N2` — M/N confusion ในรูป
- PC 308: `614ZFL2 / 6L4ZFL2 / COL7210 / GC624F3` — 1/L confusion + false positives

**Algorithm**: edit-distance ≤1 clustering + count-weighted vote
1. กลุ่ม serial readings ของ PC เดียวกันที่ต่างกัน ≤1 ตัวอักษร (same length, single-char substitution)
2. Score per cluster = (count desc, total_confidence desc)
3. Winning cluster → canonical = highest confidence reading inside
4. Rejected serials เก็บใน column แยกเพื่อ audit

### Sub-tasks
- [x] **9.4.1** Implement `scripts/dedupe_serials.py` + inline `--self-test` (PC 303 case + edge cases)
- [x] **9.4.2** Run บน `build/ground_truth.csv` → `build/pc_inventory_final.csv`
- [x] **9.4.3** Validate: PC 303 → C7WS9N2 (correct cluster), PC 308 → 6L4ZFL2 (correct cluster), false positives `21GG1F3`, `COL7210`, `GC624F3` rejected
- [x] **9.4.4** User correction stats จาก ground_truth.csv: 15/622 (2.4%) PC# corrected, 0 serial corrected → auto extraction มี real accuracy ~97.6%

### Result
- Total unique PCs: 205 (จาก 212 ก่อน dedup เพราะ user's PC# corrections รวบ PC ซ้ำ)
- 51.2% (105/205) ของ PC มี multiple serial variants ที่ต้อง dedup
- 189/205 PCs (92%) มี canonical serial
- `build/pc_inventory_final.csv` พร้อมใช้ตอนนี้

---

## Phase 9.5 — Filename-range-guided PC No. (in_progress, started 2026-05-17)

**Priority pivot**: user feedback "Serial ไม่ต้องเยอะก็ได้ เน้นที่ เลข No." → กลับมา focus PC No. accuracy ไม่ใช่ serial polish

**Context จาก ground_truth.csv (Train2 user-verified, 622/632)**:
- 8 photos ที่ OCR ได้เลขผิด — patterns:
  - `_5.jpg`: OCR=7, user=317 (lone-digit ฟลุก "7" จาก screen)
  - `_136.jpg`: OCR=025, user=347
  - `_55.jpg`: OCR=62, user=336
  - `_130.jpg`: OCR=32, user=423
  - `_153.jpg`: OCR=291, user=479
  - `_198.jpg`: OCR=10, user=460
  - `_74.jpg`: OCR=023, user=448
  - `_189.jpg`: OCR=14, user=338
- ทุกเคส OCR หยิบ 2-3 digit จาก screen artifacts ไม่ใช่ PC No. จริง
- filename ทั้งหมดมี hint "Laptop 301-400" หรือ "Laptop 401-500" → ใช้กรองได้

**Implementation**:
- [x] **9.5.1** เพิ่ม overload `parsePcNoFromText(text, rangeHint)` + helper `parsePcRangeBounds`
- [x] **9.5.2** Strategy: primary "no.NN" regex ยังชนะเสมอ; lone-digit fallback เลือก in-range ก่อน; ถ้าไม่มีตัวใน range → fall back to first (ไม่ regression)
- [x] **9.5.3** ขยาย `parsePcRangeFromFilename` รับ "Laptop NNN-NNN" (Train2) นอกจาก "pc NNN-NNN" (batch แรก)
- [x] **9.5.4** Update `extract()` ส่ง `info.pcRange` เข้า `parsePcNoFromText`
- [x] **9.5.5** Mirror logic ใน `scripts/bulk_extract.py`
- [x] **9.5.6** 9 failing tests → pass (101/101 ctest)
- [ ] **9.5.7** Re-run Train2 v3 → expect 15 corrections → ≤5 misses

**Acceptance**: PC No. mismatch rate < 1% on Train2 (was 2.4% in v2)

### v4 result (iterate primary matches)
- v2 baseline: 617/632 (97.6%) — wrong=8, missed=7
- v3 (range filter on lone-digit only): 619/632 (97.9%) — wrong=6, missed=7
- v4 (range filter on **both** primary & lone-digit): **622/632 (98.4%)** — wrong=3, missed=7

### Remaining failures (10/632 = 1.6%) — at regex limit
1. **User intent** (1): _101.jpg has both "303" sticker and "023" tape — user picked 023; OCR can't disambiguate
2. **OCR true miss** (2): _130.jpg, _74.jpg — PaddleOCR didn't return the truth value in OCR text at all (truth=423/448 absent)
3. **OCR didn't find any 2-3 digit** (7): missed photos — would need preprocessing (rotation/contrast) or ROI detection

Phase 9.5 done. Future Phase 10 (image preprocessing) or Phase 11 (fine-tune detection) could push the missed-7 group, but ROI of further OCR investment is low past 98.4%.

### Updated commit
- `4ce1f25` initial Phase 9.5 (lone-digit only)
- `__pending__` iterate primary matches + comparison script

---

## Decisions Log
| Date | Decision | Reason |
|---|---|---|
| 2026-05-16 | Windows-only | ลดความซับซ้อน ใช้ WinAPI/UI Automation ได้เต็มที่ |
| 2026-05-16 | Qt 6 (ไม่ใช่ ImGui) | ต้องการ macro editor ที่มี timeline view + accessibility |
| 2026-05-16 | Tesseract local (ไม่ใช่ cloud) | offline-first ไม่มี recurring cost |
| 2026-05-16 | CDP แทน CEF | binary size เล็ก ไม่ต้อง embed Chromium |
| 2026-05-16 | Lua แทน Python | embed ง่าย binary เล็ก sandbox controllable |
