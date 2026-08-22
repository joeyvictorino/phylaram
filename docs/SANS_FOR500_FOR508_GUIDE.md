# SANS FOR500 & FOR508 Instructor and Courseware Guide: PhylaRAM

**Document ID:** `PHYLA-SANS-GUIDE-001`  
**Audience:** SANS Faculty, Course Authors, Lab Authors, and Forensic Practitioners  
**Target Courses:** SANS FOR500 (Windows Forensic Analysis) & SANS FOR508 (Advanced Incident Response, Threat Hunting, and Digital Forensics)  
**Tool Version:** PhylaRAM 1.0  
**License:** Open Source (MIT)  

---

## 1. Executive Summary: Why PhylaRAM for SANS Labs

Digital forensics instructors consistently encounter four friction points when teaching live physical memory acquisition on modern Windows systems (Windows 10 2004+ and Windows 11):

1. **Kernel Driver Instability & HVCI Incompatibility:** Legacy acquisition tools frequently crash or fail on modern endpoints with Virtualization-Based Security (VBS), Hypervisor-Protected Code Integrity (HVCI / Memory Integrity), or Secure Boot enabled.
2. **Silent Zero-Filling (The Forensic Falsehood):** Legacy utilities often treat unreadable memory ranges as successfully acquired `0x00` bytes, misleading students into believing data was present and zeroed rather than unacquired or protected by hypervisor security.
3. **Proprietary Format Overhead:** Compressed or non-standard container formats require students to learn custom plugins or decompression steps before they can run standard tools like Volatility 3 or MemProcFS.
4. **Lab Setup Friction:** Multi-file installers, dependency requirements, and heavy GUI disk-imaging suites complicate automated lab deployment and introduce massive memory footprint contamination.

### The PhylaRAM Solution

PhylaRAM is a compact, self-contained, open-source forensic acquisition utility designed specifically for modern Windows x64 environments.

```text
phylaram.exe memory.raw
```

> **The PhylaRAM Invariant:**  
> *Physical offsets preserved. Partial reads preserved. Unreadable memory never silently reported as acquired.*

---

## 2. Evidence Output Architecture

Every acquisition produces a self-verifying evidence bundle:

```text
memory.raw                  # Flat, physical-addressed RAW memory image (file offset == physical address)
memory.raw.map.json         # Provenance sidecar detailing exact physical runs and unreadable spans
memory.raw.sha256           # Logical SHA-256 digest of the raw image
```

### 2.1 The Flat RAW Invariant (`file offset == physical address`)

In PhylaRAM, the byte at file offset `0x100000` is the exact physical memory byte at address `0x100000`. Hardware MMIO gaps and unpopulated physical address spaces are preserved as unallocated sparse file extents on NTFS and ReFS volumes:

$$\text{File Offset } \equiv \text{Physical Address}$$

**Lab Advantage:** Students can immediately inspect physical structures at known physical offsets using a hex editor, Volatility 3, or MemProcFS without translation layers.

### 2.2 Provenance Sidecar (`memory.raw.map.json`)

The sidecar adheres to schema `phylaram-map-1`, documenting the exact state of physical memory:

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

### 2.3 `UNREADABLE != ZERO`: Forensic Precision

PhylaRAM strictly distinguishes three concepts that legacy tools often conflate:

| Category | Meaning | RAW File Representation | `.map.json` Representation |
| :--- | :--- | :--- | :--- |
| **Physical MMIO Gap** | Unpopulated address space between RAM runs. | Sparse hole (reads as `0x00`). | Not listed in `ranges`. |
| **Acquired Zero RAM** | Real RAM containing observed `0x00` data. | Allocated bytes written to disk. | Accounted in `acquired_bytes`. |
| **Unreadable RAM** | RAM reported by OS but blocked by hardware/VBS. | Sparse hole (reads as `0x00`). | Recorded in `unreadable` with `NTSTATUS`. |

---

## 3. Tool Comparison & Operational Forensic Hygiene

In incident response and live forensics (RFC 3227 Order of Volatility), the tool chosen to collect memory directly impacts the survival of volatile evidence.

