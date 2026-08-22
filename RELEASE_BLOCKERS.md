# PhylaRAM Release Blockers & Windows Validation Matrix

**Brand:** PhylaRAM  
**Version:** 1.0.0 (Release Freeze)  
**Standard:** `ENGINEERING_STANDARD.md`  

This document tracks items that **genuinely require a live Windows x64 host with Visual Studio 2022, the Windows Driver Kit (WDK), EV Code Signing, and kernel test environments**. All local code architecture, C++20 unit tests, fault-injection state machines, Rust offline verifiers, and property test suites have completed and passed 100% on the local workstation.

---

## Windows-Only Release Blockers

| Gate | Validation Requirement | Environment / Tool | Pass Criteria | Status |
| :--- | :--- | :--- | :--- | :--- |
| **Gate 1** | **MSVC & WDK Compilation** | Windows 10/11 x64, VS 2022, WDK 10.0.22621+ | Build `Release\|x64` in `PhylaRAM.sln` with zero warnings treated as errors (`/W4 /WX /guard:cf /Qspectre /CETCompat`). Produces `bin\phylaram.sys` and `bin\phylaram.exe`. | **BLOCKED ON WINDOWS TOOLCHAIN** |
| **Gate 2** | **Static Driver Verifier (SDV) & CodeQL** | Windows SDK & WDK Static Tools | Static Driver Verifier (SDV) rule suite passes. CodeQL Windows Driver queries return zero alerts. | **BLOCKED ON WINDOWS TOOLCHAIN** |
| **Gate 3** | **Driver Verifier Dynamic Stress Profile** | Windows 10/11 VM with Driver Verifier active | 100 consecutive acquisition cycles with Special Pool, Force IRQL Checking, Pool Tracking, and I/O Verification enabled. Zero bugchecks, zero pool leaks. | **BLOCKED ON RUNTIME VM** |
| **Gate 4** | **Hardware & Memory Topology Matrix** | Physical hardware & Hyper-V VMs | Validated on 4 GB, 16 GB, 64 GB (large PCIe MMIO / ReBAR holes), and 128 GB+ RAM configurations. NTFS/ReFS sparse files match physical allocation. | **BLOCKED ON HARDWARE LAB** |
| **Gate 5** | **Windows Security Hardening Profile** | Windows 11 Enterprise | Production EV code-signed binary executes with Secure Boot, Virtualization-Based Security (VBS), HVCI (Memory Integrity), and Windows Defender active. | **BLOCKED ON EV CODE SIGNING** |
| **Gate 6** | **Forensic Interoperability Validation** | Volatility 3, MemProcFS | Flat `memory.raw` parses cleanly in Volatility 3 (`windows.info`, `windows.pslist`) and MemProcFS without symbol or physical offset mismatch. | **BLOCKED ON MEMORY FIXTURES** |

---

## Local Verification Status (Completed on macOS)

- [x] **Range Algebra & Integer Overflows:** Passed 10/10 test cases in `test_range_algebra` (sorting, overlap rejection, `UINT64_MAX` boundary, 256 GiB scale).
- [x] **Mock Acquisition Engine & Fault Injection:** Passed 6/6 test scenarios in `test_mock_acquire` (16 MiB fast path, 4 KiB page isolation, short-read preservation, contiguous bad-page coalescing, cancellation, topology change).
- [x] **Provenance Map Schema:** Passed 3/3 contract tests in `test_map_json` (`phylaram-map-2` schema, `kernel_hints` serialization, uppercase hex formatting, terminal status consistency).
- [x] **CLI Parser:** Passed 7/7 test cases in `test_cli_parser` (flags, `--rate-limit`, `--with-pagefile`, stdout `-`, help, unknown argument rejection).
- [x] **Rust Verifier (`phylaram-verify`):** `cargo fmt`, `cargo clippy -- -D warnings`, unit tests, e2e bundle verification, and `proptest` property tests all passed cleanly across `phylaram-map-1` and `phylaram-map-2`.
- [x] **Static Prohibited API Audit:** Zero occurrences of banned APIs (`MmMapIoSpace`, `NtLoadDriver`, `NtUnloadDriver`, `\\Device\\PhysicalMemory`, `METHOD_NEITHER`, `FILE_ANY_ACCESS`, `MAX_MEMORY_RUNS`).
- [x] **Brand Audit:** Zero stale `URC` references in production source, project files, or schemas.
