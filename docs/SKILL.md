# SKILL.md — Patterns & Recipes (จากการพัฒนา AutoSuisei v0.9.0)

> รวบรวม pattern, gotcha, recipe ที่เรียนรู้ระหว่างพัฒนา ที่นำกลับมาใช้กับโปรเจกต์
> Qt6 + Python OCR sidecar / Windows desktop app อื่นๆ ได้

---

## 1. Python sidecar via QProcess + JSON-per-line protocol

**Pattern**: Qt C++ frontend + Python heavy-compute backend สื่อสารผ่าน stdin/stdout

```cpp
// C++ side
QProcess* p = new QProcess(this);
p->setProcessChannelMode(QProcess::SeparateChannels);
connect(p, &QProcess::readyReadStandardOutput, this, &Self::onStdout);
p->start(pythonExe(), {script, arg1, arg2, "--progress-json"});

void Self::onStdout() {
    buf_ += p->readAllStandardOutput();
    while (true) {
        const int nl = buf_.indexOf('\n');
        if (nl < 0) break;
        const QByteArray line = buf_.left(nl).trimmed();
        buf_.remove(0, nl + 1);
        if (line.isEmpty()) continue;
        try {
            auto j = nlohmann::json::parse(line.toStdString());
            handleEvent(j);
        } catch (...) { /* ignore non-JSON (e.g. stderr leaked) */ }
    }
}
```

```python
# Python side
import json, sys
def emit(obj): sys.stdout.write(json.dumps(obj) + "\n"); sys.stdout.flush()

emit({"event": "start", "total": n})
emit({"event": "row", "i": i, "data": {...}})
emit({"event": "done"})
```

**Gotchas**:
- `setProcessChannelMode(QProcess::SeparateChannels)` — แยก stdout / stderr ไม่งั้น stderr ปนใน JSON parser
- Python ต้อง `flush=True` ทุก print (ไม่งั้น Windows pipe buffer 64KB)
- Buffer line-by-line ฝั่ง C++ — pipe อาจส่งหลาย line รวมกัน

## 2. Python script bundled exe — script path resolution

3-tier fallback:

```cpp
QString scriptsDir() {
    // 1. Env var override
    auto fromEnv = qEnvironmentVariable("AUTOPILOT_SCRIPTS_DIR");
    if (!fromEnv.isEmpty() && QDir(fromEnv).exists()) return fromEnv;
    // 2. Compile-time baked path (dev build)
    const auto baked = QStringLiteral(AUTOPILOT_SCRIPTS_DIR);
    if (!baked.isEmpty() && QDir(baked).exists()) return baked;
    // 3. <exeDir>/scripts/ (bundled exe)
    return QDir(QCoreApplication::applicationDirPath()).filePath("scripts");
}
```

CMake:
```cmake
target_compile_definitions(MyApp PRIVATE
    AUTOPILOT_SCRIPTS_DIR="${CMAKE_SOURCE_DIR}/scripts")
```

## 3. Bundled embedded Python on Windows

**Layout in bundle**:
```
<exeDir>/
├── MyApp.exe
├── *.dll (Qt + vcpkg)
├── vcruntime140.dll, msvcp140.dll (MSVC CRT — app-local)
├── python/
│   ├── python.exe
│   ├── python311.dll
│   ├── python311._pth (patched: `import site` uncommented)
│   ├── vcruntime140.dll, msvcp140.dll (mirror — Python loader needs)
│   ├── Lib/site-packages/{rapidocr_onnxruntime, onnxruntime, ...}
└── scripts/
    └── *.py
```

**Gotcha**: `onnxruntime_pybind11_state.pyd` โหลดผ่าน `python.exe` ซึ่ง DLL loader ค้นโฟลเดอร์ของ python.exe (`python/`) ไม่ใช่โฟลเดอร์ของ main exe → **mirror MSVC CRT เข้า `python/` ด้วย**

## 4. Bundling fonts in Qt Resource (.qrc)

```xml
<!-- resources.qrc -->
<RCC>
    <qresource prefix="/">
        <file>theme.qss</file>
        <file>resources/fonts/MyFont-Regular.ttf</file>
    </qresource>
</RCC>
```

```cpp
const int fid = QFontDatabase::addApplicationFont(
    ":/resources/fonts/MyFont-Regular.ttf");
const auto family = QFontDatabase::applicationFontFamilies(fid).value(0);
QFont base(family, 11);
app.setFont(base);

// Thai fallback (font lacks Thai glyphs)
QFont::insertSubstitutions(family,
    {"Leelawadee UI", "Tahoma", "Sarabun", "Loma"});
```

