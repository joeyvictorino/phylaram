# PhylaRAM Engineering Standard

**Document ID:** `PHYLA-ENG-STD-001`  
**Status:** Normative / Canonical  
**Version:** 1.0.0  
**Brand:** PhylaRAM  
**Long Name:** PhylaRAM — Live RAM Capture for Windows  
**Target Systems:** Windows 10 version 2004+ / Windows 11 (x64 only)  
**Implementation Languages:** C (KMDF 1.x Driver), Modern C++ (C++20 CLI), Rust (Offline Verification & Testing)  

---

## 1. Scope and Authority

This document defines the normative engineering standard for the **PhylaRAM** repository. It establishes what production-quality code means for this project, what reviewers must enforce, what constitutes a merge blocker, and what properties every contributor and AI coding agent must preserve.

PhylaRAM is defensive, security-sensitive, systems-level digital forensics physical-memory acquisition software. In this domain:
- Ambiguity produces misinterpretation.
- Hidden state produces irreproducible acquisition.
- Unsafe memory access produces kernel instability or process corruption.
- Silent partial failure produces fabricated evidence.
- Improper error semantics produce false legal and investigative claims.

This standard applies to all human-written and AI-generated source code, driver implementations (`phylaram.sys`), user-mode CLI orchestration (`phylaram.exe`), shared ABI protocols (`shared/phylaram.h`), test suites, offline verification tools (`tools/phylaram-verify`), build scripts, and architectural documentation.

---

## 2. Governing Principles

### 2.1 The Primary Standard

> **Every merged change MUST leave the codebase at least as understandable, testable, secure, and trustworthy as it found it.**

### 2.2 Core Tenets

1. **Correctness outranks convenience.** A faster or shorter path that risks evidence corruption or undefined behavior is defective.
2. **Clarity outranks cleverness.** Code that is difficult to understand is code that cannot be reliably reviewed or maintained.
3. **Evidence integrity outranks successful execution.** An aborted acquisition that preserves truth is superior to a completed acquisition containing silent falsehoods.
4. **Existing bad code is not precedent.** Technical debt in existing files never justifies introducing new debt.
5. **Transparency of trustworthiness.** The source code must not merely produce trustworthy results; it must make it possible to understand *why* those results can be trusted.

### 2.3 The Architectural Synthesis

The engineering philosophy of PhylaRAM combines:
- **Google-style discipline:** Rigorous code review, consistent formatting, strict style enforcement, and clean repository hygiene.
- **Jane Street-style correctness:** Making illegal states unrepresentable, explicit state machines, and type-level invariant enforcement.
- **SQLite-style explanatory care:** Detailed, contextual commentary explaining difficult low-level systems mechanics, platform quirks, and mathematical derivations.
- **Norvig-style semantic clarity:** Compact, direct conceptual models that expose the core algorithm without accidental framework machinery.
- **Rust-style compiler enforcement:** Exhaustive matching, strict ownership semantics, and minimal runtime surprises.
- **Microsoft WDK defensive engineering:** Strict IRQL discipline, validated user buffers, defensive IOCTL handling, and safe physical memory copying.
- **Forensic-grade precision:** Explicit distinction between facts, absences, and interpretations; provable evidence immutability; byte-accurate error isolation.

$$\text{Engineering Standard} = \text{Google Discipline} + \text{SQLite Care} + \text{Norvig Clarity} + \text{Type Enforcement} + \text{Forensic Explicitness}$$

---

## 3. Normative Language

The key words **MUST**, **MUST NOT**, **REQUIRED**, **SHOULD**, **SHOULD NOT**, **MAY**, **BLOCKER**, and **NIT** in this document are to be interpreted as follows:

| Term | Definition | Review & Merge Consequence |
| :--- | :--- | :--- |
| **MUST / REQUIRED** | An absolute technical or forensic requirement. | Non-negotiable. Violation prevents code merge. |
| **MUST NOT** | An absolute technical or forensic prohibition. | Non-negotiable. Violation prevents code merge. |
| **SHOULD** | A strong architectural default. Valid exceptions require explicit justification. | Must be followed unless an explicit architectural waiver is documented. |
| **SHOULD NOT** | A strong architectural anti-pattern. | Permitted only with compelling technical justification. |
| **MAY** | Truly optional implementation choice. | Discretion of the engineer. |
| **BLOCKER** | A code review finding of critical severity (correctness, safety, evidence defect). | Immediate veto. Pull request cannot be merged until resolved. |
| **REQUIRED (Review)** | A finding requiring resolution before merge for maintainability or standards compliance. | Must be addressed prior to final approval. |
| **NIT** | Minor polish or non-blocking suggestion. | Author may address or defer at discretion. |

