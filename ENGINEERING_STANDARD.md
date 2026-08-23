# PhylaRAM Engineering Standard

**Status:** Normative repository engineering standard  
**Applies to:** Human-written and AI-generated production code, tests, tooling, build logic, documentation, and code review  
**Project:** PhylaRAM  
**Primary platform:** Windows 10 version 2004 or later and Windows 11, x64  

This document defines what production-quality engineering means for PhylaRAM. It is intentionally stricter than a formatting guide. It governs semantic correctness, forensic integrity, security, lifetime, ownership, testing, review, and the use of language and platform contracts.

This standard is informed by secure-coding principles associated with Robert C. Seacord and the SEI CERT secure coding standards, by language specifications, by Microsoft Windows and WDK engineering guidance, and by engineering traditions that value explicit reasoning, small complete cores, and readable systems code. No external person or organization has approved, endorsed, or certified this repository or this document.

The objective is Seacord-grade engineering, not borrowed branding.

---

## 1. Repository Reality

This standard is grounded in the repository as it exists.

### 1.1 Languages and authoritative modes

| Area | Language / mode | Repository authority |
| --- | --- | --- |
| `driver/` | C17, KMDF 1.15 | `driver/phylaram.vcxproj` |
| `cli/` and Windows GUI | C++20, MSVC v143 | `cli/phylaram.vcxproj` |
| `shared/phylaram.h` | C/C++ shared native ABI | compile-time size assertions plus Windows ABI |
| `tools/phylaram-verify/` | Rust 2021 edition | `Cargo.toml` |
| portable test programs | C++20 | CI commands and `tests/` |
| validation/tooling scripts | Python 3, PowerShell, Windows batch | scripts and CI runner environments |
| CI/release automation | GitHub Actions YAML | `.github/workflows/` |

The exact Python and PowerShell interpreter minor versions are not pinned by this repository. Code in those languages MUST therefore remain within the behavior guaranteed by the versions actually selected by the supported CI/host environments, or the repository MUST pin a version before depending on narrower behavior.

### 1.2 Supported product boundary

PhylaRAM is a Windows x64 physical-memory acquisition system. Its high-risk path crosses several trust and privilege boundaries:

1. command-line, GUI, filesystem, environment, and operator input into an elevated user-mode process;
2. elevated user mode into the KMDF kernel driver through IOCTLs;
3. the embedded driver resource into a protected temporary file and the Service Control Manager;
4. live physical-memory observations into a staged RAW evidence representation;
5. staged evidence into finalized RAW, map, and SHA-256 sidecars;
6. finalized evidence and JSON sidecars into the independent Rust verifier;
7. repository source and dependency metadata into CI, build, signing, and release infrastructure.

User-controlled paths, command-line input, IOCTL payloads, provenance JSON, SHA sidecars, and RAW files presented to the verifier are untrusted until validated.

The kernel driver, acquisition engine, evidence finalization transaction, shared ABI, provenance-map writer, offline verifier, service/driver lifecycle, and cryptographic hashing path are security- and forensic-critical.

C and C++ portions are memory-unsafe by language model. Rust is memory-safe only to the extent that unsafe code, FFI, resource lifetime, concurrency, and semantic invariants remain correct.

### 1.3 Repository forensic model

The canonical finalized evidence bundle is:

```text
memory.raw
memory.raw.map.json
memory.raw.sha256
```

The acquisition model preserves these core distinctions:

- `file offset == physical address` for populated physical memory;
- unpopulated physical-address holes are representation holes, not observed RAM;
- `UNREADABLE != ZERO` as forensic meaning;
- unreadable RAM is represented explicitly in the provenance map even though the flat RAW logical representation reads zero bytes at those positions;
- complete and incomplete are distinct terminal outcomes;
- hashing is mandatory for a finalized PhylaRAM evidence bundle;
- presentation layers do not redefine acquisition truth.

---

## 2. Normative Language

**MUST / MUST NOT** means a mandatory requirement. Violation prevents merge unless formally waived under the exception process.

**SHOULD / SHOULD NOT** means a strong default. Departure requires a specific engineering justification that survives review.

**MAY** means permitted.

**BLOCKER** means the change cannot merge.

**REQUIRED** means the issue must be addressed before merge for engineering-quality reasons, but is not necessarily a vulnerability or correctness failure.

**NIT** means optional, non-blocking polish.

Not every preference is normative. The authority of this standard depends on distinguishing correctness and risk from taste.

---

## 3. Source-of-Truth and Precedence

When deciding whether behavior is legal, defined, safe, portable, or compatible, use this hierarchy:

1. governing language standard or specification;
2. governing platform ABI specification;
3. authoritative platform or API documentation;
4. applicable CERT secure-coding rule or recommendation;
5. compiler, linker, analyzer, or toolchain documentation;
6. explicit PhylaRAM architecture and forensic contracts;
7. this engineering standard;
8. established local implementation convention;
9. personal preference.

For merge decisions, correctness and defined program semantics come first, followed by forensic integrity, security and safety, authoritative specifications, explicit architecture contracts, this standard, repository consistency, and finally personal preference.

No local style preference may justify unsafe, undefined, ambiguous, or semantically incorrect behavior.

The following are never evidence of correctness by themselves:

- “It works on my machine.”
- “The compiler accepts it.”
- “Existing code already does this.”
- “The test happened to pass once.”

Existing defects are technical debt, not precedent.

