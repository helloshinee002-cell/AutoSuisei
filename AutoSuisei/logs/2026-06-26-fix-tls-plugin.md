---
date: 2026-06-26 19:20
idea: "[[ideas/v1.0.0-release-and-autoupdate]]"
tags: [log, autosuisei, fix, updater, qt]
---

# Fix — Qt TLS plugin หาย → updater "TLS initialization failed"

- **Date/Time:** 2026-06-26 19:20 (วันศุกร์)
- **Idea Ref:** [[ideas/v1.0.0-release-and-autoupdate]] (bug หลัง release v1.0.0)
- **Changes Made:**
  - กด "Check for updates" ใน v1.0.0 → error **"TLS initialization failed"** (เชื่อมต่อ GitHub ไม่ได้)
  - แก้: deploy **Qt `tls` plugin folder** (`qopensslbackend.dll` + `qschannelbackend.dll`) ข้าง exe + ใน bundle
    - `src/gui/CMakeLists.txt`: เพิ่ม `tls` ใน POST_BUILD plugin foreach (`platforms styles imageformats tls`)
    - `scripts/make_bundle.ps1`: เพิ่ม `'tls'` ใน plugin copy list
  - rebuild release → re-bundle → ทับ installer + update-package บน **GitHub release v1.0.0** (`gh release upload --clobber`)
- **Technical Notes:**
  - 🔑 **gotcha: Qt6 ย้าย TLS เป็น plugin แยก** (`plugins/tls/`) — Qt5 มี SSL ใน Qt5Network เลย แต่ Qt6 ต้อง **deploy `tls/` folder**
    ไม่งั้น `QSslSocket::supportsSsl()` = false → HTTPS ทุกอย่างพัง ("TLS initialization failed"). plugin อื่นที่ deploy อยู่แล้ว
    = platforms/styles/imageformats เท่านั้น → ต้องเพิ่ม tls
  - OpenSSL DLL (`libssl-3-x64.dll`/`libcrypto-3-x64.dll`) มากับ vcpkg DLLs อยู่แล้ว (จึงไป error ที่ขั้น TLS init ไม่ใช่ missing-dll)
  - deploy **ทั้ง folder** → ได้ `qschannelbackend.dll` (Windows native TLS, ไม่ต้องใช้ OpenSSL) เป็น fallback ชั้นสอง
  - exe **ไม่ถูก recompile** (ไม่มี source เปลี่ยน) — แค่ POST_BUILD deploy step + re-bundle
  - **ระวังอนาคต**: เพิ่ม Qt module/feature ที่ใช้ plugin (เช่น sqldrivers, multimedia) ต้องเพิ่มใน foreach ทั้ง 2 ที่ (CMake + make_bundle) ด้วย

## เกี่ยวข้อง
[[Build-and-Distribution]] §plugin deploy · [[logs/2026-06-26-v1.0.0-updater-monitor]] · [[changelog]]