---

## 4. Code as a Human-Readable System

### 4.1 The Future Reader as Primary Audience

Code is written primarily for human engineers, forensic examiners, instructors, and reviewers who must inspect, verify, and maintain the system under hostile conditions. The compiler is the secondary audience.

A function that executes correctly on the CPU but cannot be readily understood by a qualified reviewer is defective. When boring, direct code is safer and clearer than a sophisticated construct, boring code MUST be chosen.

### 4.2 The Six Review Questions

A qualified reviewer reading any function or module MUST be able to answer:
1. **What does this do?** (Precise domain operation)
2. **Why does it exist?** (Architectural purpose)
3. **Why is it safe?** (Memory, concurrency, and platform guarantees)
4. **What assumptions does it make?** (Preconditions, alignment, state prerequisites)
5. **What happens when it fails?** (Error propagation, resource reclamation, rollback)
6. **What does its result actually mean?** (Forensic and semantic interpretation)

---

## 5. Beautiful Production Code

Beautiful code is not code golf, minimal line counts, dense one-liners, or ornamental abstractions. 

> **Beautiful code expresses the correct domain model with the fewest accidental concepts while preserving every important distinction.**

### 5.1 Attributes of Production Quality

Production code in PhylaRAM MUST demonstrate:
- **Semantic Economy:** Zero unnecessary concepts, classes, or dispatch layers.
- **Directness:** Control flow proceeds linearly and visibly.
- **Precise Naming:** Identifiers describe forensic and technical meaning, not generic mechanics.
- **Visible Invariants:** Assertions, SAL annotations, and type contracts declare bounds explicitly.
- **Explicit Ownership:** Object lifetimes and deallocation responsibilities are deterministic.
- **Explicit Failure Semantics:** Partial success is never reported as total success.
- **Proportionate Commentary:** Difficult kernel, hardware, or arithmetic logic is thoroughly explained.

The ideal reviewer reaction to well-crafted code is:
$$\text{"Of course this is how it works."}$$

---

## 6. The One-Pass Test & Semantic Compression

### 6.1 The One-Pass Test

Important code SHOULD pass **The One-Pass Test**: a qualified systems engineer should understand the function's algorithm, state transitions, failure modes, and result semantics in a single deliberate reading.

If a reader must mentally simulate registers, trace deeply nested indirect jumps, or unravel layered pointer mutations to understand basic control flow, the code suffers from accidental complexity.

### 6.2 Semantic vs. Textual Compression

- **Textual Compression:** Reducing lines, tokens, or characters. (Often harmful)
- **Semantic Compression:** Reducing the number of distinct concepts a reader must hold simultaneously in working memory. (Always preferred)

A good abstraction removes cognitive burden from callers; it does not merely relocate complexity behind an indirection layer.

---

## 7. Domain Representation: Shape of Code

### 7.1 Matching the Domain Model

The structure of the source code MUST mirror the reality of the physical and operational domain:
- Physical memory is non-contiguous, broken by hardware MMIO holes and firmware regions.
- Live kernel memory copy operations can fail at arbitrary 4 KiB page boundaries.
- Operating system memory topology can mutate dynamically during acquisition.

Do not collapse rich domain states into ambiguous primitives (e.g., booleans, magic integers, or nullable tuples).

### 7.2 Explicit Domain Types

Use strong, explicit types to eliminate primitive obsession:

```cpp
// Good: Type-safe domain representation (C++20)
struct PhysicalAddress { uint64_t value; };
struct PhysicalOffset  { uint64_t value; };
struct RunIndex        { uint32_t value; };
struct TransferBytes   { uint32_t value; };

// Bad: Primitive obsession inviting parameter confusion
bool ReadMemory(uint64_t a, uint64_t b, uint32_t c, uint32_t d);
```

---

## 8. Making Illegal States Difficult to Represent

### 8.1 The Correctness Hierarchy

Guarantees in this codebase MUST be enforced at the highest possible tier of the correctness hierarchy:

