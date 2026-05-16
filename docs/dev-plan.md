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

## Decisions Log
| Date | Decision | Reason |
|---|---|---|
| 2026-05-16 | Windows-only | ลดความซับซ้อน ใช้ WinAPI/UI Automation ได้เต็มที่ |
| 2026-05-16 | Qt 6 (ไม่ใช่ ImGui) | ต้องการ macro editor ที่มี timeline view + accessibility |
| 2026-05-16 | Tesseract local (ไม่ใช่ cloud) | offline-first ไม่มี recurring cost |
| 2026-05-16 | CDP แทน CEF | binary size เล็ก ไม่ต้อง embed Chromium |
| 2026-05-16 | Lua แทน Python | embed ง่าย binary เล็ก sandbox controllable |
