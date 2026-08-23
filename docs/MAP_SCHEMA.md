# PhylaRAM Provenance Map Schema (`phylaram-map-2`)

**Schema identifier:** `phylaram-map-2`  
**Repository status:** Canonical schema for the current alpha writer  
**Production validation status:** Not yet complete  
**File extension:** `.map.json`

This document defines the provenance sidecar emitted for a **finalized** PhylaRAM capture. It is a serialization contract, not a claim of production certification or external endorsement.

A finalized bundle contains:

```text
<output>.raw
<output>.raw.map.json
<output>.raw.sha256
```

Hash-free or provenance-free finalized captures are not part of the current product contract.

---

## 1. Forensic Semantics

### Physical addressing

For populated physical RAM:

```text
RAW file offset == physical address
```

The logical RAW size equals the highest end address of the frozen physical-memory ranges.

### `UNREADABLE != ZERO`

The flat RAW representation cannot encode an “unknown byte” symbol. Unreadable physical-memory intervals therefore read as zero bytes in the logical RAW representation, but those representation bytes **MUST NOT** be interpreted as observed zero-valued RAM.

The `unreadable` array is the authoritative semantic distinction. Every unreadable interval records its physical start, length, and observed NTSTATUS.

The offline verifier checks both the provenance geometry and that the RAW representation is zero-backed for intervals declared unreadable.

### Facts, not interpretations

This map records acquisition facts. It does not contain sampled entropy classifications, ATT&CK mappings, compliance claims, heuristic threat labels, or other derived interpretations.

---

## 2. Top-Level Fields

| Field | Type | Contract |
| --- | --- | --- |
| `producer` | string | Exactly `"PhylaRAM"`. |
| `producer_version` | string | Producer version that emitted the map. |
| `schema` | string | Current writer emits `"phylaram-map-2"`. |
| `status` | string | Finalized terminal state: `"complete"` or `"incomplete"`. |
| `logical_size` | integer | Highest end address among physical-memory ranges; also exact logical RAW file size. |
| `physical_bytes` | integer | Checked sum of all reported physical-memory run lengths. |
| `acquired_bytes` | integer | Physical RAM bytes successfully returned and persisted. |
| `unreadable_bytes` | integer | Checked sum of all unreadable span lengths. |
| `topology_changed` | boolean | Whether session-end physical topology differed from the frozen session-start snapshot. |
| `sha256` | string | Exactly 64 hexadecimal characters: SHA-256 of the complete logical RAW byte representation. |
| `kernel_hints` | object, optional | Acquisition-time analysis hints, omitted if the query was unavailable. |
| `ranges` | array | Frozen physical-memory ranges. |
| `unreadable` | array | Ordered non-overlapping unreadable intervals contained within reported physical ranges. |

The following accounting invariant is mandatory:

```text
acquired_bytes + unreadable_bytes == physical_bytes
```

Arithmetic used to establish these relationships must be checked for overflow.

---

## 3. Terminal Status

Only successfully finalized bundles have final map files.

| Status | Producer exit | Required condition |
| --- | ---: | --- |
| `complete` | `0` | `unreadable_bytes == 0` and `topology_changed == false` |
| `incomplete` | `2` | `unreadable_bytes > 0` or `topology_changed == true` |

A cancelled or failed operation does not publish a successful final map as though it were a finalized evidence bundle. Staging artifacts are cleanup state, not a third final map status.

---

## 4. `ranges`

Each entry represents one physical-memory run from the frozen kernel snapshot:

| Field | Type | Meaning |
| --- | --- | --- |
| `driver_run` | integer | Original zero-based run index used by the driver protocol. |
| `start` | string | Physical start address in hexadecimal `0x...` form. |
| `length` | integer | Positive length in bytes. |

Verifier invariants include:

- length is non-zero;
- `start + length` does not overflow `u64`;
- ranges are ordered by physical start and do not overlap;
- each range ends at or before `logical_size`;
- the maximum range end equals `logical_size`;
- checked sum of lengths equals `physical_bytes`;
- `driver_run` indices are unique and form the expected zero-based domain.

---

## 5. `unreadable`

Each entry represents physical RAM that the acquisition path could not read:

| Field | Type | Meaning |
| --- | --- | --- |
| `start` | string | Physical start address in hexadecimal `0x...` form. |
| `length` | integer | Positive unreadable length in bytes. |
| `ntstatus` | string | Eight-hex-digit NTSTATUS representation, e.g. `0xC0000001`. |

Verifier invariants include:

- length is non-zero;
- `start + length` is checked for overflow;
- spans are ordered and non-overlapping;
- every span is wholly contained in one reported physical-memory run;
- checked sum of lengths equals `unreadable_bytes`;
- RAW bytes in each declared unreadable interval are zero-backed in the flat representation.

The zero-backed representation check does not convert the semantic state into “observed zero.” The map continues to mean “unknown/unreadable at acquisition time.”

---

## 6. `kernel_hints`

When available, the current producer may record:

| Field | Meaning |
| --- | --- |
| `hypervisor_present` | CPUID hypervisor-present bit. This does not prove VBS/HVCI state. |
| `directory_table_base` | System-process CR3 / page-table root captured during acquisition. |
| `kpcr_address` | KPCR virtual address for the processor executing the query; not necessarily CPU 0. |
| `kernel_base` | NTOSKRNL virtual base discovered by the kernel helper. |
| `kernel_size` | NTOSKRNL PE image size. |
| `major_version` | Windows major version. |
| `minor_version` | Windows minor version. |
| `build_number` | Windows build number. |
| `processors` | Active logical processor count. |

These values are analysis hints. Downstream tooling should validate them rather than treating their presence as proof of a broader security or platform state.

---

## 7. Example

```json
{
  "producer": "PhylaRAM",
  "producer_version": "0.1.0-alpha",
  "schema": "phylaram-map-2",
  "status": "incomplete",
  "logical_size": 12288,
  "physical_bytes": 12288,
  "acquired_bytes": 8192,
  "unreadable_bytes": 4096,
  "topology_changed": false,
  "sha256": "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
  "ranges": [
    {"driver_run": 0, "start": "0x0", "length": 12288}
  ],
  "unreadable": [
    {"start": "0x1000", "length": 4096, "ntstatus": "0xC0000001"}
  ]
}
```

The hash in this example is illustrative only.

---

## 8. Compatibility

The verifier currently retains read compatibility with `phylaram-map-1` where its fields satisfy the same structural invariants. The current producer emits only `phylaram-map-2`.

Any future schema change that alters field meaning, terminal semantics, geometry, hashing, or unreadable representation requires an explicit schema/version decision. Existing fields must not be silently reinterpreted.