```
Tier 1: Impossible by Representation (Type system / Newtypes / Tagged Unions)
Tier 2: Rejected by the Type System (Constness / Lifetimes / Move Semantics)
Tier 3: Rejected by Static Analysis (SAL / Compiler Warnings / Clang-Tidy / CodeQL)
Tier 4: Rejected by Automated Tests (Unit / Property-based / Mock Invariant Tests)
Tier 5: Rejected by Runtime Validation (Input Sanitization / Precondition Checks)
Tier 6: Prohibited by Documentation (Developer Guidance / API Comments)
Tier 7: Dependent on Programmer Memory (BANNED FOR CRITICAL INVARIANTS)
```

### 8.2 The Forgetfulness Test

For every critical safety, security, or forensic invariant, ask:
> **"What happens if the next engineer or AI agent completely forgets this rule?"**

If forgetting the rule causes memory corruption, evidence mutation, false completeness reporting, or an unquoted execution vulnerability, enforcement via documentation alone is a **BLOCKER** defect.

---

## 9. Exhaustiveness and State Representation

State machines governing driver lifecycles, memory chunking, and file writing MUST use exhaustive matching.

- Adding a new variant to a domain enumeration MUST break the build at all unhandled call sites.
- Catch-all defaults (e.g., `default:` in C/C++ or `_ => {}` in Rust) MUST NOT be used on semantically rich domain state enums unless an invariant error is raised.

```rust
// Good: Exhaustive matching in Rust offline verification tools
match span_status {
    SpanStatus::Acquired => writer.write_extent(span)?,
    SpanStatus::SparseHole => writer.skip_extent(span)?,
    SpanStatus::Unreadable(status) => {
        provenance.record_unreadable(span, status);
        writer.skip_extent(span)?;
    }
    SpanStatus::Corrupted => return Err(VerificationError::CorruptHeader),
}
```

---

## 10. Forensic Prime Directives

The following directives govern all operations involving evidence, memory acquisition, physical address mapping, and output persistence. They are non-negotiable.

### 10.1 Original Evidence Immutability

Normal forensic operations MUST NOT modify, truncate, overwrite, or touch original evidence.

1. **Explicit Write Classification:** Every first-party filesystem write MUST belong to one of six explicit write classes:
   - `CaseStore`: Permanent forensic case repository.
   - `DerivedArtifact`: Structured evidence extracted or generated during analysis.
   - `Export`: User-requested destination files (e.g., `memory.raw`).
   - `Cache`: Ephemeral performance data that can be safely deleted.
   - `Log`: Diagnostic and operational execution records.
   - `Temp`: Staging files explicitly cleaned up on termination.
2. **Evidence is NOT a Write Class:** The software MUST NOT write beside, inside, or onto source evidence.
3. **Atomic Staging:** Evidence files MUST be written to an explicit temporary staging path (e.g., `<output>.partial`) and promoted to the final target via atomic rename (`MoveFileExW` without overwrite flags) ONLY after the operation reaches a valid terminal state.
4. **Collision Preflight:** PhylaRAM MUST verify that none of the target or staging files exist before starting:
   - `<output>`
   - `<output>.partial`
   - `<output>.map.json`
   - `<output>.map.json.partial`
   - `<output>.sha256`
   - `<output>.sha256.partial`
   It MUST fail immediately if any of these files exist.

### 10.2 Unknown Is Not Zero (`UNREADABLE != ZERO`)

Physical memory that cannot be read is **NOT** zero-filled memory.

$$\text{Unreadable RAM} \neq \text{Observed } \mathtt{0x00} \text{ RAM}$$

- **Physical MMIO / PCIe Gaps:** Unpopulated hardware address space. In the flat RAW image, these are sparse holes.
- **Acquired Zero Pages:** Actual RAM containing observed `0x00` bytes. These are read, written, and recorded as acquired data.
- **Unreadable RAM:** Physical RAM ranges reported by the kernel but unreadable due to hardware fault, VBS/HVCI security boundaries, or hypervisor protection.
- **Provenance Map Invariant:** The `.map.json` sidecar MUST record the exact byte range and NTSTATUS of every unreadable span using schema `phylaram-map-1`. It MUST NEVER classify unreadable memory as acquired zero data.

### 10.3 Completeness Must Be Proven

