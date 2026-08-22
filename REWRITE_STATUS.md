# PhylaRAM Phase 2 Rewrite Status

**Brand:** PhylaRAM  
**Executable:** `phylaram.exe`  
**Driver:** `phylaram.sys`  
**Standard:** `ENGINEERING_STANDARD.md`  

---

## File Tracking Table

| Path | Language | Original State | Action | Reason | Implementation Status | Local Validation | Windows Validation Required |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| `shared/phylaram.h` | C / C++ | `shared/urc.h` | **REWRITE / RENAME** | Rename to `PHYLA_` namespace, fixed-width types, ABI static assertions | **COMPLETED** | Verified size/offsets | Static driver ABI verification |
| `shared/interfaces.hpp` | C++20 | None | **NEW** | Decoupled abstract interfaces for testability & portable execution | **COMPLETED** | Clang C++20 clean build | MSVC compile |
| `driver/driver.h` | C | `driver/driver.h` | **REWRITE** | Update `PHYLA_` prototypes, `PHYLA_FILE_CONTEXT`, idempotent cleanup | **COMPLETED** | Syntax audit | WDK / KMDF build |
| `driver/driver.c` | C | `driver/driver.c` | **REWRITE** | Control device, `WdfDeviceInitFree` ownership on failure, `EvtFileCleanup` | **COMPLETED** | Lifecycle review | KMDF runtime / Driver Verifier |
| `driver/session.c` | C | `driver/session.c` | **REWRITE** | `MmGetPhysicalMemoryRangesEx2`, idempotent `PhylaSessionRelease`, diffing | **COMPLETED** | Range math audit | Driver Verifier Special Pool |
| `driver/memory.c` | C | `driver/memory.c` | **REWRITE** | `MmCopyMemory(MM_COPY_MEMORY_PHYSICAL)`, bounds checks, exact partial bytes | **COMPLETED** | Arithmetic checked | High-load RAM copy test |
| `driver/ioctl.c` | C | `driver/ioctl.c` | **REWRITE** | `METHOD_OUT_DIRECT`, `WdfRequestRetrieveOutputWdmMdl(Request, &mdl)`, zero info | **COMPLETED** | Direct I/O audit | Direct I/O transfer test |
| `driver/phylaram.vcxproj` | MSBuild | `driver/urc.vcxproj` | **REWRITE / RENAME** | WDK build config, `EnableInf2cat=false`, `/guard:cf`, `/Qspectre`, C17 | **COMPLETED** | XML validated | MSBuild Release\|x64 |
| `cli/phylaram.hpp` | C++20 | `cli/urc.hpp` | **REWRITE / RENAME** | RAII `UniqueWin32Handle`, Rule of 5/0, reusable I/O buffer | **COMPLETED** | C++20 clean build | MSVC C++20 build |
| `cli/driver_resource.cpp` | C++20 | `cli/driver_resource.cpp`| **REWRITE** | Secure `%ProgramData%\PhylaRAM\Temp\`, restricted SDDL, unpredictable name | **COMPLETED** | SDDL audit | Win32 DACL test |
| `cli/driver_service.cpp` | C++20 | `cli/driver_service.cpp` | **REWRITE** | SCM demand start, quoted binary path, steady_clock, RAII handles | **COMPLETED** | CWE-428 check | SCM load/unload test |
| `cli/device.cpp` | C++20 | `cli/device.cpp` | **REWRITE** | `PhylaSession`, reusable transfer buffer, error propagation | **COMPLETED** | Buffer reuse verified | DeviceIoControl test |
| `cli/raw_writer.cpp` | C++20 | `cli/raw_writer.cpp` | **REWRITE** | Sparse/dense preflight, volume capability check, chunked write | **COMPLETED** | Preflight tested | NTFS / ReFS sparse test |
| `cli/sha256.cpp` | C++20 | `cli/sha256.cpp` | **REWRITE** | Windows CNG BCrypt SHA-256, `UpdateZeros` | **COMPLETED** | Move semantics check | BCrypt on Windows |
| `cli/acquire.cpp` | C++20 | `cli/acquire.cpp` | **REWRITE** | 16MB fast path, 4KB page isolation state machine, partial read handling | **COMPLETED** | Mock fault suite (PASS) | Live RAM acquisition test |
| `cli/map.cpp` | C++20 | `cli/map.cpp` | **REWRITE** | `phylaram-map-1` schema, atomic sidecar writing, accurate status | **COMPLETED** | Schema tests (PASS) | Windows filesystem test |
| `cli/main.cpp` | C++20 | `cli/main.cpp` | **REWRITE** | 6-file preflight check, elevation, dynamic RtlGetVersion, atomic promotion | **COMPLETED** | Parser tests (PASS) | Windows elevation test |
| `cli/resource.h` | C / C++ | `cli/resource.h` | **REWRITE** | IDR_PHYLA_DRIVER resource ID | **COMPLETED** | Clean include | RC compiler |
| `cli/phylaram.rc` | RC | `cli/urc.rc` | **REWRITE / RENAME** | Embeds `..\bin\phylaram.sys` | **COMPLETED** | Syntax validated | RC compile |
| `cli/phylaram.manifest` | XML | `cli/urc.manifest` | **REWRITE / RENAME** | `requireAdministrator`, Windows 10/11 compatibility GUID | **COMPLETED** | XML validated | MSBuild manifest embed |
| `cli/phylaram.vcxproj` | MSBuild | `cli/urc.vcxproj` | **REWRITE / RENAME** | `/std:c++20`, `/guard:cf`, `/permissive-`, manifest embed, no `ntdll.lib` | **COMPLETED** | XML validated | MSBuild Release\|x64 |
| `PhylaRAM.sln` | Solution | `URC.sln` | **REWRITE / RENAME** | Visual Studio 2022 solution for PhylaRAM | **COMPLETED** | Structure validated | Visual Studio 2022 |
| `tests/CMakeLists.txt` | CMake | None | **NEW** | Portable test build for macOS / Linux | **COMPLETED** | CMake spec ready | N/A |
| `tests/test_range_algebra.cpp` | C++20 | None | **NEW** | Unit tests for 64-bit range algebra, sorting, overflow | **COMPLETED** | Passed (10/10 tests) | N/A |
| `tests/test_mock_acquire.cpp` | C++20 | None | **NEW** | Mock acquisition engine fault-injection tests | **COMPLETED** | Passed (6/6 scenarios) | N/A |
| `tests/test_map_json.cpp` | C++20 | None | **NEW** | `phylaram-map-1` JSON serialization tests | **COMPLETED** | Passed (3/3 contracts) | N/A |
| `tests/test_cli_parser.cpp` | C++20 | None | **NEW** | CLI argument parsing & validation tests | **COMPLETED** | Passed (6/6 cases) | N/A |
| `tools/phylaram-verify/` | Rust | None | **NEW** | Offline verification crate & proptest suites | **COMPLETED** | Passed (cargo test & clippy) | Cargo on Windows |
| `tests/TEST_PLAN.md` | Markdown | `tests/TEST_PLAN.md` | **REWRITE** | Formal acceptance plan & 6 Windows validation gates | **COMPLETED** | Documented | Test gate execution |
| `README.md` | Markdown | `README.md` | **REWRITE** | SANS-ready tool presentation | **COMPLETED** | Rendered | Documentation |
| `THIRD_PARTY_NOTICES.md` | Markdown | `THIRD_PARTY_NOTICES.md` | **REWRITE** | PhylaRAM references | **COMPLETED** | Rendered | Legal |
| `LICENSE` | Text | `LICENSE` | **REWRITE** | PhylaRAM copyright | **COMPLETED** | Text formatted | Legal |
| `ENGINEERING_STANDARD.md`| Markdown | None | **NEW** | Canonical normative engineering standard | **COMPLETED** | Normative standard | Review |
| `docs/SANS_FOR500_FOR508_GUIDE.md` | Markdown | None | **NEW** | SANS FOR500/FOR508 Instructor & Lab Guide | **COMPLETED** | Rendered | Documentation |
| `docs/PHYLARAM_GUI_AND_VISUAL_SPEC.md` | Markdown | None | **NEW** | Dual-Mode & Visual Identity Specification | **COMPLETED** | Rendered | UX Architecture |
| `docs/BEATING_COMMERCIAL_TOOLS_AND_PRODUCT_STRATEGY.md` | Markdown | None | **NEW** | Strategy & Commercial Tools Comparison | **COMPLETED** | Rendered | Strategic Whitepaper |