```
+---------------------------------------------------------------------------------------------------+
|                                  Memory Acquisition Comparison Matrix                             |
+----------------------+--------------------+---------------------+---------------------------------+
| Dimension            | PhylaRAM           | Heavy GUI Imagers   | Targeted Process Dumps          |
|                      | (Physical RAW)     | (e.g. FTK Imager)   | (e.g. ProcDump / procdump64)    |
+----------------------+--------------------+---------------------+---------------------------------+
| Acquisition Scope    | Full Physical RAM  | Full Physical RAM   | Virtual Process Memory (1 PID)  |
| Kernel Visibility    | YES (Complete)     | YES                 | NO (User-mode only)             |
| Rootkit / DKOM Hunt  | YES (Full)         | YES                 | NO (Blind to unlinked objects)  |
| Memory Footprint     | Minimal (~16 MiB)  | High (Hundreds MB)  | Low                             |
| Page Cache Impact    | Negligible         | High (Alters Cache) | Low                             |
| HVCI / VBS Ready     | YES (Native KMDF)  | Often Incompatible  | N/A (User-mode API)             |
| Physical Addressing  | Exact (Offset==PA) | Often Mapped/Raw    | Virtual Addresses Only          |
| Unreadable Tracking  | Byte-level map.json| Silently Zeroed     | N/A                             |
| Volatility 3 Ready   | Instant Out-of-Box | Instant             | Requires process-specific flags |
+----------------------+--------------------+---------------------+---------------------------------+
```

### 3.1 Why Full Physical RAM Outranks Process Dumps (ProcDump)
While `procdump64.exe -ma <pid>` is useful for focused triage of a known malicious binary (such as `explorer.exe` or `lsass.exe`), process dumping is inherently blind to:
1. **Kernel-Level Threats:** Rootkit drivers (`.sys`), DKOM `EPROCESS` unlinking, and malicious SSDT/IDT hooks.
2. **Injected Code Across Unknown PIDs:** Fileless beacons living in unassociated physical memory pools or worker threads.
3. **Volatile Network Connections:** Kernel socket structures (`netscan`) and active TCP tables.
4. **Master Encryption Keys:** BitLocker FVEK keys, VeraCrypt volume keys, and TLS master secrets residing in non-paged pool memory.

### 3.2 Minimizing Investigator Footprint on Live Targets
Every megabyte of memory allocated by a heavy forensic tool overwrites standby lists and unallocated pages where recent adversary activity, deleted commands, and decrypted strings reside. PhylaRAM pre-allocates a single 16 MiB I/O buffer and streams chunks directly to disk with Direct I/O (`METHOD_OUT_DIRECT`), preserving maximum unallocated memory for analysis.

---

## 4. Integration with SANS Lab Tools

### 4.1 Volatility 3 Integration (FOR508)

Because PhylaRAM produces a standard flat raw image, Volatility 3 analyzes it immediately without any custom translation layers:

```bash
# Verify OS and kernel information
vol.py -f memory.raw windows.info

# List running processes and threads
vol.py -f memory.raw windows.pslist
vol.py -f memory.raw windows.pstree

# Detect code injection and stealth processes
vol.py -f memory.raw windows.malfind

# Extract network socket connections
vol.py -f memory.raw windows.netscan
```

### 4.2 MemProcFS Integration (FOR500 & FOR508)

MemProcFS can mount the generated `memory.raw` file directly into a virtual filesystem:

```cmd
MemProcFS.exe -device memory.raw -mount M:
```

Students can immediately navigate `M:\sys\`, `M:\name\`, and `M:\pid\` to inspect processes, memory dumps, and virtual address spaces.

### 4.3 Offline Verification Tool (`phylaram-verify`)

The repository includes a standalone offline verification binary written in Rust with `#![forbid(unsafe_code)]`:

```bash
# Verify evidence integrity, range ordering, and SHA-256 digest
phylaram-verify memory.raw memory.raw.map.json memory.raw.sha256
```

