# PhylaRAM Forensic Integrity Model

**Project:** PhylaRAM `0.1.0-alpha`  
**Status:** Architecture contract; production validation gates remain open

This document describes the invariants PhylaRAM is designed to preserve. It does not claim legal admissibility, external certification, or completion of the repository's open hardware/runtime validation gates.

---

## 1. Prime Directives

### 1.1 Physical address preservation

For populated physical RAM:

```text
RAW file offset == physical address
```

Physical-address holes that are not reported as RAM are left unwritten and may be represented as sparse filesystem extents.

### 1.2 `UNREADABLE != ZERO`

Unreadable memory is not factual zero-valued memory.

The flat RAW format has no “unknown byte” symbol. Therefore the logical bytes in an unreadable interval are zero-backed by the file representation, while the provenance map records that the interval was unreadable and preserves its NTSTATUS.

A consumer that ignores the map loses this distinction. For that reason PhylaRAM does not support provenance-free stdout capture as a finalized evidence mode.

### 1.3 Completeness is explicit

A finalized bundle is `complete` only when:

1. the acquisition reaches the end of every frozen physical run;
2. physical byte accounting balances;
3. the session-end topology comparison succeeds and reports no change;
4. no physical bytes remain unreadable;
5. SHA-256 finalization succeeds;
6. the staged RAW image flushes successfully;
7. the provenance map and SHA sidecar are durably written;
8. final publication succeeds.

A topology change or unreadable memory produces `incomplete`. Cancellation or internal failure does not become complete merely because some bytes were acquired.

### 1.4 Facts are separate from interpretations

The canonical provenance map contains acquisition observations and accounting. Derived analytics, heuristics, threat classifications, and compliance mappings are not acquisition facts and do not belong in the canonical map.

---

## 2. Kernel Acquisition Contract

The driver snapshots physical-memory runs with `MmGetPhysicalMemoryRangesEx2` when a file session begins. The snapshot is immutable for that session even though system topology may change later.

User mode cannot request an arbitrary physical address. `READ_RUN` identifies:

- a frozen run index;
- an offset within that run;
- a requested length bounded by `PHYLA_MAX_TRANSFER`.

The driver validates the request before deriving the physical address and invokes `MmCopyMemory` with `MM_COPY_MEMORY_PHYSICAL`.

`MmCopyMemory`'s operation status and exact `NumberOfBytesTransferred` are distinct information. The protocol preserves both so a partial physical read is not collapsed into either total success or total failure.

The control device is exclusive and restricted to SYSTEM and Built-in Administrators. Device-level KMDF synchronization serializes file cleanup with queue callbacks so the file-owned frozen range snapshot cannot be freed while an IOCTL uses it.

---

## 3. Fault Isolation

The user-mode acquisition engine begins with bounded bulk reads up to 16 MiB.

When a bulk read is partial:

```text
preserve the bytes actually returned
-> advance by exactly those bytes
-> isolate the unresolved window at page boundaries
-> preserve successful page fragments
-> record zero-byte unreadable intervals with NTSTATUS
-> continue acquisition
```

The engine does not replace a partial transfer with fabricated bytes.

Unreadable spans are accumulated in physical-address order and may be coalesced only when they are contiguous and have the same status.

---

## 4. Hashing and RAW Representation

SHA-256 is mandatory for finalized captures.

The logical hash covers the exact logical RAW byte stream:

- acquired physical bytes are hashed at their physical offsets;
- unpopulated physical-address holes contribute logical zeros;
- unreadable intervals contribute representation zeros;
- trailing logical space to `logical_size` contributes representation zeros.

This means a normal file hash of the logical RAW file should match the map and sidecar digest.

The digest establishes byte consistency. It does not independently establish source-host identity or chain of custody.

---

## 5. Evidence Publication Transaction

CLI and GUI use the same publication transaction.

The transaction:

1. rejects collisions with existing final or staging paths;
2. queries the frozen topology;
3. creates the staged RAW destination;
4. initializes SHA-256;
5. performs acquisition;
6. finalizes SHA-256;
7. flushes and closes the staged RAW image;
8. proves `acquired_bytes + unreadable_bytes == physical_bytes` with checked arithmetic;
9. writes and flushes staged map/hash sidecars;
10. promotes sidecars;
11. publishes the canonical RAW name last.

Known failure edges remove staging state and any sidecars already promoted by the current process. The application must not emit a successful terminal result when a required publication step fails.

The filesystem does not provide a single transaction spanning three independent files. A sudden system crash at an arbitrary instruction can therefore leave recoverable filesystem artifacts. This is a representation/recovery condition, not permission to report success after an observed finalization failure.

---

## 6. Independent Offline Verification

`phylaram-verify` independently validates the supplied bundle rather than trusting producer totals.

Its current checks include:

- exact logical RAW length;
- map producer/schema/hash syntax;
- non-zero physical range lengths;
- checked range-end arithmetic;
- range ordering and non-overlap;
- exact highest-range-end/logical-size relationship;
- unique zero-based driver-run domain;
- checked physical-byte sum;
- checked acquired/unreadable accounting;
- unreadable span ordering and non-overlap;
- checked unreadable-end arithmetic;
- containment of each unreadable span in one physical RAM run;
- checked unreadable-byte sum;
- zero-backed RAW representation for every span declared unreadable;
- complete/incomplete status consistency;
- SHA-256 equality against the logical RAW and optional sidecar.

The verifier does not claim that metadata is authentic merely because its internal relationships are consistent.

---

## 7. Kernel Hints

The driver may provide acquisition-time hints for downstream analysis, including System-process CR3, executing-CPU KPCR, NTOSKRNL base/size, Windows build, processor count, and the CPUID hypervisor-present bit.

These are hints. In particular, a hypervisor-present bit does not prove VBS or HVCI is active.

---

## 8. Validation Boundaries

The architecture and automated tests do not replace real Windows validation. Driver Verifier stress, physical RAM/topology matrices, production Microsoft signing under Secure Boot/HVCI, and real Volatility/MemProcFS interoperability remain separate gates until they are actually run and recorded.

See [`STATUS.md`](STATUS.md), [`../tests/TEST_PLAN.md`](../tests/TEST_PLAN.md), and the repository's open validation issues.