The acquisition process MUST NOT report success merely because execution completed without a crash.

- **Status Outcomes:**
  - `COMPLETE` (Exit Code `0`): Every physical byte across all enumerated RAM runs was read successfully, written to disk, and verified with zero topology mutations.
  - `INCOMPLETE` (Exit Code `2`): The acquisition reached the end of physical memory, but one or more pages were unreadable (`unreadable_bytes > 0`) OR the physical memory layout changed during acquisition (`topology_changed == true`).
  - `FAILED` (Exit Code `1`): Acquisition was aborted, cancelled by the user, encountered an I/O or SCM error, or violated security constraints.
- **No Upstream Elevation:** Downstream parsers, UI elements, and reporting tools MUST NOT elevate an `INCOMPLETE` or `FAILED` state to `COMPLETE`.

### 10.4 Provenance Sidecar Format (`phylaram-map-1`)

Every acquisition sidecar MUST emit standard provenance metadata:

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

## 11. Comments and Documentation

### 11.1 The Purpose of Comments

Code communicates mechanics to the compiler and reader. Comments preserve vital context, invariants, constraints, and derivations that cannot be expressed in code syntax.

> **Rule:** Code shows the implementation; comments prevent future engineers from having to rediscover the reasoning from first principles.

### 11.2 Standardized Comment Prefixes

When documenting critical architectural contracts, use standardized uppercase tags:

- `INVARIANT:` Declares a mathematical, structural, or state guarantee that must hold true.
- `SAFETY:` Explains why an unsafe operation, raw pointer dereference, or kernel call is sound.
- `FORENSIC:` Documents evidence integrity rules, byte preservation, or map accounting semantics.
- `SECURITY:` Explains an access control boundary, SDDL definition, or privilege validation.
- `COMPAT:` Documents platform-specific workarounds (e.g., Windows 10/11 version behaviors).
- `PERFORMANCE:` Justifies non-obvious optimizations with benchmark evidence.

```c
// FORENSIC: If MmCopyMemory returns partial data, preserve every copied byte.
// The unread remainder must be isolated at 4 KiB page boundaries and recorded
// in the provenance map with its exact NTSTATUS rather than zero-filled.
```

---

## 12. Public API Contracts

Every public function, class, and interface MUST document:
1. **Purpose:** Single conceptual responsibility.
2. **Preconditions / Inputs:** Value ranges, valid pointers, alignment constraints.
3. **Postconditions / Outputs:** Return semantics, output buffers, error states.
4. **Ownership:** Who allocates, retains, and deallocates memory and OS handles.
5. **Failure Modes:** Explicit error codes or exception specifications.
6. **Thread Safety:** Concurrency constraints (e.g., `PASSIVE_LEVEL`, thread-hostile).

---

## 13. Naming Conventions & Namespace Taxonomy

### 13.1 Namespace & Prefix Rules

- **Kernel & Protocol Prefix:** `PHYLA_` is the canonical prefix for all shared protocol constants, structs, IOCTLs, and driver functions.
- **Brand / Tool Name:** `PhylaRAM` in documentation, logs, sidecars, and user-facing output.
- **Binaries:** `phylaram.exe` (CLI) and `phylaram.sys` (Driver).
- **Service Name:** `PhylaRAM` (`PHYLA_SERVICE_NAME`).
- **Device Names:** `\Device\PhylaRAM` (`PHYLA_NT_DEVICE_NAME`) and `\\.\PhylaRAM` (`PHYLA_USER_DEVICE_NAME`).
- **Pool Tag:** `'MLYP'` / `'ALYP'` (Phyla RAM Pool Tag).

### 13.2 Banned Identifiers

Do NOT use vague or uninformative names:
- Banned variables: `data`, `data2`, `tmp`, `temp_buf`, `flag`, `status2`, `val`, `obj`, `misc`.
- Banned verbs: `process()`, `handle()`, `do_work()`, `execute()`, `manage()`.

### 13.3 Domain-Specific Naming Table

| Generic Name | PhylaRAM Domain Name | Context |
| :--- | :--- | :--- |
| `buf` / `data` | `payload_buffer` / `io_transfer_buffer` | Direct I/O memory storage |
| `len` / `size` | `transfer_byte_count` / `run_length_bytes` | Byte metrics |
| `addr` | `physical_address` / `virtual_address` | Address space disambiguation |
| `pos` / `offset` | `file_offset_bytes` / `run_relative_offset` | Offset position |
| `status` | `copy_ntstatus` / `win32_last_error` | Error disambiguation |
| `process_memory()` | `acquire_physical_run()` | Acquisition orchestration |

