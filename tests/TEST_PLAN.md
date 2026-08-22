# PhylaRAM Acceptance Test Plan & Validation Gates

**Brand:** PhylaRAM  
**Target:** Windows 10 version 2004+ / Windows 11 (x64)  
**Standard:** `ENGINEERING_STANDARD.md`  

---

## 1. Local Portable Validation (macOS / Linux)

These tests are executed locally before submitting changes or running Windows validation gates:

### Test Suite Summary
- **Suite 1: Range Algebra (`test_range_algebra`)**
  - Validates 64-bit integer overflow protection at `UINT64_MAX`.
  - Rejects overlapping ranges, zero-length ranges, and backwards ranges.
  - Tests massive scales: >64 runs, 4 GiB+, 64 GiB+, 128 GiB+, 256 GiB+.
- **Suite 2: Mock Acquisition Engine (`test_mock_acquire`)**
  - 16 MiB fast-path bulk transfer.
  - 16 MiB + 1 byte run boundary crossing.
  - Bad page isolation (4 KiB) and resumption of 16 MiB fast path.
  - Contiguous bad page span coalescing.
  - Real acquired all-zero page accounting vs unreadable span accounting.
  - Dynamic memory topology mutation detection at session end.
  - Ctrl+C cancellation handling with clean `.partial` state.
- **Suite 3: Provenance Map Schema (`test_map_json`)**
  - Schema `phylaram-map-1` compliance.
  - Correct uppercase hex address formatting (`0x...`).
  - Terminal status consistency (`complete` vs `incomplete` vs `failed`).
- **Suite 4: CLI Parser (`test_cli_parser`)**
  - Flag parsing (`--quiet`, `--no-hash`, `--help`, `-h`).
  - Strict rejection of unknown flags and multiple output targets.
- **Suite 5: Rust Offline Verifier (`phylaram-verify`)**
  - Independent parsing, schema validation, logical SHA-256 calculation.
  - `proptest` property-based invariant testing.

---

## 2. Windows Acceptance Validation Gates

The following 6 gates MUST be executed on dedicated Windows 10/11 x64 physical and virtual machines prior to public release:

### Gate 1: Strict Toolchain Build
- **Target:** Visual Studio 2022 + WDK (10.0.22621+).
- **Driver Flags:** C17 standard, `/W4`, `/WX`, `/guard:cf`, `/Qspectre`, `EnableInf2cat=false`.
- **CLI Flags:** C++20 standard, `/W4`, `/WX`, `/guard:cf`, `/CETCompat`, `/permissive-`.
- **Pass Criteria:** Zero warnings, zero errors in Release|x64 build producing `bin\phylaram.sys` and `bin\phylaram.exe`.

### Gate 2: Static Driver Analysis
- **Tools:** Static Driver Verifier (SDV) & CodeQL Driver Suite.
- **Pass Criteria:** All KMDF rules pass (including `ControlDeviceDeletedOnFailedInit`, `MdlZeroLengthCheck`, `IrqlPassive`, `PagedCode`).

### Gate 3: Dynamic Driver Verifier Stress Profile
- **Environment:** Dedicated Windows test VM with Driver Verifier active.
- **Verifier Flags:** Special Pool, Force IRQL Checking, Pool Tracking, I/O Verification, Deadlock Detection, Security Checks, Miscellaneous Checks.
- **Pass Criteria:** 100 consecutive acquisition cycles (`phylaram.exe test.raw`) with zero bugchecks, zero pool leaks, and zero IRQL violations.

### Gate 4: Hardware & RAM Topology Matrix
- **Topologies:**
  - 4 GB RAM (single NUMA node).
  - 16 GB RAM (dual-channel).
  - 64 GB RAM (large PCIe MMIO / ReBAR holes).
  - 128 GB+ RAM (multi-socket / enterprise workstation).
- **Filesystems:** NTFS (sparse), ReFS (sparse), FAT32 (non-sparse, verifies 4GB rejection), exFAT (dense fallback).
- **Pass Criteria:** `file offset == physical address` maintained; sparse file allocation matches `physical_bytes`.

### Gate 5: Windows Security Controls Gate
- **Controls:** Secure Boot enabled, Virtualization-Based Security (VBS) enabled, Hypervisor-Protected Code Integrity (HVCI / Memory Integrity) enabled, Windows Defender Real-Time Protection active.
- **Pass Criteria:** Driver loads and executes cleanly without requiring any security degradation or test-signing.

### Gate 6: Forensic Tool Interoperability
- **Tools:** Volatility 3 (`windows.info`, `windows.pslist`, `windows.malfind`), MemProcFS.
- **Pass Criteria:** Acquired `memory.raw` images parse cleanly with zero symbol resolution errors or physical alignment discrepancies.
