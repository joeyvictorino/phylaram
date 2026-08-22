@echo off
setlocal
echo ===============================================================================
echo                PhylaRAM - Test Certificate Installation
echo ===============================================================================
echo.

:: Check for Administrator privileges
net session >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [ERROR] This script must be run as Administrator.
    echo Please right-click install_test_cert.bat and select 'Run as administrator'.
    echo.
    pause
    exit /b 1
)

set "CERT_FILE=%~dp0PhylaRAM_TestCert.cer"
if not exist "%CERT_FILE%" (
    echo [ERROR] PhylaRAM_TestCert.cer not found in %~dp0
    pause
    exit /b 1
)

echo [1/2] Adding certificate to Trusted Root Certification Authorities...
certutil -addstore -f "Root" "%CERT_FILE%"
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Failed to add certificate to Root store.
    pause
    exit /b 1
)

echo [2/2] Adding certificate to Trusted Publishers...
certutil -addstore -f "TrustedPublisher" "%CERT_FILE%"
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Failed to add certificate to TrustedPublisher store.
    pause
    exit /b 1
)

echo.
echo ===============================================================================
echo [SUCCESS] PhylaRAM test certificate successfully installed!
echo.
echo NOTE: If you have not enabled test signing on this machine, run:
echo.
echo     bcdedit /set testsigning on
echo.
echo and restart Windows. Ensure Secure Boot is DISABLED in your UEFI/BIOS/VM settings.
echo ===============================================================================
echo.
pause
