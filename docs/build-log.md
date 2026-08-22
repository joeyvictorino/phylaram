# PhylaRAM Build Log

## CI Run #32592111442 — PhylaRAM Continuous Integration
**Trigger:** push to `main`
**Runner:** `windows-2022`
**Result:** ❌ FAILURE

### Failed Step: Build Driver & CLI (Release | x64)
```
MSBuild version 17.14.51+25f168cee for .NET Framework
  driver.c
  session.c
  ioctl.c
  memory.c
driver\driver.h(3,1): error C1083: Cannot open include file: 'ntddk.h': No such file or directory [driver\phylaram.vcxproj]
  (compiling source file 'driver.c')
driver\driver.h(3,1): error C1083: Cannot open include file: 'ntddk.h': No such file or directory [driver\phylaram.vcxproj]
  (compiling source file 'session.c')
driver\driver.h(3,1): error C1083: Cannot open include file: 'ntddk.h': No such file or directory [driver\phylaram.vcxproj]
  (compiling source file 'ioctl.c')
driver\driver.h(3,1): error C1083: Cannot open include file: 'ntddk.h': No such file or directory [driver\phylaram.vcxproj]
  (compiling source file 'memory.c')
```

### Root Cause
The `windows-2022` GitHub Actions runner includes Visual Studio 2022 and the Windows SDK, but does **not** include the Windows Driver Kit (WDK). The driver project (`driver/phylaram.vcxproj`) uses `PlatformToolset=WindowsKernelModeDriver10.0` which requires WDK targets, headers (`ntddk.h`, `wdf.h`), and libraries.

### Fix Applied
Adopted the [microsoft/Windows-driver-samples](https://github.com/microsoft/Windows-driver-samples) NuGet-based WDK pattern:
1. Added `packages.config` referencing `Microsoft.Windows.WDK.x64` and `Microsoft.Windows.SDK.CPP.x64`
2. Added `Directory.Build.props` to import WDK/SDK targets from NuGet packages
3. Updated CI workflow to run `nuget restore` before `msbuild`

---

## Release Run #32592120450 — PhylaRAM Release Build
**Trigger:** tag `v1.0.0` push
**Runner:** `windows-2022`
**Result:** ❌ FAILURE

Identical root cause to CI run above. Same `error C1083: Cannot open include file: 'ntddk.h'` across all 4 driver source files.

---

## CI Run #32599238612 — PhylaRAM Continuous Integration
**Trigger:** push to `main` (NuGet WDK & test-signing integration)
**Runner:** `windows-2022`
**Result:** ❌ FAILURE (driver compilation)

### Passed Steps:
- `✓ Portable C++20 & Rust Tests (macOS)` (41s)
- `✓ Setup MSBuild`
- `✓ Setup NuGet`
- `✓ Install WDK (NuGet)` — Successfully restored `Microsoft.Windows.WDK.x64` and `Microsoft.Windows.SDK.CPP.x64` (10.0.26100.1)
- `✓ Setup Rust`

### Failed Step: Build Driver (Release | x64)
- `session.c`: `KAPC_STATE`, `KeStackAttachProcess`, `KeUnstackDetachProcess` missing header declarations (requires `<ntifs.h>`).
- `session.c`: `IMAGE_DOS_HEADER`, `IMAGE_NT_HEADERS`, `RtlPcToFileHeader` missing `<ntimage.h>`.
- `session.c`: `ZwYieldExecution` missing explicit forward declaration.
- `ioctl.c`: `SIZE_MAX` undeclared in C17 mode (replaced with `((size_t)-1)`).

### Fix Applied
1. Included `<ntifs.h>` and `<ntimage.h>` in `driver/driver.h`.
2. Declared `NTSYSAPI NTSTATUS NTAPI ZwYieldExecution(VOID);` in `driver/driver.h`.
3. Replaced `SIZE_MAX` with `(((size_t)-1) / sizeof(PHYLA_MEMORY_RUN))` in `driver/ioctl.c`.

---

## CI Run #32599373379 — PhylaRAM Continuous Integration
**Trigger:** push to `main`
**Runner:** `windows-2022`
**Result:** ❌ FAILURE (driver compilation)

### Failed Step: Build Driver (Release | x64)
- `session.c(194)`: `error C4013: 'RtlPcToFileHeader' undefined; assuming extern returning int` (compiler warnings treated as errors under `/WX`).

### Fix Applied
Declared `NTSYSAPI PVOID NTAPI RtlPcToFileHeader(_In_ PVOID PcValue, _Out_ PVOID *BaseOfImage);` in `driver/driver.h`.

---

## CI Run #32599473508 — PhylaRAM Continuous Integration
**Trigger:** push to `main`
**Runner:** `windows-2022`
**Result:** ❌ FAILURE (MSBuild SignTool step)

### Progress:
- All 4 driver C files (`driver.c`, `session.c`, `ioctl.c`, `memory.c`) compiled and linked **100% cleanly**!
- `phylaram.sys` generated successfully.

### Failed Step: MSBuild Driver SignTask
- `SIGNTASK : SignTool error : No file digest algorithm specified.` (Default WDK project test-signing task failed because digest algorithm was not configured in project properties).

### Fix Applied
1. Added `<SignMode>Off</SignMode>` to `driver/phylaram.vcxproj` and `/p:SignMode=Off` to CI workflow (signing is handled explicitly by the dedicated PowerShell + `signtool.exe` test-signing step).
2. Fixed `OutDir` in `driver/phylaram.vcxproj` and `cli/phylaram.vcxproj` to `$(ProjectDir)..\bin\` ensuring correct binary artifact placement for resource embedding.

---

## CI Run #32599596570 — PhylaRAM Continuous Integration
**Trigger:** push to `main`
**Runner:** `windows-2022`
**Result:** ❌ FAILURE (CLI linker step)

### Progress:
- `✓ Build Driver (Release | x64)` **PASSED completely!** `phylaram.sys` generated cleanly.
- All CLI C++ translation units (`main.cpp`, `device.cpp`, `driver_resource.cpp`, `driver_service.cpp`, `raw_writer.cpp`, `sha256.cpp`, `map.cpp`, `acquire.cpp`) compiled cleanly with `/W4 /WX /guard:cf /std:c++20`.

### Failed Step: Build CLI (Release | x64)
- `LINK : fatal error LNK1181: cannot open input file 'bcrypt.lib'` (Unconditional WDK NuGet props import in `Directory.Build.props` overrode user-mode SDK library directories for non-driver projects).

### Fix Applied
1. Moved WDK props import directly into `driver/phylaram.vcxproj` so it is unconditionally applied during driver build without depending on early property evaluation in `Directory.Build.props`.
2. Emptied `Directory.Build.props` to ensure user-mode CLI build is never polluted by kernel headers or library paths.
3. Trimmed `packages.config` to `Microsoft.Windows.WDK.x64` only.

---

## CI Run #32599716320 — PhylaRAM Continuous Integration
**Trigger:** push to `main`
**Runner:** `windows-2022`
**Result:** ❌ FAILURE (driver compilation)

### Root Cause:
Conditioning `Directory.Build.props` on `$(ConfigurationType)` failed because `Directory.Build.props` is evaluated by MSBuild before the project's internal `<PropertyGroup Label="Configuration">` is parsed, causing `ConfigurationType` to be empty and WDK props to not be imported.

### Fix Applied:
Directly imported `Microsoft.Windows.WDK.x64.props` within `driver/phylaram.vcxproj` and kept `Directory.Build.props` empty, cleanly isolating kernel toolsets to the driver while leaving the user-mode CLI toolchain completely standard.





