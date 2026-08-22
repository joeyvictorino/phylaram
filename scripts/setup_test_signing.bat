@echo off
setlocal enabledelayedexpansion

echo ===============================================================================
echo                PhylaRAM - Windows Test Signing & Driver Certificate Setup
echo ===============================================================================
echo.
echo This script creates a local test certificate and signs bin\phylaram.sys for
echo execution on a dedicated Windows test VM (with TESTSIGNING enabled).
echo.

set "SCRIPT_DIR=%~dp0"
set "ROOT_DIR=%SCRIPT_DIR%.."
cd /d "%ROOT_DIR%"

if not exist "bin\phylaram.sys" (
    echo [ERROR] bin\phylaram.sys does not exist. Please run scripts\build_windows.bat first.
    exit /b 1
)

:: Check for Administrator privileges
net session >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [ERROR] This script must be run as Administrator to install test certificates.
    exit /b 1
)

echo [1/4] Generating local test certificate using PowerShell...
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject 'CN=PhylaRAM Test Certificate' -CertStoreLocation 'Cert:\LocalMachine\My' -HashAlgorithm 'SHA256'; " ^
  "Export-Certificate -Cert $cert -FilePath '%ROOT_DIR%\bin\PhylaRAM_TestCert.cer' | Out-Null; " ^
  "Write-Host '[SUCCESS] Certificate generated: bin\PhylaRAM_TestCert.cer'"

if not exist "bin\PhylaRAM_TestCert.cer" (
    echo [ERROR] Failed to export test certificate.
    exit /b 1
)

echo [2/4] Installing certificate to Trusted Root and Trusted Publishers...
certutil -addstore "Root" "bin\PhylaRAM_TestCert.cer" >nul
certutil -addstore "TrustedPublisher" "bin\PhylaRAM_TestCert.cer" >nul

echo [3/4] Signing bin\phylaram.sys with SignTool...
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$cert = Get-ChildItem -Path Cert:\LocalMachine\My | Where-Object { $_.Subject -match 'PhylaRAM Test Certificate' } | Select-Object -First 1; " ^
  "if ($cert) { " ^
  "  Set-AuthenticodeSignature -FilePath '%ROOT_DIR%\bin\phylaram.sys' -Certificate $cert -TimestampServer 'http://timestamp.digicert.com' | Out-Null; " ^
  "  Write-Host '[SUCCESS] Driver signed successfully.' " ^
  "} else { Write-Error 'Certificate not found in store.' }"

echo [4/4] Verifying signature on bin\phylaram.sys...
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "Get-AuthenticodeSignature '%ROOT_DIR%\bin\phylaram.sys' | Format-List"

echo.
echo ===============================================================================
echo [NOTE] If you are running on a test machine without an EV code signing cert,
echo enable test signing by opening an elevated Command Prompt and running:
echo.
echo     bcdedit /set testsigning on
echo.
echo Then reboot the machine before executing phylaram.exe.
echo ===============================================================================
exit /b 0
