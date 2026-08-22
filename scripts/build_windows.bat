@echo off
setlocal enabledelayedexpansion

echo ===============================================================================
echo                      PhylaRAM - Automated Windows Build System
echo ===============================================================================
echo.

set "SCRIPT_DIR=%~dp0"
set "ROOT_DIR=%SCRIPT_DIR%.."
cd /d "%ROOT_DIR%"

:: 1. Locate Visual Studio 2022 via vswhere
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo [ERROR] vswhere.exe not found at "%VSWHERE%".
    echo Please install Visual Studio 2022 with C++ Desktop and WDK development tools.
    exit /b 1
)

for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do (
    set "MSBUILD=%%i"
)

if not defined MSBUILD (
    echo [ERROR] MSBuild.exe could not be located.
    exit /b 1
)

echo [INFO] Found MSBuild: "%MSBUILD%"
echo.

:: 2. Ensure output directories exist
if not exist "bin" mkdir "bin"
if not exist "dist\PhylaRAM-v1.0-x64" mkdir "dist\PhylaRAM-v1.0-x64"

:: 3. Build PhylaRAM Solution (Driver + CLI)
echo [BUILD] Building PhylaRAM.sln (Release ^| x64)...
"%MSBUILD%" PhylaRAM.sln /p:Configuration=Release /p:Platform=x64 /p:WarningLevel=4 /p:TreatWarningAsError=true /v:minimal /m

if %ERRORLEVEL% neq 0 (
    echo.
    echo [ERROR] Visual Studio build failed with exit code %ERRORLEVEL%.
    exit /b %ERRORLEVEL%
)

echo [SUCCESS] Solution built successfully.
echo.

:: 4. Build Rust Offline Verifier if cargo is available
where cargo >nul 2>&1
if %ERRORLEVEL% equ 0 (
    echo [BUILD] Building tools\phylaram-verify with Cargo (Release)...
    cd tools\phylaram-verify
    cargo build --release
    if %ERRORLEVEL% equ 0 (
        copy /y target\release\phylaram-verify.exe "..\..\bin\phylaram-verify.exe" >nul
        echo [SUCCESS] phylaram-verify.exe built and copied to bin\.
    ) else (
        echo [WARNING] Cargo build encountered issues.
    )
    cd /d "%ROOT_DIR%"
) else (
    echo [NOTE] Cargo not found in PATH. Skipping phylaram-verify build.
)
echo.

:: 5. Package distribution
echo [PACKAGE] Assembling distribution package in dist\PhylaRAM-v1.0-x64\...
copy /y bin\phylaram.exe dist\PhylaRAM-v1.0-x64\ >nul
if exist bin\phylaram-verify.exe copy /y bin\phylaram-verify.exe dist\PhylaRAM-v1.0-x64\ >nul
copy /y README.md dist\PhylaRAM-v1.0-x64\README.txt >nul
copy /y LICENSE dist\PhylaRAM-v1.0-x64\LICENSE.txt >nul

echo.
echo ===============================================================================
echo [SUCCESS] PhylaRAM build and packaging completed successfully!
echo Binary output directory:  %ROOT_DIR%\bin\
echo Distribution package:     %ROOT_DIR%\dist\PhylaRAM-v1.0-x64\
echo.
echo Usage:
echo   dist\PhylaRAM-v1.0-x64\phylaram.exe memory.raw
echo ===============================================================================
exit /b 0
