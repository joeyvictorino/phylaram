# PhylaRAM: Incident Responder & Operator's Guide

A practical handbook for acquiring, streaming, and analyzing live physical memory on Windows systems.

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
- `DESKTOP-MEM.raw.sha256`: Flat logical image SHA-256 Authenticode sidecar.

---

## 2. Advanced Acquisition Modes

### 2.1 Bandwidth Throttling (`--rate-limit`)
To prevent saturation of storage I/O or network connections on production database servers or virtual hypervisors:
```cmd
phylaram.exe D:\Evidence\image.raw --rate-limit 150
```
*Limits memory acquisition throughput to 150 MB/s.*

### 2.2 Live Network Streaming (`stdout`)
Acquire memory without writing evidence to the local drive:
```cmd
:: Stream over Netcat / ncat to a remote forensic evidence receiver
phylaram.exe - | ncat.exe 192.168.1.50 9999

:: Stream directly over SSH
phylaram.exe - | ssh analyst@forensic-server "cat > /cases/2026/host-ram.raw"
```

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
Use the bundled bridge script to bypass symbol brute-force scanning:
```bash
python tools/phylaram_vol3.py C:\Evidence\DESKTOP-MEM.raw windows.pslist
python tools/phylaram_vol3.py C:\Evidence\DESKTOP-MEM.raw windows.malfind
```

### 4.2 MemProcFS Native Mount
Mount the raw memory file directly as a virtual filesystem:
```cmd
MemProcFS.exe -device C:\Evidence\DESKTOP-MEM.raw
```
*(MemProcFS reads the physical address space directly; page tables resolve immediately via CR3).*

---

## 5. Velociraptor VQL Artifact Integration

```yaml
name: Windows.Detection.PhylaRAMCapture
description: Acquires full physical RAM using PhylaRAM into a central SMB share.
parameters:
  - name: TargetShare
    default: "\\\\forensics-smb\\drops\\$COMPUTERNAME.raw"

sources:
  - queries:
      - SELECT execve(argv=["phylaram.exe", TargetShare, "--quiet"]) AS Result
        FROM scope()
```
