# PhylaRAM Map Schema Specification (`phylaram-map-2`)

**Schema Identifier:** `phylaram-map-2`  
**Standard Status:** Canonical / Production  
**File Extension:** `.map.json`  

Every acquisition produced by PhylaRAM generates a companion provenance map sidecar (`<output>.map.json`). This JSON document records the physical memory layout, acquisition status, byte accounting, live Ring 0 kernel hints, and any isolated unreadable pages with exact NTSTATUS codes.

---

## 1. Top-Level Fields

| Field | Type | Description |
| :--- | :--- | :--- |
| `producer` | string | Constant identifier: `"PhylaRAM"` |
| `producer_version` | string | Version of PhylaRAM that performed the acquisition (e.g. `"0.1.0-alpha"`) |
| `schema` | string | Schema version: `"phylaram-map-2"` |
| `status` | string | Terminal status: `"complete"`, `"incomplete"`, or `"failed"` |
| `logical_size` | integer | Highest physical end address (bytes). Corresponds to the logical file size of the flat RAW image. |
| `physical_bytes` | integer | Total populated physical RAM across all ranges reported by the kernel (bytes). |
| `acquired_bytes` | integer | Total physical RAM bytes successfully read and written to disk. |
| `unreadable_bytes` | integer | Total physical RAM bytes that could not be read due to hardware/kernel errors. |
| `topology_changed` | boolean | `true` if physical memory layout shifted between session start and end (`MmGetPhysicalMemoryRangesEx2` mismatch). |
| `sha256` | string | SHA-256 digest computed across the flat logical RAW address space (including sparse holes and zeroed unreadable spans). |
| `kernel_hints` | object (optional) | Live Ring 0 telemetry captured from the kernel during acquisition. Omitted if unavailable. |
| `ranges` | array of objects | Ordered list of physical memory runs reported by the OS. |
| `unreadable` | array of objects | List of isolated unreadable memory spans with error status. Empty on clean acquisition. |

---

## 2. Status Outcome Semantics

| Status | Exit Code | Invariants |
| :--- | :---: | :--- |
| `"complete"` | `0` | All physical runs acquired cleanly. `unreadable_bytes == 0` and `topology_changed == false`. |
| `"incomplete"` | `2` | Acquisition reached end of RAM, but `unreadable_bytes > 0` or `topology_changed == true`. |
| `"failed"` | `1` | Acquisition aborted prematurely (Ctrl+C, I/O failure, permission denied, driver load failure). |

---

## 3. Kernel Hints Object (`kernel_hints`)

Captured directly from Ring 0 via `IOCTL_PHYLA_QUERY_HINTS` during acquisition:

| Field | Type | Format | Description |
| :--- | :--- | :--- | :--- |
| `hypervisor_present` | boolean | bool | `true` if CPUID indicates a hypervisor is present (`CPUID.01H:ECX[31] != 0`). |
| `directory_table_base` | string | Hex string (`0x...`) | System process CR3 (Directory Table Base). Allows Volatility 3 and MemProcFS to resolve the initial page table without full physical memory scanning. |
| `kpcr_address` | string | Hex string (`0x...`) | Virtual address of the KPCR structure on the executing processor (`__readgsqword(0x18)`). Provided as an analysis hint. |
| `kernel_base` | string | Hex string (`0x...`) | NTOSKRNL base virtual address (`RtlPcToFileHeader(&ZwYieldExecution)`). |
| `kernel_size` | integer | Decimal bytes | NTOSKRNL image size from the PE Optional Header (`SizeOfImage`). |
| `major_version` | integer | Decimal | Windows major version from `PsGetVersion`. |
| `minor_version` | integer | Decimal | Windows minor version from `PsGetVersion`. |
| `build_number` | integer | Decimal | Windows build number (e.g. `19045`, `22631`). |
| `processors` | integer | Decimal | Active logical processor count (`KeQueryActiveProcessorCountEx`). |

---

## 4. Ranges Array (`ranges`)

Each element represents a physical memory range populated by RAM:

| Field | Type | Format | Description |
| :--- | :--- | :--- | :--- |
| `driver_run` | integer | Decimal | Zero-based index of the run reported by `MmGetPhysicalMemoryRangesEx2`. |
| `start` | string | Hex string (`0x...`) | Starting physical address (inclusive). |
| `length` | integer | Decimal bytes | Length of the physical memory run in bytes. |

---

## 5. Unreadable Array (`unreadable`)

Each element represents an isolated unreadable physical memory span:

| Field | Type | Format | Description |
| :--- | :--- | :--- | :--- |
| `start` | string | Hex string (`0x...`) | Starting physical address of the unreadable region. |
| `length` | integer | Decimal bytes | Length of the unreadable span in bytes (typically 4096 or a multiple). |
| `ntstatus` | string | Hex string (`0x...`, 8 chars) | The exact `NTSTATUS` returned by `MmCopyMemory` (e.g. `"0xC0000001"` for `STATUS_UNSUCCESSFUL`, `"0xC000003E"` for `STATUS_DATA_ERROR`). |

---

## 6. Example Document

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