**Verification Output:**
```text
Verifying PhylaRAM acquisition bundle...
  RAW Image : memory.raw
  Map JSON  : memory.raw.map.json
  SHA-256   : memory.raw.sha256

[VALID] Evidence bundle verified successfully.
  Producer         : PhylaRAM v1.0.0
  Schema           : phylaram-map-1
  Status           : complete
  Logical Size     : 17179869184 bytes
  Physical RAM     : 16909336576 bytes
  Acquired         : 16909336576 bytes
  Unreadable       : 0 bytes
  Topology Changed : false
  SHA-256          : e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
  RAM Runs         : 2
  Unreadable Spans : 0
```

---

## 5. Suggested SANS Lab Exercise Workbooks

### Lab Exercise A: Live Physical RAM Acquisition (FOR500 / FOR508)

#### Objective
Capture the live physical memory of a Windows 10/11 system to an external evidence drive while preserving physical offsets and documenting memory topology.

#### Steps
1. Open an elevated Administrator PowerShell / Command Prompt.
2. Execute acquisition:
   ```cmd
   D:\Tools\phylaram.exe E:\Evidence\Case001_Memory.raw
   ```
3. Observe the output:
   - Notice the physical RAM size and range count displayed during preflight.
   - Observe the interactive progress counter.
4. Verify the generated files:
   - `E:\Evidence\Case001_Memory.raw`
   - `E:\Evidence\Case001_Memory.raw.map.json`
   - `E:\Evidence\Case001_Memory.raw.sha256`

---

### Lab Exercise B: Evidence Integrity & Sidecar Provenance Analysis

#### Objective
Understand how physical memory topologies are represented and verify that no evidence collision or silent corruption occurred.

#### Steps
1. Inspect `Case001_Memory.raw.map.json` using `type` or a text editor.
2. Confirm:
   - `status` is `"complete"`.
   - `acquired_bytes` matches `physical_bytes`.
   - `topology_changed` is `false`.
3. Run the independent verification tool:
   ```bash
   phylaram-verify Case001_Memory.raw Case001_Memory.raw.map.json Case001_Memory.raw.sha256
   ```

---

### Lab Exercise C: Advanced Memory Analysis with Volatility 3

#### Objective
Identify active processes, injected DLLs, and network connections from the acquired raw image.

#### Steps
1. Identify system build:
   ```bash
   vol.py -f Case001_Memory.raw windows.info
   ```
2. Scan for hidden or terminated processes:
   ```bash
   vol.py -f Case001_Memory.raw windows.psscan
   ```
3. Scan for injected code:
   ```bash
   vol.py -f Case001_Memory.raw windows.malfind
   ```

---

## 6. Security & Stability Architecture

PhylaRAM is designed with enterprise-grade defensive security:

1. **Strictly Non-PnP Control Driver:** Operates at passive execution levels with zero SSDT/IDT hooks and zero undocumented kernel patches.
2. **Hardened Driver Extraction:** Extracted to `%ProgramData%\PhylaRAM\Temp\` with an explicit SDDL access control list (`D:P(A;;GA;;;SY)(A;;GA;;;BA)`) accessible only to `SYSTEM` and `Administrators`.
3. **CWE-428 Elimination:** Service Control Manager binary paths are quoted (`"\"" + path + "\""`).
4. **Collision Immunity:** Preflights all 6 target and staging paths before writing a single byte. Refuses to overwrite existing evidence.
5. **Atomic Promotion:** Staged to `.partial` files and promoted atomically via `MoveFileExW` without overwrite flags upon completion.
6. **Clean Teardown:** Closes device handle, stops driver service, deletes service, and removes temporary driver binary upon completion or Ctrl+C cancellation.

---

## 7. Summary for SANS Authors

PhylaRAM delivers the exact properties needed for modern DFIR instruction:
- Simple, memorable tool name (**PhylaRAM**).
- Zero student friction (single executable, automatic driver extraction, clean teardown).
- Perfect compatibility with Volatility 3 and MemProcFS.
- Uncompromising forensic truth (`UNREADABLE != ZERO`).
