<#
.SYNOPSIS
    Assemble a portable AutoSuisei bundle from the release build.

.DESCRIPTION
    Copies the release AutoSuisei.exe along with all required runtime DLLs,
    Qt plugins, and Python sidecar scripts into a single self-contained
    folder, then zips it. The resulting zip is what gets shipped to
    end users (extract anywhere, double-click AutoSuisei.exe to run).

    Python itself is NOT bundled — the user still needs Python 3.10+ on
    PATH plus `pip install rapidocr-onnxruntime onnxruntime`. We can't
    redistribute a Python install legally without significant extra work.

.PARAMETER OutDir
    Where to assemble the bundle directory and zip. Defaults to
    C:\Users\hello\Backups\AutoSuisei.

.EXAMPLE
    .\scripts\make_bundle.ps1
#>
param(
    [string]$OutDir = 'C:\Users\hello\Backups\AutoSuisei'
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$releaseDir = Join-Path $root 'build\windows-x64-release\src\gui'
$vcpkgBin   = Join-Path $root 'build\windows-x64-release\vcpkg_installed\x64-windows\bin'

if (-not (Test-Path $releaseDir\AutoSuisei.exe)) {
    throw "Release exe not found at $releaseDir. Build first:`n  cmake --build --preset windows-x64-release --target AutoSuisei"
}
if (-not (Test-Path $vcpkgBin)) {
    throw "Release vcpkg bin not found at $vcpkgBin"
}

if (-not (Test-Path $OutDir)) { New-Item -ItemType Directory -Path $OutDir | Out-Null }

$stamp     = Get-Date -Format 'yyyyMMdd-HHmmss'
$bundle    = Join-Path $OutDir "AutoSuisei-portable-$stamp"
$zipTarget = "$bundle.zip"
if (Test-Path $bundle) { Remove-Item -Recurse -Force $bundle }
New-Item -ItemType Directory -Path $bundle | Out-Null

Write-Host "Assembling bundle in $bundle"

# 1. main exe + Qt plugins (already deployed next to release exe by post-build)
Copy-Item "$releaseDir\AutoSuisei.exe" $bundle
foreach ($sub in @('platforms', 'styles', 'imageformats')) {
    if (Test-Path "$releaseDir\$sub") {
        Copy-Item -Recurse "$releaseDir\$sub" $bundle
    }
}

# 2. all release DLLs from vcpkg — overkill but reliable
Write-Host '  copying vcpkg release DLLs...'
Get-ChildItem $vcpkgBin -Filter '*.dll' | ForEach-Object {
    Copy-Item $_.FullName $bundle
}

# 2b. MSVC C++ runtime DLLs (app-local deployment)
# Target machines without Visual C++ Redistributable get error
# 0xc0000142 ("application unable to start") because they can't find
# vcruntime140.dll / msvcp140.dll. Shipping these next to the exe
# (app-local) is the supported MS deployment path that doesn't require
# admin to install the redist system-wide.
$msvcCrt = Get-ChildItem -Path 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Redist\MSVC' `
    -Recurse -Filter 'vcruntime140.dll' -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -match '\\x64\\' -and
                   $_.FullName -notmatch 'onecore' -and
                   $_.FullName -notmatch 'spectre' } |
    Select-Object -First 1 -ExpandProperty DirectoryName
if ($msvcCrt) {
    Write-Host "  copying MSVC CRT from $msvcCrt..."
    Get-ChildItem $msvcCrt -Filter '*.dll' | ForEach-Object {
        Copy-Item $_.FullName $bundle -Force
    }
} else {
    Write-Warning 'MSVC v143 CRT not found — bundle may fail on machines without VC++ Redist'
}

# 3. Python sidecar scripts
Write-Host '  copying scripts/...'
$scriptsTarget = Join-Path $bundle 'scripts'
New-Item -ItemType Directory -Path $scriptsTarget | Out-Null
Get-ChildItem "$root\scripts" -File -Filter '*.py' | ForEach-Object {
    Copy-Item $_.FullName $scriptsTarget
}

