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
    $env:USERPROFILE\Backups\AutoSuisei.

.EXAMPLE
    .\scripts\make_bundle.ps1
#>
param(
    [string]$OutDir = "$env:USERPROFILE\Backups\AutoSuisei"
)

$ErrorActionPreference = 'Stop'
# absolute: relative -OutDir must resolve the same for make_bundle, ISCC and make_update_package
$OutDir = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutDir)

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
foreach ($sub in @('platforms', 'styles', 'imageformats', 'tls')) {  # tls = HTTPS backend (Updater)
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
# any VS version/edition (<root>\<ver>\<edition>\VC\Redist\MSVC) - was hardcoded to 18\Community
$vsRedist = @(Get-ChildItem 'C:\Program Files\Microsoft Visual Studio', 'C:\Program Files (x86)\Microsoft Visual Studio' `
    -Directory -ErrorAction SilentlyContinue | Get-ChildItem -Directory |
    ForEach-Object { Join-Path $_.FullName 'VC\Redist\MSVC' } | Where-Object { Test-Path $_ })
$msvcCrt = $null
if ($vsRedist.Count -gt 0) {  # guard: Get-ChildItem -Path $null would crawl the current directory
    $msvcCrt = Get-ChildItem -LiteralPath $vsRedist `
        -Recurse -Filter 'vcruntime140.dll' -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match '\\x64\\' -and
                       $_.FullName -notmatch 'onecore' -and
                       $_.FullName -notmatch 'spectre' } |
        Select-Object -First 1 -ExpandProperty DirectoryName
}
if (-not $msvcCrt) {
    # a bundle without the CRT fails to start (0xc0000142) on PCs without the VC++ redist - never ship it
    throw 'MSVC v143 CRT (VC\Redist\MSVC\...\x64\...\vcruntime140.dll) not found in any Visual Studio install'
}
Write-Host "  copying MSVC CRT from $msvcCrt..."
Get-ChildItem $msvcCrt -Filter '*.dll' | ForEach-Object {
    Copy-Item $_.FullName $bundle -Force
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
    # barcode-first serial needs both libs - a bundle without them silently falls back to OCR
    foreach ($pkg in @('pyzbar', 'pylibdmtx')) {
        if (-not (Test-Path (Join-Path $pyTarget "Lib\site-packages\$pkg"))) {
            throw "embedded python has no $pkg - run scripts/setup_embedded_python.ps1 again"
        }
    }
    # libzbar-64.dll + libdmtx-64.dll need the VC++ 2013 runtime. python.exe's own folder is on the
    # DLL search path for both (ctypes LOAD_LIBRARY_SEARCH_DEFAULT_DIRS), so ship msvcr120.dll there.
    # make_update_package.ps1 keeps this one file so auto-updated installs get it too.
    $msvcr120 = Join-Path $env:WINDIR 'System32\msvcr120.dll'
    if (-not (Test-Path $msvcr120)) {
        throw 'msvcr120.dll not found - winget install Microsoft.VCRedist.2013.x64, then re-run'
    }
    Copy-Item $msvcr120 $pyTarget -Force
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
