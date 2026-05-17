<#
.SYNOPSIS
    Download + configure an embeddable Python distribution + install the
    OCR runtime packages, ready to be bundled into the AutoPilot installer.

.DESCRIPTION
    Output: $root\build\embedded-python\
        ├── python.exe              # standalone Windows Python (no Start menu, no PATH)
        ├── python311.dll
        ├── python311._pth          # patched to enable site-packages
        ├── Lib/site-packages/
        │     ├── rapidocr_onnxruntime/
        │     ├── onnxruntime/
        │     ├── numpy/
        │     ├── opencv-python-headless/
        │     └── ... (transitive deps)
        └── ...

    This folder is the one make_bundle.ps1 copies into the portable
    bundle as "python/". The Qt code calls "<exeDir>\python\python.exe"
    instead of requiring system Python.

    Idempotent: skips download / pip-install if the folder already
    looks correct. Pass -Force to recreate from scratch.

.PARAMETER Force
    Delete the existing embedded-python folder first, then rebuild.
#>
param(
    [switch]$Force
)

$ErrorActionPreference = 'Stop'
$root      = Split-Path -Parent $PSScriptRoot
$outDir    = Join-Path $root 'build\embedded-python'
$pyVersion = '3.11.9'
$pyZipUrl  = "https://www.python.org/ftp/python/$pyVersion/python-$pyVersion-embed-amd64.zip"
$pyZip     = Join-Path $root "build\python-embed-$pyVersion.zip"
$getPipUrl = 'https://bootstrap.pypa.io/get-pip.py'
$getPipPy  = Join-Path $root 'build\get-pip.py'

if ($Force -and (Test-Path $outDir)) {
    Write-Host "Force: removing existing $outDir"
    Remove-Item -Recurse -Force $outDir
}

# Skip if already set up
$marker = Join-Path $outDir 'Lib\site-packages\rapidocr_onnxruntime'
if (Test-Path $marker) {
    Write-Host "Embedded Python already set up at $outDir"
    Write-Host "  pass -Force to rebuild"
    return
}

if (-not (Test-Path 'build')) { New-Item -ItemType Directory build | Out-Null }

# 1. download embeddable Python
if (-not (Test-Path $pyZip)) {
    Write-Host "[1/5] Downloading Python $pyVersion embeddable..."
    Invoke-WebRequest -Uri $pyZipUrl -OutFile $pyZip -UseBasicParsing
}

# 2. extract
Write-Host "[2/5] Extracting to $outDir..."
if (Test-Path $outDir) { Remove-Item -Recurse -Force $outDir }
New-Item -ItemType Directory $outDir | Out-Null
Expand-Archive -Path $pyZip -DestinationPath $outDir -Force

# 3. patch python311._pth to enable site-packages (default is "no site")
$pth = Get-ChildItem $outDir -Filter 'python*._pth' | Select-Object -First 1
if (-not $pth) { throw "couldn't find python*._pth in $outDir" }
Write-Host "[3/5] Patching $($pth.Name) to enable site-packages..."
$content = Get-Content $pth.FullName
$patched = $content -replace '^#import site', 'import site'
Set-Content -Path $pth.FullName -Value $patched

# 4. install pip
if (-not (Test-Path $getPipPy)) {
    Write-Host "[4/5] Downloading get-pip.py..."
    Invoke-WebRequest -Uri $getPipUrl -OutFile $getPipPy -UseBasicParsing
}
$pyExe = Join-Path $outDir 'python.exe'
Write-Host "[4/5] Bootstrapping pip..."
# pip ใช้ stderr สำหรับ INFO/WARNING ที่ไม่ใช่ error — temporarily relax error
# action เพื่อไม่ให้ PowerShell ห่อ stderr เป็น terminating error
$prevEAP = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
try {
    & $pyExe $getPipPy --no-warn-script-location 2>&1 | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "get-pip.py failed (exit $LASTEXITCODE)" }

    Write-Host "[5/5] Installing rapidocr-onnxruntime + onnxruntime (5-10 min, downloads ~150MB)..."
    & $pyExe -m pip install --no-warn-script-location `
        rapidocr-onnxruntime onnxruntime 2>&1 | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "pip install failed (exit $LASTEXITCODE)" }
} finally {
    $ErrorActionPreference = $prevEAP
}

$size = [math]::Round((Get-ChildItem $outDir -Recurse | Measure-Object Length -Sum).Sum / 1MB, 0)
Write-Host ""
Write-Host "Embedded Python ready at $outDir ($size MB)"
Write-Host "Smoke test:"
& $pyExe -c "from rapidocr_onnxruntime import RapidOCR; print('OK')"