---

## 4. Primary Engineering Law

Every merged change **MUST** leave the codebase at least as understandable, analyzable, testable, secure, and trustworthy as it found it.

The governing priorities are:

- Correctness outranks convenience.
- Defined behavior outranks accidental behavior.
- Clarity outranks cleverness.
- Security properties are designed and enforced, not assumed.
- Evidence integrity outranks successful execution.
- A program that produces the right answer for the wrong reason is defective.
- A defect prevented by construction is preferable to one prevented by convention.
- Facts and interpretations remain distinct.

---

## 5. Beautiful Code

For PhylaRAM:

> **Beautiful code expresses the correct model with the fewest accidental concepts while preserving every important distinction.**

Beautiful code is not code golf, compressed syntax, decorative abstraction, pattern accumulation, framework construction, maximum comment density, mathematical display, or language-feature exhibition.

Beautiful production code is:

- **Correct:** behavior follows the governing language and platform contracts.
- **Defined:** it does not rely on undefined behavior, stale lifetime, invalid arithmetic, races, or undocumented assumptions.
- **Direct:** the path from request to result is visible.
- **Semantic:** names and types describe the forensic or systems concept.
- **Small:** unnecessary states, helpers, wrappers, callbacks, and mechanisms have been removed.
- **Complete:** real failures and edge cases remain represented.
- **Explicit:** ownership, units, bounds, state, trust, mutation, and failure are understandable.
- **Enforced:** critical properties do not depend only on programmer memory.
- **Symmetrical:** lifecycle pairs such as open/close and start/stop are recognizable.
- **Analyzable:** compilers, static analyzers, tests, and reviewers can reason about it.
- **Proportionately documented:** difficult reasoning is preserved where rediscovery would be costly.
- **Boring where boring is safer:** conventional mechanisms are preferred when equally correct.
- **Clever only under proof:** unusual mechanisms carry increased proof obligations.

The preferred reviewer reaction is:

> Of course this is how it works.

### 5.1 One-Pass Test

Important code **SHOULD** be understandable in one deliberate reading where inherent domain complexity permits. After one reading, a qualified engineer should normally be able to explain:

1. what operation occurs;
2. why it exists;
3. the major phases;
4. state consumed;
5. state produced;
6. failure modes;
7. what the result means.

If that requires mentally simulating individual instructions, investigate weak naming, hidden state, primitive obsession, unnecessary mutation, deep nesting, overloaded responsibilities, or indirection.

### 5.2 Read-Aloud Test

Important code **SHOULD** narrate in domain language. For example:

> Validate the READ_RUN request. Resolve the frozen physical run. Prove the requested interval is inside it. Copy physical bytes. Preserve the exact transferred byte count and NTSTATUS. Return the classified result.

If narration is dominated by “toggle flag,” “call helper four,” or “mutate context,” reconsider the model.

### 5.3 Semantic compression

Prefer fewer concepts, not fewer characters. A good abstraction removes knowledge from callers. A bad abstraction only relocates complexity.

Optimize for maximum meaning per concept, not minimum lines of code.

### 5.4 Shape of code equals shape of problem

If the domain has complete, incomplete, cancelled, and failed outcomes, represent those outcomes. Do not reconstruct them later from unrelated booleans, nullable values, magic numbers, or arbitrary strings.

Repository-appropriate semantic concepts include `MemoryRun`, `UnreadableSpan`, `ReadResult`, `KernelHints`, `EvidenceCaptureStatus`, and `EvidenceCaptureResult`. New code SHOULD continue moving important domain distinctions into explicit representations.

---

## 6. Language-Lawyer Correctness

Engineers are responsible for the actual semantic contract of the language they use.

First-party production code **MUST NOT** intentionally depend on undefined behavior.

Where the language distinguishes them, review explicitly for:

- defined behavior;
- implementation-defined behavior;
- unspecified behavior;
- undefined behavior;
- erroneous behavior where applicable;
- traps, exceptions, or panics;
- unsafe operations;
- data races and memory-model violations.

### 6.1 Undefined behavior

Undefined behavior in first-party production code is a **BLOCKER**.

Depending on the language, examples include signed overflow, invalid shifts, out-of-bounds access, invalid pointer arithmetic, use after lifetime, invalid alignment, incompatible aliasing, uninitialized reads, double free/destruction, invalid object representation, or data races.

“This works with MSVC today” is not a defense.

### 6.2 Implementation-defined behavior

Implementation-defined behavior **MAY** be used only when all of the following are true:

- the supported implementation is explicitly constrained;
- the implementation documents the behavior;
- the dependency is intentional;
- the assumption is documented where non-obvious;
- assertions/tests protect it where practical;
- portability consequences are understood.

### 6.3 Unspecified behavior

Correctness, security, evidence semantics, reproducibility, and ABI compatibility **MUST NOT** depend on which permitted unspecified behavior is chosen.

---

## 7. Data Model, ABI, and Serialization

Every subsystem **MUST** understand the data model it relies upon.

Do not silently assume integer width, pointer width, byte order, character representation, alignment, packing, enum representation, floating-point semantics, object layout, calling convention, or filesystem semantics unless the governing contract guarantees it.

Required assumptions SHOULD be enforced with static assertions, explicit serialization, architecture guards, checked conversion, runtime validation, or compatibility tests.

### 7.1 Shared driver ABI

