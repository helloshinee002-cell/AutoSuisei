<#
.SYNOPSIS
    Build the AutoSuisei installer with Inno Setup.

.DESCRIPTION
    1. Re-runs scripts/make_bundle.ps1 to ensure a fresh portable bundle
       exists at <OutDir>\AutoSuisei-portable-*
    2. Invokes Inno Setup's iscc.exe on installer/AutoSuisei.iss
    3. Output: <OutDir>\AutoSuisei-Setup-<version>.exe

    Requires Inno Setup 6 installed (winget install JRSoftware.InnoSetup).
#>
param(
    [switch]$SkipBundle,
    [string]$OutDir = "$env:USERPROFILE\Backups\AutoSuisei"
)

$ErrorActionPreference = 'Stop'
# absolute: ISCC resolves a relative OutputDir against installer\, PowerShell against the cwd
$OutDir = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutDir)
$root = Split-Path -Parent $PSScriptRoot

# locate ISCC — try the common system + per-user install locations
$iscc = @(
    'C:\Program Files (x86)\Inno Setup 6\ISCC.exe',
    'C:\Program Files\Inno Setup 6\ISCC.exe',
    "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe"
) | Where-Object { Test-Path $_ } | Select-Object -First 1

if (-not $iscc) {
    throw @'
Inno Setup not found. Install via:
    winget install JRSoftware.InnoSetup
Then re-run this script.
'@
}

if (-not $SkipBundle) {
    Write-Host '[1/2] Refreshing portable bundle...'
    & "$root\scripts\make_bundle.ps1" -OutDir $OutDir
}

$bundleDir = Get-ChildItem "$OutDir\AutoSuisei-portable-*" -Directory |
             Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (-not $bundleDir) { throw 'No portable bundle directory found. Run make_bundle.ps1 first.' }

Write-Host "[2/2] Building installer with $iscc"
Write-Host "      Bundle source: $($bundleDir.FullName)"
& $iscc "/DBundleDir=$($bundleDir.FullName)" "/DBundleRoot=$OutDir" "$root\installer\AutoSuisei.iss"
if ($LASTEXITCODE -ne 0) { throw "iscc failed with exit $LASTEXITCODE" }

$out = Get-ChildItem "$OutDir\AutoSuisei-Setup-*.exe" |
       Sort-Object LastWriteTime -Descending | Select-Object -First 1
if ($out) {
    $mb = [math]::Round($out.Length / 1MB, 1)
    Write-Host ""
    Write-Host "Installer: $($out.FullName)  ($mb MB)"
}
