<#
.SYNOPSIS
    Build update-package.zip (app-patch) = portable bundle MINUS the python/ folder.

.DESCRIPTION
    The auto-updater (src/gui/Updater.cpp) downloads this instead of the full installer to save
    bandwidth. The embedded Python (~440 MB) is a stable base, so we strip python/ and ship only the
    app layer (exe + Qt DLLs + plugins + scripts/ + models/ + other DLLs + icon, ~30-50 MB).
    The updater extracts this over the existing install dir; the old python/ stays in place.

    NOTE: if a release changes Python deps (adds/upgrades a pip lib, e.g. pylibdmtx/pyzbar), do NOT
    publish update-package.zip for that release -- the updater will fall back to the full installer .exe
    (a patch without python/ would miss the new lib). State "full install required" in the release notes.

    ASCII-only on purpose: Windows PowerShell 5.1 parses .ps1 as ANSI; non-ASCII without a BOM breaks.

    Run after scripts/make_bundle.ps1 (uses the newest bundle). Upload the zip to the GitHub Release
    as an asset named exactly "update-package.zip".
#>
param(
    [string]$BundleDir = "",
    [string]$OutDir = "C:\Users\hello\Backups\AutoSuisei"
)
$ErrorActionPreference = 'Stop'

if (-not $BundleDir) {
    $latest = Get-ChildItem "$OutDir\AutoSuisei-portable-*" -Directory |
              Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if ($latest) { $BundleDir = $latest.FullName }
}
if (-not $BundleDir -or -not (Test-Path $BundleDir)) {
    throw "No portable bundle found in $OutDir -- run scripts/make_bundle.ps1 first"
}
Write-Host "Bundle source: $BundleDir"

$stage = Join-Path $env:TEMP "autosuisei-update-stage"
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Path $stage | Out-Null

# Copy everything except python/ (embedded Python base -- excluded to keep the patch small)
Get-ChildItem $BundleDir | Where-Object { $_.Name -ne 'python' } | ForEach-Object {
    Copy-Item $_.FullName -Destination $stage -Recurse -Force
}

$zip = Join-Path $OutDir "update-package.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path "$stage\*" -DestinationPath $zip -CompressionLevel Optimal
Remove-Item $stage -Recurse -Force

$mb = [math]::Round((Get-Item $zip).Length / 1MB, 1)
Write-Host ""
Write-Host "update-package: $zip  ($mb MB)"
Write-Host "(= bundle minus python/; updater extracts over install dir, keeps existing python/)"
