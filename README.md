# PhylaRAM

Live physical-memory acquisition for modern Windows.

PhylaRAM is a small forensic acquisition utility for capturing live physical RAM from x64 Windows 10 version 2004 or later and Windows 11 into a flat raw memory image.

```text
phylaram.exe memory.raw
```

**Physical offsets preserved. Partial reads preserved. Unreadable memory never silently reported as acquired.**

---

## A Love Letter to the DFIR Community

> *I built **PhylaRAM** as an open-source love letter to the digital forensics and incident response (DFIR) community.*
> 
> *Recently, while working an active, high-severity incident response case, our team needed an immediate, reliable physical memory acquisition tool for a modern Windows 11 endpoint with Virtualization-Based Security (VBS) and Memory Integrity (HVCI) enabled. Instead of being able to download a clean tool and immediately start triage, we were met with broken legacy utilities that bluescreened the host, or commercial tools trapped behind corporate registration gates, demo requests, and vendor sales funnels.*
> 
> *In the middle of a live intrusion, every second matters. Evidence in volatile memory perishes rapidly. Responders should never have to wait hours for a sales rep to email a license key just to preserve critical forensic evidence.*
> 
> *PhylaRAM is 100% open source, free forever, mathematically verified, and purpose-built for real-world defenders.*

## Output

Every acquisition produces an evidence bundle:

```text
memory.raw
memory.raw.map.json
memory.raw.sha256
```

### Provenance Map (`memory.raw.map.json`)

The `.map.json` sidecar identifies the tool and documents exact physical run topologies and unreadable spans:

```json
{
  "producer": "PhylaRAM",
  "producer_version": "1.0.0",
  "schema": "phylaram-map-1",
  "status": "complete",
  "logical_size": 17179869184,
  "physical_bytes": 16909336576,
  "acquired_bytes": 16909336576,
  "unreadable_bytes": 0,
  "topology_changed": false,
  "sha256": "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
  "ranges": [
    {"driver_run": 0, "start": "0x1000", "length": 651264},
    {"driver_run": 1, "start": "0x100000", "length": 16908685312}
  ],
  "unreadable": []
}
```

---

## Core Characteristics

- **Single Portable Executable:** One self-contained executable (`phylaram.exe`) with an embedded signed driver (`phylaram.sys`). No installer required.
- **Strict Physical Addressing:** `file offset == physical address`. Hardware MMIO gaps and unreadable pages remain unallocated sparse extents on NTFS and ReFS without zero-byte block inflation.
- **Byte-Accurate Error Isolation:** If a 16 MiB read partially fails, every successfully transferred byte is preserved. Unreadable pages are isolated at 4 KiB granularity and recorded in the provenance map with their exact NTSTATUS.
- **Evidence Immutability:** Preflight collision protection checks for existing targets and staging files. Data is staged to `memory.raw.partial` and promoted atomically only upon valid completion.
- **Volatility & MemProcFS Compatible:** Directly parsable by Volatility 3 (`windows.info`, `windows.pslist`, `windows.malfind`) and MemProcFS.
- **Hardened Windows Ready:** Operates with Secure Boot, Virtualization-Based Security (VBS), and Hypervisor-Protected Code Integrity (HVCI / Memory Integrity) fully enabled.

---

## Architecture & Lifecycle

1. `phylaram.exe` extracts the embedded signed driver to a secure private directory with a restricted ACL granting access only to `SYSTEM` and `BUILTIN\Administrators`.
2. Installs and starts a demand-start kernel service through documented Service Control Manager (SCM) APIs.
3. Opens `\\.\PhylaRAM`, creating an immutable physical memory topology snapshot (`MmGetPhysicalMemoryRangesEx2`).
4. Acquires RAM via run-relative `METHOD_OUT_DIRECT` IOCTLs (`MmCopyMemory` with `MM_COPY_MEMORY_PHYSICAL`).
5. Writes sparse extents to disk and computes the logical SHA-256 hash.
6. Closes the device handle, stops and deletes the service, and cleans up the temporary driver file.

---

## Usage

```text
phylaram.exe <output.raw> [--quiet] [--no-hash]
```

### Options

- `--quiet`: Suppress interactive terminal progress and statistics.
- `--no-hash`: Skip SHA-256 calculation and `.sha256` sidecar generation.
- `--help`, `-h`: Display command-line usage.

### Exit Codes

- `0` **COMPLETE**: All physical RAM ranges acquired successfully, topology unchanged, hash verified.
- `2` **INCOMPLETE**: Acquisition reached the end of RAM, but one or more pages were unreadable or memory topology shifted.
- `1` **FAILED**: Acquisition was cancelled (Ctrl+C), encountered an I/O failure, permission failure, or unsupported OS.

---

## SANS FOR500 / FOR508 Courseware Guide

For forensic instructors, lab authors, and DFIR practitioners integrating PhylaRAM into courseware, see the dedicated [**SANS FOR500 & FOR508 Instructor Guide**](docs/SANS_FOR500_FOR508_GUIDE.md).

---

## Dual-Mode Architecture & Visual Experience

PhylaRAM combines an ultra-fast, zero-overhead **CLI** for power users and automation with an **Apple-inspired dark glassmorphic GUI** for intuitive one-click acquisition and live physical memory visualization. See the complete design specification in [**PhylaRAM Dual-Mode Architecture & Visual Identity**](docs/PHYLARAM_GUI_AND_VISUAL_SPEC.md).

---

## Strategy & Beating Commercial Acquisition Tools

For an in-depth breakdown of how PhylaRAM outclasses legacy commercial tools (HVCI immunity, zero silent-zero fabrication, micro-footprint preservation, and open RAW architecture), see [**PhylaRAM Product Strategy & Commercial Comparison**](docs/BEATING_COMMERCIAL_TOOLS_AND_PRODUCT_STRATEGY.md).

---

## Offline Verification Tool (`phylaram-verify`)

An independent offline verification tool written in Rust is available in `tools/phylaram-verify/`:

```bash
phylaram-verify memory.raw memory.raw.map.json [memory.raw.sha256]
```

---

---

## Windows Quick Start & Build

### Requirements
- Windows 10 (version 2004 / build 19041 or later) or Windows 11 (x64)
- Visual Studio 2022 with **Desktop development with C++**
- Windows 10/11 SDK & Windows Driver Kit (WDK)
- Administrator privileges

### Automated 1-Click Build (Windows)
Open an elevated Command Prompt or Developer Command Prompt for VS 2022 and run:
```cmd
scripts\build_windows.bat
```
This builds `bin\phylaram.sys`, embeds it into `bin\phylaram.exe`, compiles `bin\phylaram-verify.exe`, and packages everything to `dist\PhylaRAM-v1.0-x64\`.

### Test Signing (For Development / Test VMs)
To test on a development VM without an EV certificate:
```cmd
scripts\setup_test_signing.bat
```
*(Ensure `bcdedit /set testsigning on` is run in an elevated command prompt on the test VM followed by a reboot).*

### Running PhylaRAM
Open an Administrator Command Prompt and run:
```cmd
phylaram.exe E:\Evidence\memory.raw
```

## Engineering Standard

All development, review, and verification follow the canonical [`ENGINEERING_STANDARD.md`](ENGINEERING_STANDARD.md).
