# AutoSuisei

**เครื่องมือ OCR สำหรับงาน inventory เครื่อง IT** — ดึง **เลขกำกับ (No.) + Dell Serial** จากภาพถ่ายมือถือ
ของเครื่องที่ wipe แล้ว → ตรวจ/แก้ใน GUI → **เปลี่ยนชื่อไฟล์ภาพอัตโนมัติ**. Windows desktop app (C++20 Qt 6 + Python/PaddleOCR).

> เดิมชื่อ *AutoPilot* — rebrand เป็น UI-only (exe/installer = "AutoSuisei"); namespace `autopilot::` + library targets ยังใช้ชื่อเดิม

---

## ✨ Features
- **Barcode-first Serial** — อ่าน Dell barcode ก่อน OCR (แม่นกว่า, แก้ปัญหา OCR สับสน `O`↔`0`):
  - PC/Laptop = **Data Matrix** (pylibdmtx) · Desktop/Donate = **Code128** (ZBar) · Monitor = **CN serial** (Data Matrix/Code128 + rotation sweep → `CN-…-A00`)
  - อ่านไม่ออก → fallback OCR อัตโนมัติ + ระบุที่มาในคอลัมน์ **Src**
- **OCR 4 หมวด** — PC&Laptop / Monitor / Accessory / Donate (parser แยกตามชนิดป้าย)
- **3 โหมด (sidebar)** — **OCR** (bulk extract ทั้งโฟลเดอร์) · **Folder Watch** (ประมวลผลไฟล์ใหม่อัตโนมัติ) · **Review** (ดูภาพ + แก้ค่า + เปลี่ยนชื่อไฟล์)
- **Rotation fallback** — รองรับภาพถ่ายเอียง/กลับหัว (90°/180°/270°)
- **Auto-update** — ปุ่ม *Check for updates* (ผ่าน GitHub Releases) + แสดงเวอร์ชันมุมซ้ายล่าง

## ⬇️ Install (ผู้ใช้ทั่วไป)
ดาวน์โหลด **`AutoSuisei-Setup-x.y.z.exe`** จาก [Releases](https://github.com/helloshinee002-cell/AutoSuisei/releases) แล้วติดตั้ง
(ฝัง Python + PaddleOCR ในตัว — ไม่ต้องลงอะไรเพิ่ม). อัปเดตภายหลังกดปุ่ม *Check for updates* ในแอปได้เลย

## 🛠️ Build from source (Windows)
ต้องมี: Visual Studio 2022/Build Tools (MSVC v143) · CMake ≥ 3.25 · Ninja · vcpkg (`VCPKG_ROOT`)
```powershell
# โหลด vcvars64 ก่อนทุกครั้ง (ให้ cl.exe / ninja อยู่ใน PATH)
vcpkg install --triplet x64-windows          # 1) ลง C++ deps (Qt6/OpenCV/Tesseract/…)

cmake --preset windows-x64-release           # 2) configure + build GUI
cmake --build --preset windows-x64-release --target AutoSuisei

powershell -File scripts\setup_embedded_python.ps1   # 3) สร้าง Python sidecar (PaddleOCR + pylibdmtx + pyzbar)

.\build\windows-x64-release\src\gui\AutoSuisei.exe   # 4) รัน
ctest --preset windows-x64-debug --output-on-failure # (test — debug preset เท่านั้น)
```
แพ็กเป็น installer: `scripts\make_installer.ps1` (→ bundle + Inno Setup) · update-package: `scripts\make_update_package.ps1`

## 🧱 Tech stack
- **C++20 + Qt 6 Widgets** (GUI) — `qt_add_executable`, vcpkg manifest mode
- **Python (PaddleOCR ผ่าน `rapidocr-onnxruntime`)** sidecar — เรียกผ่าน `QProcess` + JSON-per-line protocol
- **OpenCV** (image I/O + rotation) · **pylibdmtx** (Data Matrix) · **pyzbar/ZBar** (Code128/QR) · **nlohmann/json** · **spdlog** · **SQLite** · **GoogleTest**

## 📁 Structure (ย่อ)
`src/ocr` (AssetExtractor/ReviewModel) · `src/gui` (OcrTab/WatchTab/ReviewTab/Updater) · `src/cli` (headless `autopilot_cli ocr <image>`) · `scripts/` (Python sidecars: `bulk_extract.py`, `ocr_worker.py` + build scripts) · `tests/` (GoogleTest)

> หมายเหตุ: โมดูล legacy (macro/web recorder, Lua scripting, CDP, image matcher) ยังอยู่ใน codebase จาก phase ก่อน
> แต่ **ไม่ใช่ฟีเจอร์ของผลิตภัณฑ์ปัจจุบัน** — รายละเอียด stack/rules/workflow ดู [CLAUDE.md](./CLAUDE.md)

## License
TBD