`shared/phylaram.h` crosses the user/kernel privilege boundary and is therefore a protocol definition, not an ordinary convenience header.

ABI structures **MUST** have deliberate widths and stable size assertions. Adding, removing, repurposing, or reinterpreting fields requires a protocol-version decision and compatibility review.

Native in-memory layout **MUST NOT** be treated as a stable persisted format unless the ABI explicitly defines it.

IOCTL buffers **MUST** be validated for size, version, count, range, and cross-field consistency before dependent use.

### 7.2 Provenance JSON

The provenance map is an explicit serialized format. Fields have defined widths, units, status semantics, and cross-field relationships. The verifier **MUST** treat JSON as hostile input and independently prove geometry and accounting rather than trusting producer totals.

Existing fields **MUST NOT** silently acquire new meanings.

---

## 8. Make Invalid States Difficult to Express

For a critical property, prefer enforcement in this order:

1. impossible by representation;
2. rejected by type system;
3. rejected at compile time;
4. rejected by static analysis;
5. rejected by automated testing;
6. rejected by runtime validation;
7. prohibited by documentation;
8. dependent on programmer memory.

Move critical properties upward whenever practical.

### 8.1 Forgetfulness Test

For every important invariant ask:

> What happens when the next competent programmer forgets this rule?

If forgetting can cause memory corruption, evidence mutation, false completeness, fabricated data, privilege escalation, use-after-free, arithmetic overflow, ABI incompatibility, race conditions, data loss, or incorrect forensic interpretation, a comment alone is insufficient when stronger enforcement is practical.

### 8.2 Exhaustiveness

Important state machines and enums **SHOULD** be exhaustively handled. Catch-all branches deserve scrutiny.

The desired behavior is:

```text
new state
  -> affected decisions become visible
  -> compiler/static analysis/test fails
  -> engineer makes an explicit semantic decision
```

---

## 9. Input Validation

Validate all data received from an untrusted source before dependent use.

For PhylaRAM this includes command-line values, operator-selected paths, environment-derived paths, IOCTL input, map JSON, SHA sidecars, and RAW files supplied to the verifier.

Validate as applicable:

- length and capacity;
- offsets and intervals;
- counts;
- versions;
- types and enum domains;
- alignment;
- encodings;
- allocation sizes;
- arithmetic preconditions;
- cross-field relationships;
- file and path semantics.

Do not dereference, index, allocate from, narrow, shift, multiply, or copy based on hostile input and only afterward decide whether the value was valid.

Assertions are not hostile-input validation.

---

## 10. Integer Safety

All integer operations involving external, security-relevant, memory-relevant, evidence-relevant, or resource-allocation values **MUST** be range-safe.

This includes addition, subtraction, multiplication, division, remainder, negation, shifts, narrowing, signed/unsigned conversion, indexing, allocation sizes, file offsets, physical addresses, byte counts, and timeout calculations.

The preferred sequence is:

```text
establish mathematical range
-> verify operands
-> perform operation
```

not:

```text
perform dangerous operation
-> inspect whether it appears to have overflowed
```

Use checked arithmetic, range-aware types, widening arithmetic, explicit preconditions, and checked conversions as appropriate.

Saturating arithmetic is permitted only when saturation is the intended domain meaning.

Every narrowing conversion requires a proof that the source is representable. Units and signedness must be obvious.

Physical-address and file-offset math receives heightened review.

---

## 11. Initialization, Bounds, and Memory Safety

Objects **MUST** have defined state before observation. Uninitialized reads are prohibited. Security-sensitive output buffers **MUST NOT** expose uninitialized bytes.

Whenever an operation has data plus a length, count, capacity, or index, their relationship is part of the API contract.

Requested bytes and actual bytes read are different concepts and **MUST** remain distinguishable.

For memory-unsafe languages, out-of-bounds access, use-after-free, double-free, invalid pointer use, invalid alignment, unchecked pointer arithmetic, and stale pointers are **BLOCKER** defects.

Memory-safe languages still require review of unsafe escape hatches, FFI, concurrency, integer-derived indexes, resource lifetime, and unbounded allocation.

Memory safety is a system property, not a language label.

---

## 12. Object Lifetime and Resource Ownership

Every reference, pointer, view, iterator, handle, callback context, or captured object **MUST** remain inside the valid lifetime of the referenced state.

Prohibited outcomes include use after free, dangling views, stale iterators, stale handles, callbacks retaining destroyed state, and concurrency accessing destroyed state.

Every resource must have clear ownership. Reviewers must be able to answer:

1. who creates it;
2. who owns it;
3. whether ownership transfers;
4. how transfer is represented;
5. who releases it;
6. how partial failure unwinds;
7. whether cleanup is idempotent when required.

Resources include heap memory, Win32 handles, service handles, WDF objects, temporary driver files, files, cryptographic providers, threads, locks, mappings, and staged evidence artifacts.

Prefer RAII, deterministic scope cleanup, ownership types, `Drop`, structured cleanup, or `finally` over manual lifetime bookkeeping.

Acquisition and release SHOULD live at the same conceptual level.

### 12.1 Lifecycle symmetry

Open/close, start/stop, begin/end, allocate/release, create/delete, and map/unmap code SHOULD be structurally recognizable. Asymmetric lifecycle code receives additional scrutiny.

---

## 13. Expressions and Side Effects

Keep side effects obvious.

Correctness **MUST NOT** depend on subtle or unspecified evaluation order.

