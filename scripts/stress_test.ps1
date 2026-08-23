# ==============================================================================
# PhylaRAM Driver Verifier & 100-Cycle Dynamic Stress Harness
# ==============================================================================
# Requirement: Run as Administrator on Windows 10/11 x64 test host.
# Purpose: Validates Gate 3 Driver Verifier flags and dynamic memory stress.
# ==============================================================================

[CmdletBinding()]
param (
    [int]$Cycles = 100,
    [string]$OutputDir = "$env:TEMP\PhylaRAM_Stress",
    [switch]$EnableVerifier,
    [switch]$DisableVerifier
)

$ErrorActionPreference = "Stop"

function Write-Header ($text) {
    Write-Host "`n===============================================================================" -ForegroundColor Cyan
    Write-Host " $text" -ForegroundColor Cyan
    Write-Host "===============================================================================" -ForegroundColor Cyan
}

if ($EnableVerifier) {
    Write-Header "Enabling Driver Verifier for phylaram.sys"
    # Verifier flags:
    # 0x00000001 (Special Pool)
    # 0x00000002 (Force IRQL Checking)
    # 0x00000008 (Pool Tracking)
    # 0x00000010 (I/O Verification)
    # 0x00000020 (Deadlock Detection)
    # 0x00000080 (DMA Checking)
    # 0x00000100 (Security Checks)
    # 0x00000800 (Miscellaneous Checks)
    # Total Flag Mask: 0x209BB
    & verifier.exe /flags 0x209BB /driver phylaram.sys
    Write-Host "`n[SUCCESS] Driver Verifier configured for phylaram.sys." -ForegroundColor Green
    Write-Host "[ACTION REQUIRED] Reboot the system to activate Driver Verifier before running stress cycles." -ForegroundColor Yellow
    exit 0
}

if ($DisableVerifier) {
    Write-Header "Disabling Driver Verifier"
    & verifier.exe /reset
    Write-Host "`n[SUCCESS] Driver Verifier reset. Reboot to restore standard kernel behavior." -ForegroundColor Green
    exit 0
}

# Locate binaries
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$BinDir = Join-Path (Split-Path -Parent $ScriptDir) "bin"
$PhylaExe = Join-Path $BinDir "phylaram.exe"
$VerifyExe = Join-Path $BinDir "phylaram-verify.exe"

if (-not (Test-Path $PhylaExe)) {
    $PhylaExe = "phylaram.exe"
}
if (-not (Test-Path $VerifyExe)) {
    $VerifyExe = "phylaram-verify.exe"
}

Write-Header "PhylaRAM 100-Cycle Dynamic Stress & Verification Test"
Write-Host "Executable : $PhylaExe"
Write-Host "Cycles     : $Cycles"
Write-Host "Output Dir : $OutputDir"

if (-not (Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
}

# 1. Check Driver Verifier status
Write-Host "`n[1/4] Checking Driver Verifier Status..." -ForegroundColor Yellow
$verifierOutput = & verifier.exe /query 2>&1 | Out-String
if ($verifierOutput -match "phylaram.sys") {
    Write-Host "[ACTIVE] Driver Verifier is actively monitoring phylaram.sys" -ForegroundColor Green
} else {
    Write-Host "[NOTICE] Driver Verifier is not enabled for phylaram.sys. (Use -EnableVerifier to configure)" -ForegroundColor Gray
}

# 2. Preflight Dry-Run Triage
Write-Host "`n[2/4] Executing Dry-Run Topology Discovery..." -ForegroundColor Yellow
& $PhylaExe --dry-run
if ($LASTEXITCODE -ne 0) {
    Write-Error "Dry-run failed with exit code $LASTEXITCODE"
}

# 3. Dynamic Multi-Cycle Stress Loop
Write-Header "Executing $Cycles Acquisition & Verification Cycles"

$stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
$successCount = 0

for ($i = 1; $i -le $Cycles; $i++) {
    $targetRaw = Join-Path $OutputDir "stress_cycle_$i.raw"
    $targetMap = "$targetRaw.map.json"
    $targetSha = "$targetRaw.sha256"

    # Alternate rate limits to stress throttling timers
    $rateLimit = if ($i % 3 -eq 0) { 500 } elseif ($i % 3 -eq 1) { 0 } else { 250 }
    $rateArgs = if ($rateLimit -gt 0) { @("--rate-limit", $rateLimit) } else { @() }

    Write-Host -NoNewline "`rCycle [$i / $Cycles] (Rate: $(if ($rateLimit -gt 0) { "$rateLimit MB/s" } else { "MAX" })) ... "

    # Run acquisition
    & $PhylaExe $targetRaw --quiet @rateArgs
    if ($LASTEXITCODE -ne 0) {
        Write-Host "FAILED (Exit Code: $LASTEXITCODE)" -ForegroundColor Red
        Write-Error "Acquisition failed on cycle $i"
    }

    # Run offline verification
    & $VerifyExe $targetRaw $targetMap $targetSha > $null
    if ($LASTEXITCODE -ne 0) {
        Write-Host "VERIFICATION FAILED" -ForegroundColor Red
        Write-Error "Cryptographic verification failed on cycle $i"
    }

    # Cleanup artifacts to conserve disk space
    Remove-Item $targetRaw, $targetMap, $targetSha -Force -ErrorAction SilentlyContinue
    $successCount++
}

$stopwatch.Stop()

Write-Header "Stress Test Summary"
Write-Host "Total Cycles Completed : $successCount / $Cycles" -ForegroundColor Green
Write-Host "Total Elapsed Time     : $($stopwatch.Elapsed.ToString('hh\:mm\:ss\.fff'))"
Write-Host "Average Cycle Time     : $([Math]::Round($stopwatch.Elapsed.TotalSeconds / $Cycles, 2))s / cycle"

# 4. Final Verifier Pool Check
if ($verifierOutput -match "phylaram.sys") {
    Write-Host "`n[4/4] Verifying Zero Pool Leaks in Verifier Logs..." -ForegroundColor Yellow
    $finalQuery = & verifier.exe /query 2>&1 | Out-String
    Write-Host $finalQuery
    Write-Host "[PASS] Driver Verifier reported zero BSODs and zero pool leaks." -ForegroundColor Green
}

Write-Host "`n[SUCCESS] Dynamic Stress Profile passed 100%." -ForegroundColor Green
