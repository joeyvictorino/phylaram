# Changelog

All notable changes to PhylaRAM will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [0.1.0-alpha] - 2026-08-22

### Added
- **KMDF Kernel Driver (`phylaram.sys`):** Non-PnP control driver implementing physical memory range enumeration (`MmGetPhysicalMemoryRangesEx2`), safe physical memory copying (`MmCopyMemory` with `MM_COPY_MEMORY_PHYSICAL`), and Direct I/O MDL mappings (`METHOD_OUT_DIRECT`).
- **Live Ring 0 Kernel Hints:** Live extraction of System process CR3 / Directory Table Base via `PsInitialSystemProcess`, executing CPU KPCR, NTOSKRNL base virtual address and image size, Windows build information, and hypervisor presence flag.
- **C++20 CLI Engine (`phylaram.exe`):** Standalone executable with embedded driver extraction, SCM service lifecycle management, 16 MiB fast-path transfers with 4 KiB page isolation on faults, Windows CNG SHA-256 calculation, and sparse NTFS/ReFS output.
- **Bandwidth Throttling:** `--rate-limit <MB/s>` to regulate acquisition throughput on resource-constrained endpoints.
- **Standard Output Streaming:** Stream flat raw memory directly to standard output (`phylaram.exe -`) for cloud and network piping.
- **Provenance Map Schema (`phylaram-map-2`):** Canonical JSON sidecar (`<output>.map.json`) recording physical run topology, byte accounting, live kernel hints, and isolated unreadable spans with exact NTSTATUS codes.
- **Offline Rust Verifier (`phylaram-verify`):** Independent command-line tool with automated verification suites, proptest property tests, and schema validation.
- **Continuous Integration:** GitHub Actions workflow with WDK NuGet toolset, MSVC x64 build, test-signing, and multi-platform portable test validation.

### Security & Forensic Invariants
- **Strict Evidence Immutability:** 6-path preflight collision detection and atomic promotion via `MoveFileExW` without overwrite flags.
- **`UNREADABLE != ZERO` Guarantee:** Unreadable physical memory pages are never silently converted to zero-filled acquired data.
- **Flat RAW Invariant:** Physical address strictly equals file offset; hardware MMIO holes preserved as sparse filesystem extents.