Avoid surprising multiple mutations in one expression, assignments buried in unrelated expressions, hidden mutation in conditions, or overloaded operations with unexpected side effects.

“Legal” is not sufficient when a simpler form is materially easier to reason about.

---

## 14. Strings, Text, and Encodings

Strings are structured data. Do not assume null termination, ASCII, valid Unicode, UTF-8, byte length equals character count, or character count equals display width unless guaranteed.

Encoding boundaries **MUST** be explicit where paths, protocol data, identity, security, or forensic meaning is involved.

Lossy conversion in those paths requires explicit justification.

---

## 15. Errors Are Part of the API

Error behavior must be designed.

A useful error preserves enough information to determine:

1. what failed;
2. where;
3. why;
4. whether anything succeeded;
5. whether state changed;
6. whether retry makes sense.

Errors affecting correctness, evidence interpretation, integrity, completeness, persistence, security, or auditability **MUST NOT** be silently discarded.

Do not collapse semantically different failures into a boolean if downstream logic needs the distinction.

### 15.1 Truthful terminal states

A partially successful operation **MUST NOT** be reported as complete success.

Preserve distinctions among success, incomplete/partial, cancellation, timeout, resource exhaustion, unsupported conditions, corruption, validation failure, and internal failure where applicable.

Stopping is not completing.

### 15.2 Assertions, panics, and malformed input

Assertions prove programmer assumptions, not hostile input.

Malformed external evidence is ordinary verifier input. It SHOULD produce an explicit error, not an uncontrolled panic, assertion failure, or kernel crash.

---

## 16. Security Architecture

### 16.1 Default deny

Permission is explicitly granted. Unknown or unclassified requests are not implicitly authorized.

The kernel device remains restricted to SYSTEM and Administrators. Unknown IOCTLs fail.

### 16.2 Least privilege

Privilege exists for the smallest practical component, operation, and duration.

The elevated acquisition executable **MUST NOT** gain unrelated privileged features.

The kernel driver **MUST NOT** expose arbitrary physical-address, arbitrary virtual-memory, or arbitrary read/write primitives. The accepted physical read contract is a validated run index, offset within the frozen run, and bounded requested length.

### 16.3 Defense in depth

Independent controls are appropriate at critical boundaries. User mode validates driver responses even when the kernel produced them. The verifier independently proves evidence relationships even when the producer wrote the map.

### 16.4 Kernel-code provenance

The production application’s implicit kernel-code source is the driver embedded at build time. A nearby or user-selected `phylaram.sys` **MUST NOT** silently replace it.

Any future developer override requires an explicit non-production mode, cryptographic identity validation, and clear user-visible semantics.

### 16.5 Output to other interpreters

Input validation and output encoding solve different problems. Data sent to shells, JSON, filesystem APIs, logs, regular expressions, subprocesses, or other interpreters **MUST** use the correct structured API or encoding for the destination.

---

## 17. Concurrency and Synchronization

Concurrency requires justification.

Concurrent code **MUST** address ownership, protected state, synchronization, atomicity, memory ordering where relevant, cancellation, shutdown, lifetime, lock ordering, and error propagation.

Data races are **BLOCKER** defects even when rare.

Shared mutable state SHOULD be minimized.

For the KMDF control device, the file context owns the frozen physical-range snapshot. File cleanup and IOCTL access to that snapshot **MUST** be synchronized so cleanup cannot free it while a request is using it.

For the GUI worker, window lifetime, posted-message ownership, cancellation, and thread join semantics are part of correctness.

---

## 18. Forensic Prime Directives

These rules override convenience.

### 18.1 Original evidence is immutable

Normal operation **MUST NOT** modify source evidence. PhylaRAM acquires volatile memory into new output; it does not create incidental artifacts beside an existing evidence source presented to the verifier.

Final destination collisions are denied rather than overwritten.

### 18.2 Unknown is not zero

**FORENSIC: `UNREADABLE != ZERO`.**

The flat RAW representation may logically read zeros in an unreadable interval because RAW has no native unknown-byte symbol. That representation fact **MUST NOT** be interpreted as an observation that RAM contained zero.

Every unreadable physical interval **MUST** remain explicitly represented in the provenance map with location, length, and observed NTSTATUS.

A delivery mechanism that loses that provenance is not a valid finalized PhylaRAM evidence bundle.

### 18.3 Completeness is proved

A capture is complete only after acquisition reaches the end, session-end topology comparison succeeds, SHA-256 finalizes, the RAW file flushes, sidecars flush, accounting invariants hold, and the bundle is published successfully.

Unreadable physical memory or topology change yields **incomplete**, not complete.

Cancellation and internal failure do not publish a successful final bundle.

### 18.4 Facts are not interpretations

Provenance contains observations and acquisition facts. Heuristics, classifications, threat mappings, compliance claims, analytic hypotheses, or sampled entropy metrics **MUST NOT** be inserted into the canonical provenance map as if they were acquisition facts.

Derived analysis belongs in an explicitly derived artifact with its own provenance and semantics.

### 18.5 Hashing is mandatory

Every finalized PhylaRAM evidence bundle **MUST** contain a SHA-256 digest in the map and companion sidecar. Hash-free finalized evidence is not a supported product state.

The hash proves byte-level consistency against the recorded digest. It does not by itself prove identity of the original host, legal admissibility, or chain of custody.