## 5. QListWidget + setItemWidget (custom row widget)

For nav sidebar items with icon + label + badge:

```cpp
auto* item = new QListWidgetItem(list_);
item->setSizeHint(QSize(220, 60));   // ← explicit width, else label truncates
auto* row = new MyRowWidget(...);
list_->setItemWidget(item, row);
```

**Gotcha**: QSS `padding` on `::item` does NOT apply to `setItemWidget` content — instead it shrinks available width for the widget. Remove `padding`, control spacing inside the widget instead.

```css
/* GOOD */
QListWidget#nav::item { margin: 2px 4px; border-radius: 8px; }

/* BAD — shrinks widget area */
QListWidget#nav::item { padding: 12px; ... }
```

**qobject_cast vs static_cast**: Custom widget in anonymous namespace can't have Q_OBJECT → use `static_cast` since you know the type at insertion:

```cpp
auto* row = static_cast<MyRowWidget*>(list_->itemWidget(items_[idx]));
```

## 6. Process kill on Windows — what stays simple

For "Stop bulk job" button:
```cpp
bulkProcess_->kill();   // ← QProcess does TerminateProcess; works reliably
```

**What does NOT work reliably** (tried + abandoned):
- `NtSuspendProcess` from ntdll.dll — undocumented, OpenProcess may fail silently
- Toolhelp32 SuspendThread on each thread — works but pipe-buffered output still drains after suspend → user sees "no immediate effect"
- ConsoleCtrlEvent(CTRL_C_EVENT) — child must share console

**Lesson**: for "pause + resume from same place", Python-level cooperation (sleep loop checking a flag file) is far more reliable than OS-level suspend. We removed Pause feature per user request.

## 7. OCR rotation fallback (PaddleOCR can't auto-rotate 90°/270°)

```python
def ocr_with_rotation(engine, img_path, category):
    """Try 0°, fall back to 90°/270°/180° if scoring criteria not met.
    
    Score: (has_serial, has_pc_no, mean_confidence) — higher wins.
    """
    import cv2
    text0, mc0, lc0 = _run(engine, str(img_path))
    s0 = _score(text0, category, mc0)
    if s0[0] == 1 and s0[1] == 1:  # both found, no rotation needed
        return text0, mc0, lc0
    img = cv2.imread(str(img_path))
    if img is None: return text0, mc0, lc0
    best = (s0, text0, mc0, lc0)
    for angle, code in [(90, cv2.ROTATE_90_CLOCKWISE),
                         (270, cv2.ROTATE_90_COUNTERCLOCKWISE),
                         (180, cv2.ROTATE_180)]:
        rotated = cv2.rotate(img, code)
        text, mc, lc = _run(engine, rotated)
        s = _score(text, category, mc)
        if s > best[0]: best = (s, text, mc, lc)
        if s[0] == 1 and s[1] == 1: break  # early exit
    return best[1], best[2], best[3]
```

**Impact**: Serial extraction on rotated batch 53% → 96% (+43pp).

**Cost**: 2-3x time on failure cases (still acceptable: 0.5s → 1s/photo).

## 8. Regex for Dell serial (CN-...-A00 format)

Real format: `CN-0XXXX-XXXXX-XXX-XXXX-A00` (5-6 hyphen-separated segments)

```python
# Match cleanly, segments 2-7 chars to avoid swallowing barcodes
MONITOR_SERIAL_RE = re.compile(
    r"\b(CN-[A-Z0-9]{4,7}-[A-Z0-9]{3,6}-[A-Z0-9]{2,5}"
    r"(?:-[A-Z0-9]{2,5})?(?:-[A-Z0-9]{2,4})?)\b")
```

**Pre-processing** (order matters!):
1. **Strip CBA asset barcode lines** `^\s*CBA[0-9]+\s*$` — these interleave between CN segments
2. **Strip 7+ digit numeric lines** `^\s*\d{7,}\s*$` — Express SVC codes / phone numbers
3. **Collapse trailing-hyphen line breaks** `-\s*\n\s*` → `-`
4. **Line-merge fragments** — if line matches `^CN-...` and next line is `^[A-Z0-9]+(-[A-Z0-9]+)*-?$`, merge with `-`

```python
def _merge_cn_fragments(text):
    """OCR sometimes drops trailing hyphens. Walk lines, merge continuations."""
    lines = text.split("\n")
    out, i = [], 0
    while i < len(lines):
        stripped = lines[i].strip()
        if _CN_LINE_RE.match(stripped):
            merged = [stripped.rstrip("-")]
            i += 1
            while i < len(lines):
                nxt = lines[i].strip()
                if _CN_CONT_RE.match(nxt) and not nxt.startswith("CBA"):
                    merged.append(nxt.rstrip("-"))
                    i += 1
                else: break
            out.append("-".join(merged))
        else:
            out.append(lines[i]); i += 1
    return "\n".join(out)
```

