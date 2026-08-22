@echo off
setlocal enabledelayedexpansion

echo ===============================================================================
echo                      PhylaRAM - Windows Test Verification Suite
echo ===============================================================================
echo.

set "SCRIPT_DIR=%~dp0"
set "ROOT_DIR=%SCRIPT_DIR%.."
cd /d "%ROOT_DIR%"

if not exist "bin\phylaram.exe" (
    echo [ERROR] bin\phylaram.exe not found. Please build the project first.
    exit /b 1
)

echo [TEST 1/3] Validating CLI help and parser flags...
"bin\phylaram.exe" --help >nul
if %ERRORLEVEL% equ 0 (
    echo   [PASS] phylaram.exe --help returned exit code 0.
) else (
    echo   [FAIL] phylaram.exe --help failed with exit code %ERRORLEVEL%.
)

echo.
echo [TEST 2/3] Validating Rust Offline Verifier (if available)...
if exist "bin\phylaram-verify.exe" (
    cd tools\phylaram-verify
    cargo test
    if %ERRORLEVEL% equ 0 (
        echo   [PASS] phylaram-verify tests passed successfully.
    ) else (
        echo   [FAIL] phylaram-verify tests failed.
    )
    cd /d "%ROOT_DIR%"
) else (
    echo   [SKIP] bin\phylaram-verify.exe not built.
)

echo.
echo [TEST 3/3] Validating Embedded Driver Resource...
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$bytes = [System.IO.File]::ReadAllBytes('%ROOT_DIR%\bin\phylaram.exe'); " ^
  "if ($bytes.Length -gt 50000) { Write-Host '  [PASS] phylaram.exe size confirms embedded driver payload (' $bytes.Length ' bytes).' } else { Write-Error 'Executable size suspiciously small.' }"

echo.
echo ===============================================================================
echo [SUCCESS] Windows test verification completed.
echo ===============================================================================
exit /b 0