---

## 14. Functions and Decomposition

### 14.1 The Fundamental Operations

Before building complex abstractions, identify the fundamental operations of the domain. In PhylaRAM physical acquisition, the fundamental loop is:

$$\text{Enumerate Runs} \longrightarrow \text{Request 16 MiB Chunk} \longrightarrow \text{Preserve Copied Bytes} \longrightarrow \text{Isolate Bad Pages} \longrightarrow \text{Commit Extent}$$

### 14.2 Cohesive Functions

- Functions SHOULD perform exactly one conceptual task.
- Length is secondary to cognitive complexity. Extract concepts, not arbitrary line blocks.
- Do not mix validation, low-level I/O, UI printing, and persistence in the same function.

---

## 15. Architectural Symmetry

Operations that create, open, or initialize state MUST have visible, symmetrical counterparts:

| Creation / Allocation | Teardown / Reclamation | Context |
| :--- | :--- | :--- |
| `PhylaSessionBegin` | `PhylaSessionRelease` / `PhylaSessionEnd` | Driver Session Lifecycle |
| `OpenSCManagerW` + `CreateServiceW` | `ControlService(STOP)` + `DeleteService` | SCM Driver Management |
| `RawWriter::PreflightAndOpen` | `RawWriter::FlushAndClose` | Sparse RAW Output |
| `BCryptCreateHash` | `BCryptDestroyHash` | Cryptographic Hash State |
| `Acquire` | `Release` | Resource Locks |

---

## 16. The Cleverness Budget and Proof of Magic

Non-obvious techniques consume the **Cleverness Budget** and are permitted ONLY when achieving a material correctness, security, or performance requirement.

Any non-obvious low-level construct MUST be accompanied by:
1. An explicit `INVARIANT:` comment.
2. An authoritative citation (Microsoft DDI, Intel SDM, RFC).
3. A formal proof or derivation of arithmetic correctness.
4. Dedicated boundary unit tests covering all edge cases.

---

## 17. Input Validation and Arithmetic Safety

- All input crossing a trust boundary (CLI arguments, IOCTL buffers, memory descriptors) MUST be validated before use.
- All 64-bit arithmetic involving addresses, offsets, lengths, array counts, and buffer allocations MUST be mathematically checked for overflow, underflow, and truncation:

```c
// Defensive 64-bit bounds check
if (request_offset > run_length ||
    request_length > run_length - request_offset ||
    request_offset > MAXULONGLONG - run_base) {
    return STATUS_INVALID_PARAMETER;
}
```

---

## 18. Error Handling and Failure Semantics

- Errors are an integral part of the API contract and MUST NOT be swallowed or collapsed into generic error codes.
- Preserve exact error sources: distinguish `NTSTATUS` from Win32 `DWORD` error codes.
- Capture `GetLastError()` immediately after a Win32 API failure before calling any subsequent API that might overwrite it.
- Malformed external input MUST NEVER trigger an assertion failure, process abort, or kernel bugcheck.

---

## 19. Resource Ownership and Lifetime Management

In user-mode C++, all OS resources (file handles, SCM handles, security identifiers, CNG crypto providers) MUST be managed using RAII wrappers adhering to the **Rule of 5/0**.

- Raw handles (`HANDLE`, `SC_HANDLE`, `PSID`) MUST NOT be manually closed across complex branching logic.
- Copy constructors and copy assignment operators for resource-owning classes MUST be explicitly deleted (`= delete`).
- Move constructors and move assignment operators MUST transfer ownership cleanly and nullify the donor.

```cpp
// Mandatory RAII Pattern for Win32 Handles
template <typename HandleType, typename DeleterType, HandleType InvalidValue = nullptr>
class UniqueWin32Handle {
public:
    explicit UniqueWin32Handle(HandleType h = InvalidValue) noexcept : handle_(h) {}
    ~UniqueWin32Handle() noexcept { Reset(); }
    UniqueWin32Handle(const UniqueWin32Handle&) = delete;
    UniqueWin32Handle& operator=(const UniqueWin32Handle&) = delete;
    UniqueWin32Handle(UniqueWin32Handle&& o) noexcept : handle_(o.Release()) {}
    UniqueWin32Handle& operator=(UniqueWin32Handle&& o) noexcept {
        if (this != &o) Reset(o.Release());
        return *this;
    }
    // ...
};
```