---

## 19. Filesystem Write Surface

All first-party writes **MUST** be discoverable and classified.

PhylaRAM uses these write classes:

- **EvidenceStaging:** `.partial` RAW/map/hash files written before publication.
- **EvidenceBundle:** finalized RAW, map, and hash files produced only by successful finalization.
- **Temp:** protected extracted driver files required for the kernel-service lifecycle.
- **BuildArtifact:** compiler, test, packaging, signing, and CI outputs outside the evidence runtime path.
- **Log:** diagnostic logs where explicitly produced by scripts or validation tooling.

“Evidence” is not a generic permission to write anywhere.

Creation, write, append, truncate, rename, copy, delete, directory creation, and temporary promotion are all writes for review purposes.

Unclassified production write sites are **BLOCKER** defects.

The acquisition runtime **MUST NOT** overwrite existing final or staging evidence paths.

---

## 20. Small Complete Cores and Explicit Composition

Before adding machinery, identify the fundamental operation.

For PhylaRAM acquisition the core is approximately:

```text
validate frozen run
-> request bounded physical bytes
-> classify actual transfer
-> write acquired bytes at physical offset
-> preserve unreadable intervals
-> update logical hash
-> compare terminal topology
```

Evidence publication is a separate transaction:

```text
preflight destinations
-> acquire into staging
-> finalize hash
-> flush RAW
-> prove accounting
-> write and flush sidecars
-> promote finalized bundle
```

Presentation layers may display progress and terminal results, but **MUST NOT** implement competing acquisition or finalization semantics.

Prefer visibly composed functions over factories, registries, callback forests, deep inheritance, or generic dispatch systems unless the domain actually requires them.

### 20.1 Keep the system small

Every new abstraction, dependency, thread, process, callback, cache, state machine, or plugin mechanism adds assurance cost. Add it only when it removes more accidental complexity than it creates.

### 20.2 Do not apply DRY mechanically

Duplicate syntax may be preferable to false semantic coupling. Create abstractions when they represent a real concept, centralize a true invariant, remove knowledge from callers, or reduce the number of concepts exposed.

### 20.3 The deletion question

During design and review ask:

> What code could disappear if the model were better?

Look especially for duplicate validation, duplicate state, boolean combinations, synchronization glue, repeated cleanup, repeated error translation, and comments explaining accidental complexity.

---

## 21. Naming and Domain Meaning

Names are part of correctness.

Prefer names such as `physicalAddress`, `offsetWithinRun`, `requestedBytes`, `acquiredBytes`, `unreadableBytes`, `topologyChanged`, and `EvidenceCaptureStatus`.

Avoid meaningless names such as `tmp`, `data2`, `thing`, `flag`, `status2`, or generic verbs such as `handle`, `process`, and `manage` when a precise domain verb exists.

Boolean parameters are suspicious when they erase meaning. Prefer semantic enums or configuration objects when a call site would otherwise require signature lookup.

Unnamed domain constants are prohibited. Units SHOULD be encoded in names such as `timeoutMilliseconds`, `physicalBytes`, and `rateLimitMBps`/`MiBps` as appropriate. New names SHOULD distinguish SI MB from binary MiB accurately.

---

## 22. Comments and Proof Preservation

Code communicates ordinary mechanics. Comments preserve engineering knowledge that would otherwise be lost.

A valuable comment may explain why, what, how, or under which external constraint when omitting it would force rediscovery.

Use these labels where they represent real contracts:

- `INVARIANT:` state that must remain true;
- `SAFETY:` proof obligation around unsafe/native operations;
- `SECURITY:` trust-boundary or attack assumption;
- `FORENSIC:` rule preserving evidentiary meaning;
- `COMPAT:` externally imposed compatibility rule;
- `PERFORMANCE:` non-obvious optimization supported by measurement.

Do not use labels decoratively.

### 22.1 Safety comments

Material unsafe/native operations SHOULD make relevant proof obligations visible: validity, provenance, lifetime, bounds, initialization, alignment, aliasing, threading, mutability, retention, and ownership transfer.

“Safe because this is correct” is not a proof.

### 22.2 Comment density

Comment density follows semantic density. Straightforward code may need no internal commentary. Kernel, ABI, physical-address, concurrency, crypto, and forensic-semantic code may need extensive explanation.

Optimize for minimum rediscovery, not minimum comments.

### 22.3 Comments must be true

A materially false comment is a defect. When implementation invalidates reasoning, update the comment in the same change.

Do not preserve commented-out implementations or obsolete history in production source. Version control is the archive.

---

## 23. Public Contracts

Meaningful APIs **MUST** document applicable input validity, output meaning, ownership, lifetime, mutation, units, bounds, blocking behavior, partial results, failures, security requirements, forensic semantics, and unsafe obligations.

A caller should not need to reverse-engineer an implementation to discover whether an API may publish partial evidence or retain a pointer.

---

## 24. Compiler Diagnostics and Static Analysis

Heed compiler warnings.

First-party C/C++ builds use strong warning levels and warnings as errors. New code **MUST NOT** weaken those settings globally.

Warning suppressions must be narrow and justified. A warning is an unresolved engineering question until understood.

Applicable static analysis includes CodeQL, MSVC analysis/SDL checks, WDK analysis, and Static Driver Verifier where available.

Findings must be reviewed. Suppression is not resolution unless the analyzer model demonstrably does not apply and the reason is recorded.

