# PhylaRAM Current Status and Verification Report

**Version:** `0.1.0-alpha`  
**Date:** August 2026  
**Target:** Windows 10 version 2004+ / Windows 11, x64  

This report separates implementation state from verification evidence. A feature being present in source does not mean its runtime or forensic claims have been validated on the required environment.

---

## 1. Current Implementation State

The current remediation branch includes:

- C17 KMDF control driver with exclusive SYSTEM/Administrator access;
- frozen run-index/offset physical read protocol;
- device-level synchronization between file cleanup and IOCTL callbacks;
- preservation of `MmCopyMemory` partial byte counts and NTSTATUS;
- C++20 acquisition engine with checked byte accounting;
- mandatory SHA-256 for finalized captures;
- one shared evidence publication transaction for CLI and GUI;
- no provenance-free stdout acquisition mode;
- no hash-free finalized evidence mode;
- embedded-driver-only implicit trust path;
- protected temporary driver extraction and transactional service cleanup;
- Rust 2021 offline verifier with range/unreadable geometry validation;
- provenance map limited to acquisition facts rather than analytic/compliance interpretation;
- warning-as-error build configuration, Rust fmt/clippy/tests, portable tests, and CodeQL workflows.

These statements describe source architecture. They do not imply that every platform validation gate below has passed.

---

## 2. Automated Repository Checks

| Area | Configured check | What it establishes if green |
| --- | --- | --- |
| C++ portable models/tests | Clang C++20, `-Wall -Wextra -Werror` | Portable test programs compile cleanly and their assertions pass. |
| Rust verifier | `cargo fmt --check`, `cargo clippy -- -D warnings`, `cargo test` | Formatting, Clippy diagnostics, and verifier tests pass for the CI environment. |
| Python validation models | fixture/topology scripts | Model/fixture assertions pass; this is not physical hardware validation. |
| Windows C/C++ | MSBuild on `windows-2022` | Driver and application source build in the configured CI toolchain. |
| CodeQL | C/C++ security/quality queries | Configured CodeQL analysis completed for the built source. |

A green CI run is necessary but not sufficient for release readiness.

---

## 3. Signing Status

Current CI/pre-release artifacts are **test-signed** for controlled test environments.

Production deployment with Secure Boot and HVCI requires completion of the production driver-signing gate using the exact Microsoft path selected for release.

Microsoft **attestation signing** and **Windows Hardware Compatibility Program (WHCP/HLK) certification** are distinct submission/certification paths. This repository must not describe them as “WHQL attestation signing.” The production gate remains open until the chosen path is completed and the resulting driver is validated under the stated Windows security controls.

Do not weaken security controls on a production evidence host merely to load an alpha/test-signed driver.

---

## 4. Open Validation Gates

These remain release-significant until evidence shows they have actually run and passed.

| Gate | Requirement | State |
| ---: | --- | --- |
| 1 | Strict Windows x64 build and packaging in the authoritative toolchain | CI-configured; must remain green at release commit |
| 2 | Static driver analysis including applicable WDK/SDV checks | Open where dedicated WDK tooling is required |
| 3 | Driver Verifier stress with repeated acquisition cycles and zero bugchecks/leaks/IRQL violations | **Open** |
| 4 | Physical/virtual topology matrix including 4 GB through 128 GB+, ReBAR/MMIO holes, NUMA, NTFS/ReFS behavior | **Open** |
| 5 | Production Microsoft driver signing plus Secure Boot/VBS/HVCI validation without security degradation | **Open** |
| 6 | Real acquired-image interoperability across supported Windows builds in Volatility 3 and MemProcFS | **Open** |

The corresponding GitHub issues remain the operational tracking records.

---

## 5. Claims the Repository Can Make Today

Subject to the current branch compiling/tests passing, the implementation is designed to provide:

- bounded physical reads derived from a frozen run index rather than arbitrary caller physical addresses;
- flat physical-address-oriented RAW placement;
- explicit provenance for unreadable memory rather than semantically treating it as observed zero;
- exact acquired/unreadable accounting in finalized maps;
- explicit topology-change status;
- mandatory logical RAW SHA-256;
- independent offline structural/hash verification;
- shared CLI/GUI evidence finalization semantics;
- fail-closed handling of observed finalization errors.

The repository should **not** currently claim:

- production-ready responder deployment;
- physical-hardware validation across the support matrix;
- Driver Verifier completion;
- production Microsoft driver-signing completion;
- guaranteed Secure Boot/HVCI compatibility;
- universal Volatility/MemProcFS interoperability across supported Windows builds;
- legal admissibility or complete chain of custody merely because `phylaram-verify` passes.

---

## 6. Readiness Rule

PhylaRAM remains alpha until the code, tests, documentation, and stated validation evidence agree.

Implementation does not outrank verification. A gate closes only when the relevant command/hardware/test actually ran and the result was recorded.