---

## 20. Concurrency and Cancellation

- Concurrency requires justification. Do not introduce background threads when sequential processing satisfies requirements.
- Signal handlers (e.g., `ConsoleHandler` on Ctrl+C) MUST interact with running worker threads strictly through atomic flags (`std::atomic_bool`) and thread-safe cancellation tokens.

---

## 21. Language-Specific Engineering Rules

### 21.1 C / KMDF Windows Kernel Driver (`driver/`)

1. **Non-PnP Control Driver:** Must use `WdfControlDeviceInitAllocate` with restrictive SDDL `D:P(A;;GA;;;SY)(A;;GA;;;BA)` and exclusive access (`WdfDeviceInitSetExclusive(init, TRUE)`).
2. **`WDFDEVICE_INIT` Ownership:**
   - If any step fails before `WdfDeviceCreate`: call `WdfDeviceInitFree(init)`.
   - If `WdfDeviceCreate` fails: call `WdfDeviceInitFree(init)`.
   - If `WdfDeviceCreate` succeeds: `init` is consumed (KMDF sets `init = NULL`). If subsequent steps fail, call `WdfObjectDelete(device)`.
3. **Buffer Access Protocol:**
   - Query and lifecycle IOCTLs: `METHOD_BUFFERED`.
   - High-throughput physical memory transfer IOCTL: `METHOD_OUT_DIRECT`.
   - MDL Mapping: Retrieve MDL using `status = WdfRequestRetrieveOutputWdmMdl(Request, &mdl)` and map safely using `MmGetSystemAddressForMdlSafe(mdl, NormalPagePriority | MdlMappingNoExecute)`.
   - Output Safety: Force `bytesReturned = 0` on any error status.
4. **Physical Memory Copying:**
   - Must use `MmCopyMemory` with `MM_COPY_MEMORY_PHYSICAL` at `IRQL <= APC_LEVEL` (`WdfExecutionLevelPassive`).
   - Must preserve exact `NumberOfBytesTransferred`.
5. **Idempotent Session Cleanup:**
   - Implement `PhylaSessionRelease(Context)` that atomically detaches `Context->Ranges` before calling `ExFreePool`.
   - Register both `EvtFileCleanup` (early release) and `EvtFileClose` (fallback release).
6. **Prohibited Kernel Techniques (INSTANT MERGE BLOCKERS):**
   - `MmMapIoSpace` for ordinary physical RAM.
   - `\Device\PhysicalMemory` direct handles.
   - Page Table Entry (PTE) or CR3 manipulation.
   - Arbitrary kernel virtual read/write interfaces.
   - Arbitrary physical address IOCTLs (caller may only supply validated Run Index + Offset).
   - Hooking SSDT, IDT, or kernel routines.
   - Disabling HVCI, VBS, or Secure Boot.

### 21.2 Modern C++ (C++20 CLI Application, `cli/`)

1. **Standard & Conformance:** Compiled with MSVC `/std:c++20`, `/permissive-`, `/W4`, `/WX`, and `/guard:cf`.
2. **Secure Driver Extraction:**
   - Extract `phylaram.sys` into `%ProgramData%\PhylaRAM\Temp\` with restrictive SDDL `D:P(A;;GA;;;SY)(A;;GA;;;BA)`.
   - Use cryptographically unpredictable filenames (`phylaram-<random>.sys`).
   - Create with `CREATE_NEW`, write completely, flush, close, and pass quoted path `L"\"" + path + L"\""` to `CreateServiceW`.
   - Teardown: Stop service $\to$ Delete service $\to$ Delete temporary driver file $\to$ Remove temp directory.
3. **Filesystem Preflight Policy:**
   - Query volume flags via `GetVolumeInformationW`.
   - If sparse is supported: verify free disk space $\ge \text{physicalBytes} + 64\text{ MiB}$ and apply `FSCTL_SET_SPARSE`.
   - If non-sparse (e.g. FAT32/exFAT): verify filesystem can represent `HighestPhysicalEnd` (reject FAT32 if $>4\text{ GB}$) and verify free disk space $\ge \text{logicalSize} + 64\text{ MiB}$.
4. **Heap Churn Elimination:**
   - Reusable I/O buffers (16 MiB + 32 B) MUST be pre-allocated for transfer loops.
5. **Console Output:**
   - Do not intermix narrow (`std::cout`) and wide (`std::wcout`) standard output streams.

### 21.3 Rust (Offline Verification, Property Tests & Tooling, `tools/phylaram-verify/`)

1. **Safety:** `#![forbid(unsafe_code)]` in verification tooling crates.
2. **Error Handling:** Minimal `unwrap()` or `expect()`. Production paths MUST propagate typed errors using `Result<T, E>`.
3. **Testing:** Extensive property-based testing with `proptest` for range algebra, integer boundaries, and sparse hash equivalence.