## 9. Thai PDF generation with fpdf2 + Tahoma

```python
from fpdf import FPDF
pdf = FPDF(orientation="P", unit="mm", format="A4")
pdf.add_font("Tahoma", "", r"C:\Windows\Fonts\tahoma.ttf")
pdf.add_font("Tahoma", "B", r"C:\Windows\Fonts\tahomabd.ttf")
pdf.set_font("Tahoma", "", 11)
```

**Gotcha**: `multi_cell` ขึ้น "Not enough horizontal space" ถ้า cursor x ไม่อยู่ที่ left margin (เช่น หลัง `image()` หรือ partial `cell()`). Always:
```python
self.set_x(self.l_margin)   # ← reset before multi_cell
self.multi_cell(0, 6, text)
```

## 10. Windows build environment in Qt project

**vcvars64 loading** in PowerShell:
```powershell
$vcvars = 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat'
$envBlock = & cmd /c "`"$vcvars`" >nul && set"
foreach ($line in $envBlock) {
    if ($line -match '^([^=]+)=(.*)$') { Set-Item -Path "env:$($Matches[1])" -Value $Matches[2] }
}
```

This captures the env from cmd's view of vcvars64.bat and applies to current PowerShell session.

**CMake preset workflow**:
```powershell
cmake --preset windows-x64-release       # Configure
cmake --build --preset windows-x64-release --target MyApp   # Build target
```

CMakePresets.json with vcpkg toolchain:
```json
{
  "cacheVariables": {
    "CMAKE_TOOLCHAIN_FILE": "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake",
    "VCPKG_TARGET_TRIPLET": "x64-windows"
  }
}
```

## 11. Refactor parser signature without breaking sidecars

**Lesson**: when changing function signature ใน module (e.g. `extract_serial(text)` → `extract_serial(text, category)`),
search ALL consumers including sister Python scripts that import the module.

Real bug: `ocr_worker.py` imported `extract_serial` from `bulk_extract.py` — after refactor,
called with old signature → TypeError ทันทีที่ภาพแรก → worker crash silent → watching stops.

**Prevention**: Default arg + warn:
```python
def extract_serial(text, category="pc"):
    # backward-compat: callers without category default to 'pc'
```

Or: keep dispatcher with explicit per-category functions exported:
```python
def extract_serial_pc(text): ...
def extract_serial_monitor(text): ...
def extract_serial_accessory(text): ...
def extract_serial(text, category):  # dispatcher
    return {"pc": extract_serial_pc, ...}[category](text)
```

## 12. Sequential vs filename-derived index

**Anti-pattern**: ใช้ `photo_index` จาก filename suffix (`_NNN.jpg`) เป็น `#` column ของ table
— เพราะ Python's `sorted()` เรียงตัวอักษร: `_1, _10, _100, _101, _102, _2, _3, ...`

**Fix**: ใช้ `row + 1` เป็น display index (เรียงตามลำดับที่ประมวลผลจริง):
```cpp
table_->setItem(row, 0, new QTableWidgetItem(QString::number(row + 1)));
```

## 13. Git LFS / large binary files

Bundled font (~100 KB) — OK ใส่ git ปกติ. Bundled embedded Python (~150 MB) — ต้องใช้
LFS หรือไม่ commit (ให้ user build เอง). เราเลือก **ไม่ commit** — `setup_embedded_python.ps1`
ดาวน์โหลด + setup ตอน build.

`.gitignore`:
```
build/
vcpkg_installed/
*.log
__pycache__/
moc_*.cpp
```

## 14. Documenting decisions in CLAUDE.md

**Pattern**: Project state markdown file ที่:
- มี "Tech Stack" + "Build commands" + "Module boundaries" — context สำหรับ Claude session
- มี "Session 2026-XX-XX summary" ต่อท้าย — history of major changes
- Reference `docs/dev-plan.md` (phase log) + `docs/tomorrow.md` (next session)

Each session: update Status section + add Session summary block + create new `tomorrow.md`.

---

## Recipes ที่ใช้ได้ทันที (copy-paste)

- **OCR pipeline**: section 1, 2, 3, 7, 8
- **Qt GUI redesign with QSS**: section 4, 5
- **Windows installer (Inno Setup) + embedded Python**: section 3
- **Thai PDF auto-generation**: section 9
- **Build environment**: section 10
