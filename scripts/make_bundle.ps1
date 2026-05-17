<#
.SYNOPSIS
    Assemble a portable AutoPilot bundle from the release build.

.DESCRIPTION
    Copies the release AutoPilot.exe along with all required runtime DLLs,
    Qt plugins, and Python sidecar scripts into a single self-contained
    folder, then zips it. The resulting zip is what gets shipped to
    end users (extract anywhere, double-click AutoPilot.exe to run).

    Python itself is NOT bundled — the user still needs Python 3.10+ on
    PATH plus `pip install rapidocr-onnxruntime onnxruntime`. We can't
    redistribute a Python install legally without significant extra work.

.PARAMETER OutDir
    Where to assemble the bundle directory and zip. Defaults to
    C:\Users\hello\Backups\AutoPilot.

.EXAMPLE
    .\scripts\make_bundle.ps1
#>
param(
    [string]$OutDir = 'C:\Users\hello\Backups\AutoPilot'
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$releaseDir = Join-Path $root 'build\windows-x64-release\src\gui'
$vcpkgBin   = Join-Path $root 'build\windows-x64-release\vcpkg_installed\x64-windows\bin'

if (-not (Test-Path $releaseDir\AutoPilot.exe)) {
    throw "Release exe not found at $releaseDir. Build first:`n  cmake --build --preset windows-x64-release --target AutoPilot"
}
if (-not (Test-Path $vcpkgBin)) {
    throw "Release vcpkg bin not found at $vcpkgBin"
}

if (-not (Test-Path $OutDir)) { New-Item -ItemType Directory -Path $OutDir | Out-Null }

$stamp     = Get-Date -Format 'yyyyMMdd-HHmmss'
$bundle    = Join-Path $OutDir "AutoPilot-portable-$stamp"
$zipTarget = "$bundle.zip"
if (Test-Path $bundle) { Remove-Item -Recurse -Force $bundle }
New-Item -ItemType Directory -Path $bundle | Out-Null

Write-Host "Assembling bundle in $bundle"

# 1. main exe + Qt plugins (already deployed next to release exe by post-build)
Copy-Item "$releaseDir\AutoPilot.exe" $bundle
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

# 3. Python sidecar scripts
Write-Host '  copying scripts/...'
$scriptsTarget = Join-Path $bundle 'scripts'
New-Item -ItemType Directory -Path $scriptsTarget | Out-Null
Get-ChildItem "$root\scripts" -File -Filter '*.py' | ForEach-Object {
    Copy-Item $_.FullName $scriptsTarget
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
} else {
    Write-Warning 'embedded-python not built; bundle will require system Python.'
    Write-Warning '  run scripts/setup_embedded_python.ps1 first if you want self-contained.'
}

# 4. README for the bundle recipient
$readme = @'
AutoPilot — Portable Bundle
============================

PC Inventory OCR — ดึง PC No. + Dell serial จากภาพถ่ายมือถือ

วิธีใช้:
  1. คลายไฟล์ zip นี้ไว้ตรงไหนก็ได้บนเครื่อง
  2. ดับเบิ้ลคลิก AutoPilot.exe

(self-contained — Python + รายการ deps อยู่ใน python/ พร้อมใช้ทันที)

3 แท็บ:
  OCR    — Bulk Extract ภาพในโฟลเดอร์ทั้งหมดทีเดียว
  Watch  — เฝ้าโฟลเดอร์ ภาพใหม่มา → auto-extract ทันที
  Review — ตรวจ/แก้ผล + Rename ไฟล์เป็น PC No.

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