### 21.4 Shared ABI and Protocol Contracts (`shared/phylaram.h`)

1. **Explicit Fixed-Width Types:** Structures MUST use explicit integer types (`ULONG`, `ULONGLONG`, `LONG`, `uint32_t`, `uint64_t`).
2. **Struct Alignment & Static Assertions:** All shared structures MUST be validated via `static_assert` / `C_ASSERT` for exact size and 8-byte natural alignment.
3. **Versioning:** Structures MUST contain a `Version` field set to `PHYLA_PROTOCOL_VERSION` (1u).

---

## 22. Dependencies and Supply Chain

- **Shipped Binaries (`phylaram.exe` + `phylaram.sys`):** MUST NOT depend on external third-party libraries (no Boost, Qt, OpenSSL, protobuf, or `nlohmann/json`).
- Shipped executable relies strictly on:
  - Windows SDK / Win32 API (`bcrypt.lib`, `advapi32.lib`)
  - Dynamic `ntdll!RtlGetVersion` resolution (no `ntdll.lib` link dependency)
  - Windows Driver Frameworks (KMDF) / WDK
  - C/C++ Standard Library
- External dependencies in test/verification tools (Rust) must be vetted, locked in `Cargo.lock`, and minimal.

---

## 23. Testing Strategy and Validation Gates

```
+-------------------------------------------------------------------------------+
|                           PhylaRAM Test Hierarchy                             |
+-------------------------------------------------------------------------------+
|  1. Local macOS Tests (C++20 / CMake & Rust Cargo)                            |
|     - 64-bit Range algebra boundary & integer wrap tests                      |
|     - Mock driver fault-injection state machine (partial reads, bad ECC pages)|
|     - JSON Provenance schema validation & round-trip tests (phylaram-map-1)   |
|     - CLI parser, collision preflight & flag combination tests               |
|     - Rust proptest property tests & streaming SHA-256 validation             |
|                                                                               |
|  2. Windows Acceptance Validation Gates (MSVC / WDK / Dedicated VM)           |
|     - Gate 1: Strict toolchain build (/W4 /WX /guard:cf /Qspectre)            |
|     - Gate 2: Static Driver Verifier (SDV) & CodeQL Driver Suite              |
|     - Gate 3: Dynamic Driver Verifier (DV) 100-cycle stress profile           |
|     - Gate 4: Hardware & RAM topology matrix (4GB to 128GB, ReBAR, NUMA)      |
|     - Gate 5: Security controls active (Secure Boot, VBS, HVCI, Defender)     |
|     - Gate 6: Downstream DFIR tool parsing (Volatility 3, MemProcFS)          |
+-------------------------------------------------------------------------------+
```

---

## 24. Code Review Discipline

### 24.1 Severity Definitions

- **BLOCKER:** Cannot merge. Correctness defect, evidence mutation, vulnerability, undefined behavior, unsafe buffer access, false completeness, unclassified filesystem write.
- **REQUIRED:** Must be corrected before merge. Misleading naming, missing API contract, unnecessary complexity, insufficient tests.
- **NIT:** Minor polish. Non-blocking suggestion.

### 24.2 The Read-Aloud Test & The Deletion Question

- Logic must be narratable in domain terminology rather than mechanical syntax.
- Reviewers must ask: *"What code could disappear if the underlying domain model were better?"*

---

## 25. The Normative Merge Checklist

Before any pull request can be merged into PhylaRAM, the author and reviewer MUST verify that every applicable question below is answered affirmatively:

```markdown
### PhylaRAM Production Merge Checklist

#### 1. Architecture & Domain Model
- [ ] Does the change leave the codebase at least as understandable and testable as it found it?
- [ ] Does the code pass the One-Pass Test?
- [ ] Are all identifiers descriptive of domain concepts under the `PHYLA_` namespace?
- [ ] Is illegal state impossible to represent or rejected by the type system?
- [ ] Are state machine enumerations matched exhaustively without inappropriate catch-all branches?

#### 2. Forensic & Evidence Integrity
- [ ] Is original evidence provably immutable (no modification, truncation, or sidecar colocation)?
- [ ] Are all first-party filesystem writes explicitly classified (CaseStore, DerivedArtifact, Export, Cache, Log, Temp)?
- [ ] Is unreadable RAM strictly distinguished from observed 0x00 zero RAM?
- [ ] Does the .map.json sidecar conform to schema `phylaram-map-1` and record all unreadable spans with NTSTATUS?
- [ ] Is partial success accurately reported as INCOMPLETE (Exit Code 2), never upgraded to COMPLETE?
- [ ] Is file offset == physical address maintained for the flat RAW output?
- [ ] Are all 6 target and staging paths preflighted against collisions prior to execution?
- [ ] Are outputs staged to .partial and promoted atomically via MoveFileExW without overwrite flags?

#### 3. Kernel & System Safety
- [ ] Are all prohibited kernel techniques absent (no MmMapIoSpace, no \Device\PhysicalMemory, no PTE editing)?
- [ ] Does WdfDeviceCreate cleanup correctly call WdfDeviceInitFree on failure?
- [ ] Are all user-mode IOCTL buffers validated for size, version, and capacity before access?
- [ ] Is METHOD_OUT_DIRECT MDL retrieved and mapped safely with MdlMappingNoExecute?
- [ ] Are all 64-bit arithmetic operations on addresses, lengths, and offsets guarded against overflow?
- [ ] Is PhylaSessionRelease idempotent and registered in both EvtFileCleanup and EvtFileClose?
- [ ] Is the driver extracted to a secure directory with restricted SDDL and quoted in CreateServiceW?

#### 4. Engineering Quality & Memory Safety
- [ ] Are all Win32, SCM, and crypto handles managed via deterministic RAII wrappers?
- [ ] Do resource-owning classes delete copy operations and implement safe move semantics (Rule of 5/0)?
- [ ] Are reusable I/O buffers utilized to prevent multi-gigabyte heap churn?
- [ ] Are public API contracts fully documented with invariants, safety constraints, and failure modes?
- [ ] Are comments accurate, synchronized, and tagged with appropriate prefixes (INVARIANT, SAFETY, FORENSIC)?

#### 5. Verification, Tests & Build
- [ ] Does the code compile cleanly with zero warnings treated as errors (/W4 /WX)?
- [ ] Are unit tests, property tests, or mock acquisition tests included for new or modified logic?
- [ ] Does malformed or hostile input fail safely without process aborts or bugchecks?
- [ ] Are documentation and architecture specifications synchronized with behavioral changes?
```

---

## 26. Precedence Hierarchy

If conflicts arise between different documents or preferences, the following order of precedence is strictly authoritative:

1. **Forensic Correctness and Evidence Integrity**
2. **System Security and Kernel Memory Safety**
3. **This Engineering Standard (`ENGINEERING_STANDARD.md`)**
4. **Explicit Architectural Specifications (`.agents/agents/urc-architect/agent.md`)**
5. **Platform Toolchain & Language Standards (WDK DDI / ISO C++20 / Rust)**
6. **Existing Repository Consistency**
7. **Personal Developer Preference**

---

## 27. Closing Invariant

PhylaRAM does not pursue beautiful code as superficial decoration. It pursues beauty because clarity is a fundamental form of correctness.

The best source code eliminates accidental complexity while preserving essential physical reality. The source code must explain itself where possible, comments must preserve reasoning the code cannot express, tests must mathematically prove critical invariants, and the type architecture must make catastrophic forensic errors impossible to represent.

> **PhylaRAM captures x64 Windows physical memory to a flat raw image while preserving physical offsets and explicitly identifying unreadable acquisition ranges. Its source code makes it transparently obvious why those results can be trusted.**