---

## 25. Dynamic Analysis

Use overlapping dynamic techniques where they add assurance.

For the KMDF driver this includes Driver Verifier with Special Pool, force IRQL checking, pool tracking, I/O verification, deadlock detection, security checks, and appropriate stress profiles.

For parsers and verifier logic, malformed-input tests, property testing, and fuzzing are appropriate.

Passing one analyzer does not prove correctness.

The project **MUST NOT** claim a live validation gate passed until the gate actually ran on the stated environment and evidence of the result exists.

---

## 26. Testing as a Proof System

Tests are production code.

Use appropriate combinations of unit, regression, integration, property, fuzz, malformed-input, platform, end-to-end, differential, and model-based tests.

A bug fix **SHOULD** include a regression test that would fail on the original defect.

Security boundaries require hostile-input coverage. Parser changes require malformed-input coverage. Forensic-semantic changes require invariant tests.

### 26.1 Crash-free is not correct

A memory-safe, crash-free program can still be forensically wrong.

Fuzzing proves resilience properties; it does not automatically prove semantic correctness, completeness, provenance, or interpretation.

Maintain explicit semantic-invariant tests separately.

### 26.2 PhylaRAM-specific proof obligations

Tests SHOULD cover at minimum:

- partial `MmCopyMemory` transfer preservation;
- 4 KiB unreadable isolation and continuation;
- real zero data versus unreadable data;
- run-boundary arithmetic;
- topology change at session end;
- cancellation without final publication;
- finalization failure without false success;
- destination collision denial;
- low-bandwidth rate limits whose required wait exceeds five seconds;
- malformed map geometry, overlapping ranges, unreadable spans outside ranges, and arithmetic overflow;
- non-zero RAW bytes falsely claimed as unreadable;
- mandatory SHA-256 semantics;
- user/kernel ABI size/version consistency;
- cleanup under failure and repeated acquisition cycles.

---

## 27. Performance and Cleverness Budget

No performance folklore.

Use:

```text
benchmark
-> profile
-> identify bottleneck
-> change
-> benchmark again
```

A non-obvious optimization requires a material reason and measurement. “No measurable improvement” is a valid result.

Every unusual mechanism consumes a **cleverness budget**: bit tricks, macros, metaprogramming, unsafe code, lock-free algorithms, packed representation, obscure language features, compiler-specific behavior, and lifetime tricks all have a higher burden of proof.

Where relevant, the author must provide:

1. invariant;
2. derivation or source;
3. reason ordinary code is insufficient;
4. boundary tests;
5. analyzer coverage;
6. performance measurement if performance is the justification.

Magic requires proof, not faith. Preserve references to language standards, WDK/API documentation, ABI specifications, RFCs, filesystem specifications, or algorithm papers when they materially support the local invariant.

Explain the local invariant as well as citing authority.

---

## 28. Dependencies, Generated Code, and Dead Code

Every dependency adds maintenance and supply-chain risk. Evaluate necessity, maintenance health, security history, transitive dependencies, license, binary impact, and update burden.

Do not add a dependency to avoid a few obvious lines. Do not reimplement complex security primitives merely to avoid a mature, vetted platform implementation.

Generated code **MUST** be identified and regenerated from its source definition rather than manually edited.

Delete dead code. Do not keep obsolete implementations in comments or maintain compatibility paths without an active requirement.

---

## 29. Language-Specific Realization

Universal invariants govern first. Language mechanisms implement them.

### 29.1 C17 and KMDF

C code in `driver/` is subject to the strongest applicable CERT C principles and WDK contracts.

**MUST:**

- avoid undefined behavior;
- validate every user-mode IOCTL input before use;
- prove size/offset/count arithmetic before performing it;
- preserve exact partial-transfer byte counts and NTSTATUS;
- initialize output visible to user mode;
- obey documented WDF ownership and `WDFDEVICE_INIT` lifetime rules;
- obey IRQL requirements and annotate pageable code correctly;
- synchronize file-context lifetime with I/O access;
- use SAL where it improves caller/analyzer understanding;
- maintain the run-index/offset protocol rather than accepting arbitrary physical addresses;
- retain `/W4` and `/WX` and applicable Spectre/CFG hardening.

**BLOCKER examples:** use-after-free, invalid MDL assumptions, unsynchronized cleanup, unchecked range arithmetic, arbitrary kernel/physical-memory access, or returning uninitialized kernel data.

Pointer validity, alignment, aliasing, object lifetime, initialization, signed arithmetic, and integer conversion receive explicit review.

### 29.2 C++20 / Win32

Prefer C++ facilities that make invalid usage ill-formed or scope-bound over C mechanisms that defer failure to convention.

**Prefer:**

- RAII for handles, services, cryptographic objects, threads, and staged resources;
- value semantics for result objects;
- scoped ownership;
- standard containers rather than raw allocation;
- semantic enums for terminal states;
- direct composition rather than framework layers.

**Review explicitly:** object lifetime, dangling references, callback/thread lifetime, invalid casts, narrowing, signed arithmetic, iterator invalidation, exception behavior, concurrency, and partial-failure cleanup.

The evidence transaction is a core semantic boundary. CLI and GUI **MUST** call it rather than reimplementing finalization independently.

Win32 APIs with legacy signatures that require a cast SHOULD have a local safety/compatibility explanation when the cast obscures a proof obligation.

