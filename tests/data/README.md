# Test Fixtures

วางไฟล์ภาพตัวอย่างที่ test ต้องใช้ลงในโฟลเดอร์นี้

ที่จำเป็น:
- `number_20.png` — ภาพที่เห็นเลข `20` ชัด (เคสตัวอย่างของ user)
- `xZ9k_3f8a1.png` — ภาพอะไรก็ได้ ใช้เทสต์ "ชื่อไฟล์แปลก เก็บไว้ใน result"

คำสั่งสร้างเร็ว ๆ (PowerShell + ImageMagick):
```powershell
magick -background white -fill black -size 200x100 -gravity center label:"20" number_20.png
magick -background white -fill black -size 200x100 -gravity center label:"hello" xZ9k_3f8a1.png
```
