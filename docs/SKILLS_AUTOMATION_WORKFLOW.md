# Skill: End-to-End Automation Workflow

ทุก feature ผ่าน 5 step ตามลำดับห้ามข้าม

## 1. Explore
อ่านไฟล์ที่เกี่ยวข้องสำรวจโครงสร้างก่อนเสนอแผน
- ใช้ Grep/Glob หาฟังก์ชันใกล้เคียง
- อ่าน `.clauderules` ของ module ที่จะแก้

## 2. Plan
สร้าง/อัปเดต `docs/dev-plan.md`
- ระบุ acceptance criteria แบบ measurable
- ระบุ risk + mitigation
- รอ user approve ก่อนเริ่ม implement

## 3. Test First (TDD — บังคับ)
เขียน GTest ที่ต้อง **fail** ก่อน
- หนึ่ง test = หนึ่ง behavior
- ตั้งชื่อ `TEST(Subject, ExpectedBehavior)`

## 4. Implement
เขียนโค้ดให้ test ผ่าน + รัน:
```powershell
clang-format -i src/<module>/*.{cpp,h}
clang-tidy -p build/windows-x64-debug src/<module>/*.cpp
```

## 5. Verify
```powershell
ctest --preset windows-x64-debug --output-on-failure
```
ต้องผ่าน 100% — ถ้าทำลาย test เดิมต้องแก้ก่อน commit

## 6. Report
- สรุปไฟล์ที่เปลี่ยน
- ร่าง Conventional Commit message
- ถ้ามี API change ระบุ migration note

---

## Blueprints (อ้างอิงโค้ดต้นแบบ)
- การเขียน OCR module → `src/ocr/OcrEngine.{h,cpp}` (pimpl + RAII + Doxygen)
- การเขียน interface → `src/recorder/IRecorder.h` (pure virtual + ActionCallback)
- การเขียน test → `tests/ocr/test_ocr_engine.cpp`

## Common Errors
| Symptom | Action |
|---|---|
| `vcpkg install` ล้ม | ตรวจ `VCPKG_ROOT` env + `vcpkg integrate install` |
| Tesseract init fail | ตั้ง `TESSDATA_PREFIX` ชี้ไปยังโฟลเดอร์ที่มี `eng.traineddata` |
| Qt6 ไม่เจอ | ใส่ `-DCMAKE_PREFIX_PATH=C:\Qt\6.7.2\msvc2022_64` |
| Hook ไม่ทำงาน | รัน Visual Studio/Terminal เป็น Administrator |

## Debug
- ใช้ `claude /doctor`
- เพิ่ม `spdlog::set_level(spdlog::level::debug)` ชั่วคราวลบออกก่อน commit
