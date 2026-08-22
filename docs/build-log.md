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
