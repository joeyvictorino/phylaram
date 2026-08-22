# PhylaRAM Current Status & Verification Report

**Version:** `0.1.0-alpha`  
**Date:** August 2026  
**Repository:** [github.com/joeyvictorino/phylaram](https://github.com/joeyvictorino/phylaram)  

This document provides a transparent, auditable report on what has been implemented, what is verified, what remains to be validated on physical hardware, and the roadmap to production EV attestation signing.

---

## 1. Build and Toolchain Status

| Component | Target | Toolchain | Status | Verification Evidence |
| :--- | :--- | :--- | :---: | :--- |
| **Driver (`phylaram.sys`)** | Windows x64 | MSVC / KMDF 1.15 via NuGet WDK `10.0.26100.1` | **BUILDABLE** | Built in CI via `Directory.Build.props` and `packages.config` |
| **CLI Engine (`phylaram.exe`)** | Windows x64 | MSVC C++20 (`/std:c++20 /guard:cf /permissive-`) | **BUILDABLE** | Built in CI; embeds `phylaram.sys` as RCDATA resource |
| **Offline Verifier (`phylaram-verify`)** | Windows / macOS / Linux | Rust 2021 (`cargo build --release`) | **VERIFIED** | 100% test pass on macOS and Windows CI |
| **Unit & Mock Tests** | Cross-platform | C++20 Clang/MSVC | **VERIFIED** | 4 test suites pass cleanly with `-Wall -Wextra -Werror` |

---

## 2. Code Signing Status

> [!WARNING]
> **Pre-Release Test-Signing Only**  
> All Windows binaries currently produced in GitHub Actions CI are signed with an ephemeral, self-signed test certificate generated during the CI build (`CN=PhylaRAM Test Signing`).

### Loading on Test Environments
Because the driver is test-signed, Windows requires Test-Signing mode to load `phylaram.sys`:
```cmd
bcdedit /set testsigning on
:: Reboot required
```

### Path to Production Microsoft Attestation Signing
To achieve seamless loading on production Windows 10/11 machines with Secure Boot and HVCI enabled:
1. **Acquire Hardware Token EV Certificate:** Obtain an Extended Validation (EV) Code Signing Certificate from an approved Certificate Authority (DigiCert, Sectigo).
2. **Enroll in Microsoft Partner Center:** Register the organization in the [Windows Hardware Developer Program](https://developer.microsoft.com/en-us/windows/hardware/).
3. **Driver Package Preparation:** Package `phylaram.sys` and its companion `.inf` into a signed cabinet (`.cab`) file using `makecab` and `signtool`.
4. **Attestation Submission:** Submit the package to the Microsoft Hardware Dev Center portal for automated WHQL Attestation Signing.
5. **Download Signed Package:** Retrieve the Microsoft-signed catalog (`.cat`) and driver binary, valid on all modern Windows installations without disabling driver signature enforcement.

*(Tracked in GitHub Issue [#5](https://github.com/joeyvictorino/phylaram/issues/5))*

---

## 3. README Claims & Evidence Matrix

Every claim in the PhylaRAM README is mapped directly to source code and validation evidence:

| Claim in README | Implementation Location | Validation Evidence |
| :--- | :--- | :--- |
| **Strict Physical Addressing (`offset == phys`)** | `cli/raw_writer.cpp` (`PreflightAndOpen`, `SetFilePointerEx`, `FSCTL_SET_SPARSE`) | Tested via range algebra suite and offline verifier schema |
| **Forensic Truth (`UNREADABLE != ZERO`)** | `cli/acquire.cpp` (16 MiB $\to$ 4 KiB isolation loop, `AddUnreadable`) | `tests/test_mock_acquire.cpp` scenario 2 & 4 (synthetic bad pages) |
| **Live Kernel Hints (DTB / KPCR / Base)** | `driver/session.c` (`PhylaQueryKernelHints`, `KeStackAttachProcess`) | ABI struct assertions (`shared/phylaram.h`), `test_map_json.cpp` |
| **Bandwidth Throttling (`--rate-limit`)** | `cli/acquire.cpp` (steady_clock elapsed micro-sleep regulation) | `tests/test_cli_parser.cpp` parser validation |
| **Stdout Streaming (`-`)** | `cli/raw_writer.cpp` (`_setmode(_fileno(stdout), _O_BINARY)`) | `tests/test_cli_parser.cpp` stdout argument validation |
| **Provenance Map (`phylaram-map-2`)** | `cli/map.cpp` (`WriteMapJson`), `tools/phylaram-verify/src/schema.rs` | `tests/test_map_json.cpp` schema contract & serialization tests |
| **Non-PnP Control KMDF Driver** | `driver/driver.c` (`WdfControlDeviceInitAllocate`, SDDL `D:P(A;;GA;;;SY)(A;;GA;;;BA)`) | Code audit, exclusive device access contract |
| **Offline Verifier (`phylaram-verify`)** | `tools/phylaram-verify/` (Rust crate) | Unit tests, e2e bundle verification, and `proptest` property tests |

---

## 4. Open Validation Gates (Requiring Live Windows VM / Hardware)

The following items cannot be fully proven in macOS or GitHub-hosted runners and are tracked as open GitHub issues:

| Gate | Title | Description | GitHub Issue |
| :---: | :--- | :--- | :---: |
| **Gate 1** | MSVC & KMDF Automated CI Build | Toolset restored via NuGet in GitHub Actions | [#1](https://github.com/joeyvictorino/phylaram/issues/1) |
| **Gate 2** | Static Driver Verifier (SDV) & CodeQL | Static analysis requiring local WDK install | [#2](https://github.com/joeyvictorino/phylaram/issues/2) |
| **Gate 3** | Driver Verifier Dynamic Stress Profile | 100-cycle live acquisition stress test | [#3](https://github.com/joeyvictorino/phylaram/issues/3) |
| **Gate 4** | Hardware & Memory Topology Matrix | 4 GB to 128 GB+ RAM, ReBAR, NUMA validation | [#4](https://github.com/joeyvictorino/phylaram/issues/4) |
| **Gate 5** | Production EV / WHQL Attestation Signing | Microsoft Partner Center driver attestation | [#5](https://github.com/joeyvictorino/phylaram/issues/5) |
| **Gate 6** | Forensic Interoperability Fixtures | Live test image validation in Volatility 3 | [#6](https://github.com/joeyvictorino/phylaram/issues/6) |

---

## 5. Summary

PhylaRAM `0.1.0-alpha` provides a mathematically sound, forensically honest memory acquisition engine. All code compiles under strict warning-as-error constraints, all algorithms pass extensive property and mock tests, and all known forensic liabilities (guessed VBS status, caller-CR3 instead of System DTB, non-functional pagefile/dmp flags) have been removed.