### 29.3 Rust 2021 verifier

Prefer safe Rust. This crate currently has no need for first-party unsafe code; `unsafe` therefore requires architecture review before introduction and SHOULD be forbidden at crate level once CI enforcement is added.

Production verifier paths SHOULD NOT casually use `unwrap`, `expect`, `panic!`, `todo!`, or `unimplemented!` for externally reachable states.

Prefer checked arithmetic, `TryFrom` for narrowing, semantic enums, and exhaustive matching.

The verifier treats RAW, JSON, and sidecars as hostile. Malformed evidence must return structured errors, not panic.

Any future `unsafe` block requires a `SAFETY:` proof addressing validity, lifetime, bounds, alignment, aliasing, threading, and FFI obligations as applicable, plus a safe wrapper where practical.

### 29.4 Python 3 tooling

Python scripts MUST validate external paths/arguments, avoid shell construction when structured subprocess arguments suffice, propagate failure status, and use explicit integer/range checks when modelling fixed-width native behavior.

Tests that model Windows semantics MUST state where the model differs from real Windows execution.

### 29.5 PowerShell

PowerShell scripts used for validation, signing, or packaging MUST fail visibly on command failure, validate paths and artifacts before destructive operations, avoid string-built command interpretation when direct invocation suffices, and never report a validation gate as passed merely because a command was attempted.

Security-control weakening for test signing belongs only in explicitly designated test environments.

### 29.6 Windows batch

Batch files are retained only for narrow Windows setup/build tasks. Because error propagation and quoting are fragile, batch changes require explicit `%ERRORLEVEL%`/command failure handling and careful quoting of paths.

Complex logic SHOULD move to a language with stronger error and data semantics rather than expanding batch control flow.

---

## 30. Universal Compliance Matrix

| Universal guarantee | C17 / KMDF | C++20 | Rust 2021 | Python / PowerShell / batch |
| --- | --- | --- | --- | --- |
| Bounds safety | explicit preconditions, SAL, WDF buffer APIs | containers, checked offsets, bounded Win32 calls | slices plus checked indexing | explicit length/range validation |
| Integer range | precondition before arithmetic | checked preconditions / safe conversion | `checked_*`, `TryFrom` | explicit fixed-width/domain validation |
| Lifetime | WDF ownership + synchronization discipline | RAII and scoped ownership | borrow checker + `Drop` | structured scope / `finally` / explicit cleanup |
| Exhaustive states | enum + warnings/analyzers/tests | `enum class` + exhaustive switch/review | exhaustive enum match | explicit tagged states and tests |
| Resource cleanup | paired WDF/kernel ownership | RAII | `Drop` / RAII | `try/finally`, `using`-style constructs, explicit error paths |
| Hostile input | validate IOCTL buffers first | parse/validate before dependent use | deserialize then independently validate | validate before command/filesystem use |
| ABI | fixed-width fields + static assertions | same shared contract | explicit parser schema | do not infer native layout |
| Forensic outcome | explicit status/NTSTATUS | `EvidenceCaptureStatus` | validated map status | scripts must not invent pass status |

The invariant is universal; the enforcement mechanism depends on the language.

---

## 31. Code Review Ownership

A reviewer owns every line they approve. Do not approve code you do not understand.

Review as applicable:

1. architecture;
2. language semantics;
3. correctness;
4. undefined behavior;
5. integer behavior;
6. lifetime;
7. ownership;
8. bounds;
9. initialization;
10. concurrency;
11. errors and terminal states;
12. forensic meaning;
13. security boundaries;
14. input validation;
15. privilege;
16. write surfaces;
17. naming;
18. comments;
19. public contracts;
20. tests;
21. static-analysis findings;
22. dynamic validation;
23. performance claims;
24. dependencies;
25. documentation truthfulness.

### 31.1 Severity

**BLOCKER** examples include undefined behavior, correctness defects, vulnerabilities, out-of-bounds access, invalid lifetime, use-after-free, data races, unchecked hostile input, behavior-affecting integer overflow, evidence-integrity violations, false completeness, fabricated forensic content, unclassified production writes, or privilege-boundary failures.

**REQUIRED** examples include materially misleading naming, inadequate API contracts, unnecessary complexity, ownership ambiguity, materially inadequate tests, missing critical reasoning, or architectural degradation.

**NIT** is optional polish. Personal aesthetics must not masquerade as correctness.

---

## 32. Formal Exceptions

A `MUST` may be waived only explicitly.

Record:

- exact rule;
- engineering reason;
- risk;
- scope;
- approving maintainer;
- mitigation;
- removal condition if temporary.

“Legacy code does it” is not justification. “Compiler X accepts it” is not sufficient justification for behavior outside the governing language contract.

For forensic-integrity or kernel-memory-safety rules, waivers require exceptional scrutiny and MUST NOT merely convert a known correctness defect into documented behavior.

---

## 33. CI Enforcement Plan

The long-term objective is:

> Every rule that can reasonably be automated should stop depending on reviewer memory.

### 33.1 Automatically enforceable now

The repository can and does automate significant portions of:

- C/C++ warning-as-error builds;
- C++20/C17 language modes;
- Rust formatting;
- Rust Clippy warnings as errors;
- Rust unit/property tests;
- portable C++ tests;
- Python fixture/topology tests;
- CodeQL analysis;
- x64 Windows build;
- ABI compile-time size assertions;
- release artifact construction.

