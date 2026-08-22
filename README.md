# PhylaRAM

> **Live physical-memory acquisition for modern Windows with byte-accurate error isolation, live kernel hints, and zero vendor lock-in.**

[![CI](https://github.com/joeyvictorino/phylaram/actions/workflows/ci.yml/badge.svg)](https://github.com/joeyvictorino/phylaram/actions/workflows/ci.yml)
[![Pre-release](https://img.shields.io/github/v/release/joeyvictorino/phylaram?include_prereleases&label=pre-release)](https://github.com/joeyvictorino/phylaram/releases)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

---

> [!WARNING]
> **Pre-Release Status (v0.1.0-alpha):** Binaries produced in CI are test-signed with a self-signed certificate. To load the kernel driver (`phylaram.sys`) on a test VM or physical machine:
> 1. Ensure **Secure Boot is Disabled** in your UEFI/BIOS or VM firmware settings.
> 2. Right-click `install_test_cert.bat` and select **"Run as administrator"** (installs certificate to Trusted Root & Trusted Publishers).
> 3. Enable test-signing mode from an elevated prompt:
>    ```cmd
>    bcdedit /set testsigning on
>    shutdown /r /t 0
>    ```
> Production EV / WHQL attestation signing is planned for post-validation releases.

---

## The Three Core Differentiators

| # | Differentiator | Why It Matters to DFIR |
| :---: | :--- | :--- |
| **1** | **Strict Physical Addressing (`file offset == physical address`)** | Hardware MMIO gaps and unallocated address spaces remain non-allocated sparse extents on NTFS/ReFS. The flat RAW image matches the physical bus layout 1:1 without synthetic zero-inflation. |
| **2** | **Forensic Truth (`UNREADABLE != ZERO`)** | If a 16 MiB read fails, every transferred byte is preserved. Unreadable pages are isolated at 4 KiB page boundaries and recorded in `memory.raw.map.json` with their exact `NTSTATUS`. Memory is **never** silently fabricated as zero data. |
| **3** | **Live Ring 0 Kernel Hints (`phylaram-map-2`)** | Captures the System process Directory Table Base (CR3), executing CPU KPCR, NTOSKRNL base address, and image size directly from Ring 0 during acquisition, eliminating slow KDBG/DTB brute-force scans in Volatility 3 and MemProcFS. |

---

## Comparison with Existing Acquisition Tools

| Capability | **PhylaRAM** | WinPmem (v3/v4) | DumpIt (Comae) | Magnet RAM Capture | Belkasoft RAM Capturer |
| :--- | :---: | :---: | :---: | :---: | :---: |
| **License & Availability** | **100% Open Source (MIT)** | Open Source (GPL/Apache) | Commercial (Locked) | Freeware (Registration Gate) | Freeware (Registration Gate) |
| **Flat RAW (`offset == phys`)** | **Yes (Sparse NTFS/ReFS)** | Yes / Raw | RAW / Crash Dump | RAW | RAW |
| **Error Isolation (`UNREADABLE != ZERO`)** | **Yes (4 KiB + NTSTATUS in Map)** | Partial / Zero-fills | Zero-fills | Zero-fills | Zero-fills |
| **Live Kernel Hints (CR3 / KPCR / NT Base)** | **Yes (`.map.json`)** | No | No | No | No |
| **Memory Acquisition Method** | **`MmCopyMemory(PHYSICAL)`** | `\Device\PhysicalMemory` / PTE | Proprietary Driver | Proprietary Driver | Proprietary Driver |
| **Non-PnP Control KMDF Driver** | **Yes (Exclusive Access)** | Legacy / Monolithic | Monolithic | Monolithic | Monolithic |
| **Bandwidth Throttling (`--rate-limit`)** | **Yes** | No | No | No | No |
| **Stdout Streaming (`-`)** | **Yes** | No | No | No | No |
| **Independent Offline Verifier** | **Yes (Rust `phylaram-verify`)** | No | No | No | No |

---

## Output Evidence Bundle

Every successful acquisition produces three files:

```text
E:\Evidence\
├── memory.raw           # Flat physical RAM image (sparse NTFS/ReFS)
├── memory.raw.map.json  # Provenance map with run topology & kernel hints
└── memory.raw.sha256    # Cryptographic checksum of logical RAW image
```

### Provenance Map Example (`memory.raw.map.json`)

*(Illustrative example conforming to [`phylaram-map-2`](docs/MAP_SCHEMA.md))*

```json
{
  "producer": "PhylaRAM",
  "producer_version": "0.1.0-alpha",
  "schema": "phylaram-map-2",
  "status": "complete",
  "logical_size": 17179869184,
  "physical_bytes": 16909336576,
  "acquired_bytes": 16909336576,
  "unreadable_bytes": 0,
  "topology_changed": false,
  "sha256": "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
  "kernel_hints": {
    "hypervisor_present": true,
    "directory_table_base": "0x1AA002",
    "kpcr_address": "0xFFFFF80023400000",
    "kernel_base": "0xFFFFF80023000000",
    "kernel_size": 11534336,
    "major_version": 10,
    "minor_version": 0,
    "build_number": 22631,
    "processors": 8
  },
  "ranges": [
    {"driver_run": 0, "start": "0x1000", "length": 651264},
    {"driver_run": 1, "start": "0x100000", "length": 16908685312}
  ],
  "unreadable": []
}
```

---

## Usage

Run `phylaram.exe` from an elevated Command Prompt or PowerShell (Administrator privileges required):

```cmd
phylaram.exe <output.raw | -> [options]
```

### Options

| Option | Description |
| :--- | :--- |
| `<output.raw>` | Destination path for the RAW image (supports local and UNC network paths). |
| `-` | Stream raw physical memory directly to standard output (`stdout`). |
| `--rate-limit <MB>` | Throttle maximum acquisition bandwidth in MB/s (e.g. `--rate-limit 150`). |
| `--quiet` | Suppress interactive progress display and statistics. |
| `--no-hash` | Skip SHA-256 computation and `.sha256` sidecar creation. |
| `--help`, `-h` | Display usage instructions. |

### Exit Codes

| Exit Code | Meaning |
| :---: | :--- |
| `0` | **COMPLETE:** All physical memory acquired cleanly, topology unchanged. |
| `2` | **INCOMPLETE:** Reached end of memory, but one or more pages were unreadable or topology shifted. |
| `1` | **FAILED:** Acquisition aborted (Ctrl+C, I/O error, elevation failure, unsupported OS). |

---

## Validating in a Lab / VM with Volatility 3

1. **Prepare your VM / Test Environment:**
   - Ensure **Secure Boot is Disabled** in VM settings.
   - Run `install_test_cert.bat` as Administrator.
   - Enable test-signing and restart:
     ```cmd
     bcdedit /set testsigning on
     shutdown /r /t 0
     ```

2. **Acquire Live Memory:**
   ```cmd
   phylaram.exe C:\evidence\mem.raw
   ```

3. **Verify Bundle with Offline Verifier:**
   ```bash
   phylaram-verify C:\evidence\mem.raw C:\evidence\mem.raw.map.json C:\evidence\mem.raw.sha256
   ```

4. **Triage with Volatility 3:**
   ```bash
   # System information
   python vol.py -f mem.raw windows.info

   # Process listing
   python vol.py -f mem.raw windows.pslist

   # Scan for injected code
   python vol.py -f mem.raw windows.malfind
   ```

---

## Offline Verification Tool (`phylaram-verify`)

PhylaRAM includes an independent offline verifier written in Rust (`tools/phylaram-verify/`):

```bash
phylaram-verify <memory.raw> <memory.raw.map.json> [memory.raw.sha256]
```

It validates:
- Logical RAW file size equals `HighestPhysicalEnd`.
- Exact match between acquired/unreadable byte counts and physical memory runs.
- Cryptographic SHA-256 hash match over the logical address space.

---

## Building from Source

### Requirements
- Visual Studio 2022 (C++ Desktop Development)
- Windows Driver Kit (WDK) 10.0.26100+ (or restored via NuGet in CI)
- Rust toolchain (for `phylaram-verify`)

### Build Steps (Windows)
```cmd
nuget restore packages.config -PackagesDirectory packages
msbuild PhylaRAM.sln /p:Configuration=Release /p:Platform=x64 /v:minimal /m
```

### Portable Test Suite (macOS / Linux / Windows)
```bash
clang++ -std=c++20 -Wall -Wextra -Werror tests/test_range_algebra.cpp -o /tmp/test_range_algebra && /tmp/test_range_algebra
clang++ -std=c++20 -Wall -Wextra -Werror tests/test_mock_acquire.cpp -o /tmp/test_mock_acquire && /tmp/test_mock_acquire
clang++ -std=c++20 -Wall -Wextra -Werror tests/test_map_json.cpp -o /tmp/test_map_json && /tmp/test_map_json
clang++ -std=c++20 -Wall -Wextra -Werror tests/test_cli_parser.cpp -o /tmp/test_cli_parser && /tmp/test_cli_parser

cd tools/phylaram-verify && cargo test
```

---

## Origin & Motivation

> *PhylaRAM was created during an active incident response engagement where we needed to acquire memory from a modern Windows host, but found existing tools either outdated or locked behind vendor registration gates and sales funnels. In a live incident, responders need reliable, verifiable tools immediately — without sales friction.*

---

## License

PhylaRAM is licensed under the [MIT License](LICENSE).
