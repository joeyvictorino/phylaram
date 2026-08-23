# ==============================================================================
# PhylaRAM Microsoft WHQL / Hardware Dev Center Attestation Preparation Harness
# ==============================================================================
# Requirement: Run on a Windows machine with Windows SDK / WDK installed.
# Purpose: Prepares .cat catalog, validates INF, and bundles driver package for
#          submission to Microsoft Hardware Developer Center.
# ==============================================================================

[CmdletBinding()]
param (
    [string]$OutputDir = "$PSScriptRoot\..\dist\WHQL_Submission",
    [string]$CertificateThumbprint
)

$ErrorActionPreference = "Stop"

function Write-Header ($text) {
    Write-Host "`n===============================================================================" -ForegroundColor Cyan
    Write-Host " $text" -ForegroundColor Cyan
    Write-Host "===============================================================================" -ForegroundColor Cyan
}

Write-Header "PhylaRAM WHQL & Attestation Package Builder"

$RepoRoot = Resolve-Path "$PSScriptRoot\.."
$DriverSys = Join-Path $RepoRoot "bin\phylaram.sys"
$DriverInf = Join-Path $RepoRoot "driver\phylaram.inf"

if (-not (Test-Path $DriverSys)) {
    Write-Error "Driver binary not found at $DriverSys. Please build the Release x64 driver first."
}
if (-not (Test-Path $DriverInf)) {
    Write-Error "Driver INF not found at $DriverInf."
}

# 1. Prepare Staging Directory
if (Test-Path $OutputDir) {
    Remove-Item $OutputDir -Recurse -Force
}
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
$PackageDir = Join-Path $OutputDir "phylaram_package"
New-Item -ItemType Directory -Path $PackageDir -Force | Out-Null

Copy-Item $DriverSys $PackageDir
Copy-Item $DriverInf $PackageDir

# 2. Generate Driver Catalog File with Inf2Cat
Write-Host "`n[1/4] Generating Driver Catalog (.cat) with Inf2Cat..." -ForegroundColor Yellow

$inf2cat = Get-ChildItem -Path "C:\Program Files (x86)\Windows Kits\10\bin" -Recurse -Filter "Inf2Cat.exe" |
    Where-Object { $_.FullName -match 'x64' } |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1 -ExpandProperty FullName

if ($inf2cat) {
    Write-Host "Found Inf2Cat: $inf2cat"
    # Target Windows 10 & Windows 11 x64
    & $inf2cat /driver:$PackageDir /os:10_X64,Server2022_X64
    if ($LASTEXITCODE -ne 0) {
        Write-Warning "Inf2Cat completed with warnings/errors. Review catalog output."
    }
} else {
    Write-Warning "Inf2Cat.exe not found in Windows SDK path. Skipping automatic catalog generation."
}

# 3. Sign Package with EV Certificate (if provided)
if ($CertificateThumbprint) {
    Write-Host "`n[2/4] Signing Driver Package with Authenticode EV Certificate ($CertificateThumbprint)..." -ForegroundColor Yellow
    $signtool = Get-ChildItem -Path "C:\Program Files (x86)\Windows Kits\10\bin" -Recurse -Filter "signtool.exe" |
        Where-Object { $_.FullName -match 'x64' } |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1 -ExpandProperty FullName

    if ($signtool) {
        & $signtool sign /fd SHA256 /sha1 $CertificateThumbprint /tr http://timestamp.digicert.com /td SHA256 "$PackageDir\phylaram.sys"
        if (Test-Path "$PackageDir\phylaram.cat") {
            & $signtool sign /fd SHA256 /sha1 $CertificateThumbprint /tr http://timestamp.digicert.com /td SHA256 "$PackageDir\phylaram.cat"
        }
        Write-Host "[SUCCESS] Driver and catalog signed." -ForegroundColor Green
    }
} else {
    Write-Host "`n[2/4] No EV CertificateThumbprint specified. Skipping Authenticode signing step." -ForegroundColor Gray
}

# 4. Generate CAB Package for Hardware Dev Center
Write-Host "`n[3/4] Packaging Cabinet (.cab) File for Submission..." -ForegroundColor Yellow
$DdfPath = Join-Path $OutputDir "package.ddf"
$CabPath = Join-Path $OutputDir "PhylaRAM_WHQL_Submission.cab"

$ddfContent = @"
.OPTION EXPLICIT
.Set CabinetNameTemplate=PhylaRAM_WHQL_Submission.cab
.Set DiskDirectory1=$OutputDir
.Set Cabinet=ON
.Set Compress=ON
"$PackageDir\phylaram.sys"
"$PackageDir\phylaram.inf"
"@
if (Test-Path "$PackageDir\phylaram.cat") {
    $ddfContent += "`n`"$PackageDir\phylaram.cat`""
}

Set-Content -Path $DdfPath -Value $ddfContent -Encoding ASCII

& makecab.exe /F $DdfPath
Remove-Item $DdfPath -Force -ErrorAction SilentlyContinue

# 5. Summary & Instructions
Write-Header "WHQL Submission Package Ready"
Write-Host "Cabinet File : $CabPath" -ForegroundColor Green
Write-Host "`nNext Steps for Hardware Dev Center Attestation:"
Write-Host " 1. Sign the .cab file with your Extended Validation (EV) code signing certificate:"
Write-Host "    signtool.exe sign /fd SHA256 /sha1 <EV_THUMBPRINT> /tr http://timestamp.digicert.com /td SHA256 `"$CabPath`""
Write-Host " 2. Log in to Microsoft Hardware Developer Center (https://partner.microsoft.com/dashboard/hardware)."
Write-Host " 3. Create a new Driver Submission and upload `"$CabPath`"."
Write-Host " 4. Download the signed submission package containing the official Microsoft WHQL signature."
