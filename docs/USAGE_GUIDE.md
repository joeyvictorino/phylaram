# PhylaRAM: Incident Responder & Operator's Guide

A practical handbook for acquiring and analyzing live physical memory on Windows systems.

> Alpha software. Lab and controlled test environments only. See the warning in the [README](../README.md) and [`STATUS.md`](STATUS.md).

---

## 1. Quick Start

### 1.1 Local Disk Acquisition
Run elevated from PowerShell or Administrator Command Prompt:
```cmd
phylaram.exe C:\Evidence\DESKTOP-MEM.raw
```

**Generated Evidence Bundle:**
- `DESKTOP-MEM.raw`: Flat physical RAW image (sparse-allocated).
- `DESKTOP-MEM.raw.map.json`: Full physical topology map, Ring 0 kernel hints, and status.
- `DESKTOP-MEM.raw.sha256`: SHA-256 digest of the flat logical image (sidecar).

---

## 2. Advanced Acquisition Modes

### 2.1 Bandwidth Throttling (`--rate-limit`)
To prevent saturation of storage I/O or network connections on production database servers or virtual hypervisors:
```cmd
phylaram.exe D:\Evidence\image.raw --rate-limit 150
```
*Limits memory acquisition throughput to 150 MB/s.*

### 2.2 Output Formats
The format is chosen by the output file extension or with `--format`:
```cmd
phylaram.exe D:\Evidence\image.raw                :: flat physical-address-preserving RAW (default)
phylaram.exe D:\Evidence\image.zdmp               :: 64-bit Complete Memory Crash Dump layout (.zdmp or .dmp)
phylaram.exe D:\Evidence\image.e01 --case-number C-1 --examiner "A. Analyst"   :: Expert Witness container
```
`--case-number`, `--evidence-number`, `--examiner`, `--description` and `--notes` populate E01 metadata. Every capture still produces the map and SHA-256 sidecars. The repository's ZDMP and E01 tests check header-layout invariants only; they do not exercise the writers, and no test opens a produced `.zdmp` or `.e01` with a third-party reader, so treat those two formats as experimental (see [`STATUS.md`](STATUS.md)). `phylaram-verify` verifies the flat RAW layout.

Streaming to stdout (`phylaram.exe -`) is **not supported**: a raw stream cannot carry the unreadable-byte provenance map, so the CLI rejects it.

### 2.3 Pre-Acquisition Triage (`--dry-run`)
Inspect physical RAM topology, MMIO ranges, and kernel pointers in <1 second without touching disk:
```cmd
phylaram.exe --dry-run
```

**JSON Mode for Automation / SOAR:**
```cmd
phylaram.exe --dry-run --json
```

---

## 3. Evidence Verification

Verify evidence integrity using the standalone Rust offline verifier:
```cmd
phylaram-verify.exe C:\Evidence\DESKTOP-MEM.raw C:\Evidence\DESKTOP-MEM.raw.map.json C:\Evidence\DESKTOP-MEM.raw.sha256 --verbose
```

---

## 4. Volatility 3 & MemProcFS Analysis

### 4.1 Volatility 3 Accelerated Bridge
The bundled helper prints the kernel hints recorded in the map (System DTB, kernel base, build) and then runs `vol -f <image> <plugin>`. The hints are informational and are not passed to Volatility 3:
```bash
python tools/phylaram_vol3.py C:\Evidence\DESKTOP-MEM.raw windows.pslist
python tools/phylaram_vol3.py C:\Evidence\DESKTOP-MEM.raw windows.malfind
```

### 4.2 MemProcFS Native Mount
Mount the raw memory file directly as a virtual filesystem:
```cmd
MemProcFS.exe -device C:\Evidence\DESKTOP-MEM.raw
```
*(`tools/phylaram_memprocfs.py` wraps this and also passes the System DTB hint; interoperability with real acquired images is an open validation gate.)*

---

## 5. Velociraptor Artifacts

Example artifacts are in [`integrations/velociraptor/`](../integrations/velociraptor/): `Windows.Memory.PhylaRAM.yaml` (capture) and `Windows.Triage.PhylaRAMDryRun.yaml` (topology and kernel hints only). No test in this repository runs them against a Velociraptor deployment; treat them as starting points and test in a lab first.