### 33.2 Automatically enforceable next

CI SHOULD progressively add:

- a crate-level `#![forbid(unsafe_code)]` while verifier unsafe code remains unnecessary;
- dedicated regression tests for evidence-transaction publication failures;
- hostile JSON corpus/fuzzing for the verifier;
- policy checks that contributor instructions reference this standard;
- inventory/checks for production filesystem write sites;
- dependency auditing and lockfile policy;
- Windows tests exercising strict command-line parsing and rate-limit pacing;
- explicit static driver analysis where a compatible WDK environment is available;
- artifact-level checks proving that the embedded driver is the intended signed binary.

### 33.3 Review-enforceable

Human/AI review must still judge abstraction quality, semantic clarity, comment truthfulness, domain modelling, cleverness justification, and whether facts remain separated from interpretation.

### 33.4 Architecture-enforceable

Architecture must continue enforcing privilege separation, run-index-bounded physical reads, immutable evidence destinations, one canonical publication transaction, protected driver extraction, synchronized kernel session lifetime, and independent verification.

### 33.5 Gates that require real environments

Driver Verifier stress, Secure Boot/HVCI production-signing behavior, large-RAM/ReBAR/NUMA topology behavior, and real Volatility/MemProcFS interoperability are environment-dependent validation gates. They MUST remain visibly open until actually executed with preserved evidence.

---

## 34. Final Merge Gate

Before merge, answer every applicable question.

### Semantics

- [ ] Is all relied-upon behavior defined by the language/platform contract?
- [ ] Does no correctness property depend on undefined behavior?
- [ ] Are implementation-defined assumptions explicit and constrained?
- [ ] Is unspecified behavior semantically irrelevant?

### Data and arithmetic

- [ ] Are hostile inputs validated before dependent use?
- [ ] Are additions, subtractions, multiplications, shifts, and conversions proven in range?
- [ ] Are narrowing conversions representable by proof?
- [ ] Are units, lengths, counts, and offsets unambiguous?
- [ ] Are reads initialized?
- [ ] Are data/bounds relationships explicit?

### Lifetime and ownership

- [ ] Is every resource owner obvious?
- [ ] Are pointer/reference/handle lifetimes valid?
- [ ] Can cleanup occur exactly once where required?
- [ ] Do all partial-failure paths unwind correctly?
- [ ] Can no stale handle, callback, view, or reference escape its lifetime?

### Architecture

- [ ] Does code represent the domain rather than incidental machinery?
- [ ] Does it pass the One-Pass Test where reasonable?
- [ ] Can it pass the Read-Aloud Test?
- [ ] Could stronger types remove bookkeeping?
- [ ] Are important states exhaustive?
- [ ] Could code disappear if the model were better?

### Security

- [ ] Are trust boundaries explicit?
- [ ] Is default deny preserved?
- [ ] Is least privilege preserved?
- [ ] Are outputs encoded correctly for downstream APIs/interpreters?
- [ ] Is defense in depth appropriate?
- [ ] Can kernel code be substituted implicitly?

### Forensics

- [ ] Is original evidence immutable?
- [ ] Is every production write classified?
- [ ] Is unknown/unreadable distinct from factual zero?
- [ ] Is incomplete distinct from complete?
- [ ] Are cancellation and failure truthful?
- [ ] Are facts separated from interpretation?
- [ ] Is a finalized capture always hashed?
- [ ] Does UI presentation derive from core truth instead of recreating it?

### Clarity

- [ ] Are names semantic?
- [ ] Are comments true?
- [ ] Does commentary match semantic density?
- [ ] Are unsafe/non-obvious operations accompanied by a proof where needed?
- [ ] Is cleverness justified?
- [ ] Could another qualified engineer understand the change without asking the author?

### Verification

- [ ] Are compiler warnings clean?
- [ ] Has applicable static analysis passed?
- [ ] Has applicable dynamic analysis passed, or is the unrun gate explicitly recorded?
- [ ] Does each fixed defect have regression coverage where practical?
- [ ] Do tests prove important semantic invariants?
- [ ] Is hostile/malformed input tested at trust boundaries?
- [ ] Are performance claims measured?
- [ ] Are documentation claims no stronger than the evidence?

### Forgetfulness

- [ ] If a future competent programmer forgets an important rule, does the design prevent or expose the mistake?

If the answer to an applicable critical question is **NO**, the change is not finished.

---

## 35. Closing Principle

Beautiful code is not merely pleasant to read. It is code whose correctness can be reasoned about.

It uses the programming language according to its actual semantic contract rather than according to folklore.

It validates hostile data before use.

It proves arithmetic is valid before performing dangerous operations.

It makes ownership and lifetime visible.

It constrains state so invalid combinations are difficult to represent.

It uses compilers, type systems, static analysis, dynamic analysis, tests, architecture, and review together rather than trusting any one mechanism.

It preserves difficult reasoning in precise comments.

It favors simple, explicit implementations over clever ones.

It treats warnings as questions requiring answers.

It treats undefined behavior as a defect, not an optimization technique.

It makes dangerous operations visually and architecturally exceptional.

It makes the safe operation the natural operation.

And in forensic software, it preserves reality even when reality is incomplete, corrupt, unreadable, or inconvenient.

The highest objective is:

> **A qualified engineer should be able to read the source, understand the model, identify the invariants, verify the dangerous operations, and explain why the result can be trusted.**