# 3b. ML model (sticker digit detector) — sticker_digit.py resolves it as
# <scripts-parent>/models/sticker_digit.onnx, i.e. <bundle>/models/. Without
# this the donate sticker-number model is missing and the Photos-3-001 fusion
# silently falls back to crop-only (DonateMore still works via the explicit path).
$modelsSrc = Join-Path $root 'models'
if (Test-Path "$modelsSrc\sticker_digit.onnx") {
    Write-Host '  copying models/sticker_digit.onnx...'
    $modelsTarget = Join-Path $bundle 'models'
    New-Item -ItemType Directory -Path $modelsTarget | Out-Null
    Copy-Item "$modelsSrc\sticker_digit.onnx" $modelsTarget
} else {
    Write-Warning 'models/sticker_digit.onnx not found — donate sticker model will be absent from bundle.'
}

# 4. embedded Python (if it's been built — produced by setup_embedded_python.ps1)
$pyDir = Join-Path $root 'build\embedded-python'
if (Test-Path "$pyDir\python.exe") {
    Write-Host '  copying embedded Python (this takes a moment)...'
    $pyTarget = Join-Path $bundle 'python'
    Copy-Item -Recurse $pyDir $pyTarget
    # Strip __pycache__ to save space
    Get-ChildItem $pyTarget -Recurse -Force -Filter '__pycache__' -Directory |
        Remove-Item -Recurse -Force

    # Mirror MSVC CRT into python/ — onnxruntime_pybind11_state.pyd
    # loads via python.exe's loader, which doesn't see DLLs next to
    # AutoSuisei.exe. Without these the DLL init fails with
    # "A dynamic link library (DLL) initialization routine failed".
    if ($msvcCrt) {
        Write-Host '  mirroring MSVC CRT into python/ for Python DLL loader...'
        Get-ChildItem $msvcCrt -Filter '*.dll' | ForEach-Object {
            Copy-Item $_.FullName $pyTarget -Force
        }
    }
} else {
    Write-Warning 'embedded-python not built; bundle will require system Python.'
    Write-Warning '  run scripts/setup_embedded_python.ps1 first if you want self-contained.'
}

# 4. README for the bundle recipient
$readme = @'
AutoSuisei — Portable Bundle
============================

Inventory OCR — ดึง No. + Dell serial จากภาพถ่ายมือถือ

วิธีใช้:
  1. คลายไฟล์ zip นี้ไว้ตรงไหนก็ได้บนเครื่อง
  2. ดับเบิ้ลคลิก AutoSuisei.exe

(self-contained — Python + รายการ deps อยู่ใน python/ พร้อมใช้ทันที)

3 แท็บ:
  OCR    — Bulk Extract ภาพในโฟลเดอร์ทั้งหมดทีเดียว
  Watch  — เฝ้าโฟลเดอร์ ภาพใหม่มา → auto-extract ทันที
  Review — ตรวจ/แก้ผล + Rename ไฟล์เป็น No.

ดูประวัติเวอร์ชั่นที่ docs/dev-plan.md ใน source repo
'@
Set-Content -Path (Join-Path $bundle 'README.txt') -Value $readme -Encoding utf8

# 5. zip it
Write-Host "  zipping → $zipTarget"
if (Test-Path $zipTarget) { Remove-Item -Force $zipTarget }
Compress-Archive -Path "$bundle\*" -DestinationPath $zipTarget

$bundleSizeMB = [math]::Round((Get-ChildItem $bundle -Recurse | Measure-Object Length -Sum).Sum / 1MB, 1)
$zipSizeMB    = [math]::Round((Get-Item $zipTarget).Length / 1MB, 1)
Write-Host ''
Write-Host "Bundle:  $bundle   ($bundleSizeMB MB)"
Write-Host "Zip:     $zipTarget   ($zipSizeMB MB)"
