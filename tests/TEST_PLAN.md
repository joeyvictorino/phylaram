# PhylaRAM Acceptance Test Plan and Validation Gates

**Target:** Windows 10 version 2004+ / Windows 11, x64  
**Normative engineering rules:** [`../ENGINEERING_STANDARD.md`](../ENGINEERING_STANDARD.md)

A test plan is not evidence that a test ran. Each release gate closes only after execution evidence is recorded for the release candidate.

---

## 1. Portable Automated Validation

CI runs the following portable checks where configured.

### Range algebra

Proves model-level behavior for:

- checked 64-bit address arithmetic;
- zero-length and overlapping ranges;
- large physical-address/topology cases.

### Mock acquisition

Proves model-level behavior for:

- bounded bulk reads;
- partial transfer preservation;
- page-granular unreadable isolation;
- continuation after unreadable pages;
- zero-valued acquired data remaining distinct from unreadable data;
- topology-change terminal state;
- cancellation behavior.

### Provenance-map contract

Proves model-level serialization expectations for:

- `phylaram-map-2`;
- complete vs incomplete terminal status;
- physical addresses and NTSTATUS formatting;
- optional kernel hints;
- exclusion of analytic/compliance interpretation from canonical provenance.

### CLI contract

Proves parser-model behavior for:

- ordinary capture;
- strict non-negative `--rate-limit` parsing;
- low limits such as 1 MiB/s remaining valid;
- overflow and malformed rate rejection;
- `--dry-run` and `--json` mode restrictions;
- unknown option rejection;
- rejection of raw stdout (`-`);
- rejection of legacy `--no-hash` and `--throttle` aliases;
- one-output-path rule;
- GUI/help mode exclusivity.

### Rust offline verifier

CI requires:

```bash
cargo fmt --check
cargo clippy -- -D warnings
cargo test --verbose
```

Verifier tests must cover malformed and adversarial relationships including:

- range overlap and arithmetic overflow;
- invalid driver-run domains;
- unreadable span overlap;
- unreadable spans outside physical RAM;
- unreadable sum/accounting mismatch;
- non-zero RAW bytes falsely described as unreadable;
- invalid terminal state;
- invalid hash encoding and hash mismatch.

---

## 2. Windows Build Gate

**Environment:** configured Visual Studio 2022 / MSVC v143 and WDK toolchain.

### Driver

- C17;
- x64;
- `/W4` and `/WX`;
- Control Flow Guard and configured Spectre mitigations;
- zero first-party warnings.

### User mode

- C++20;
- x64;
- `/W4` and `/WX`;
- conformance mode;
- SDL checks;
- CFG, DEP, ASLR, CET where configured;
- zero first-party warnings.

### Rust

- release build of `phylaram-verify`.

The gate passes only when all required binaries are produced from the exact release commit and every required build command succeeds.

---

## 3. Evidence-Transaction Regression Gate

Windows integration tests SHOULD exercise the real user-mode transaction against a controlled/mocked device seam or dedicated test VM and prove:

1. writer preflight occurs before acquisition writes;
2. SHA-256 is mandatory;
3. `END_SESSION` occurs exactly once;
4. topology change is not overwritten by presentation code;
5. RAW flush failure produces failure, not a success UI/message;
6. map write failure produces failure;
7. SHA sidecar write failure produces failure;
8. map/hash promotion failure produces failure and cleanup;
9. RAW promotion failure produces failure and removes already-promoted sidecars when possible;
10. destination collision refuses overwrite;
11. cancellation publishes no successful final bundle;
12. GUI and CLI consume the same `CaptureEvidenceToFile` result semantics.

This is a required regression area for the defects that motivated the remediation branch.

---

## 4. Rate-Limit Regression Gate

Rate limiting must be tested with transfer sizes for which the required pacing delay exceeds five seconds.

At minimum:

- 16 MiB transferred under a 1 MiB/s limit must require approximately 16 seconds of target elapsed time, subject to scheduler tolerance;
- long waits must be split into cancellation-responsive sleeps rather than discarded;
- cancellation during a pacing wait must terminate without final publication;
- malformed/negative/overflowing CLI limits must be rejected.

A parser test alone does not prove the pacing implementation.

---

## 5. Static Driver Analysis Gate

Use the strongest practical WDK/static checks for the release toolchain, including Static Driver Verifier where supported and CodeQL/other analysis where applicable.

Review specifically for:

- control-device initialization ownership;
- IOCTL buffer/MDL validation;
- integer arithmetic;
- pageable-code/IRQL discipline;
- file-context lifetime;
- synchronization scope;
- pool ownership and release;
- output initialization.

No finding is considered resolved merely because it was suppressed.

---

## 6. Driver Verifier Dynamic Gate

**Environment:** dedicated Windows test VM or hardware, never the only copy of evidence.

Enable applicable Driver Verifier checks including:

- Special Pool;
- Force IRQL Checking;
- Pool Tracking;
- I/O Verification;
- Deadlock Detection;
- Security Checks;
- Miscellaneous Checks.

Execute at least 100 repeated acquisition cycles under memory/I/O stress.

Pass criteria:

- zero bugchecks;
- zero use-after-free indicators;
- zero IRQL violations;
- zero pool leaks/corruption attributable to `phylaram.sys`;
- clean service/temporary-driver lifecycle after every cycle.

This gate specifically validates the file-cleanup/IOCTL synchronization fix; code inspection alone does not close it.

---

## 7. Physical Memory and Filesystem Matrix

Validate representative topologies including:

- 4 GB;
- 16 GB;
- 64 GB with large PCIe MMIO/ReBAR holes;
- 128 GB+ and NUMA/multi-node where available.

Validate destination behavior on supported filesystems, especially NTFS and ReFS sparse files.

For every case prove:

- `file offset == physical address` for populated RAM;
- logical size equals highest physical range end;
- reported physical sum matches range geometry;
- sparse holes correspond to unpopulated address space rather than silently omitted RAM;
- final SHA matches independent hashing;
- verifier accepts the finalized bundle;
- incomplete conditions remain explicit.

FAT/FAT32 oversized-file rejection may be tested as an unsupported destination case; it is not a recommended evidence destination.

---

## 8. Production Windows Security / Signing Gate

Validate the final production-signed driver on supported Windows systems with:

- Secure Boot enabled;
- VBS enabled where applicable;
- HVCI / Memory Integrity enabled;
- normal Windows Defender security controls active.

The project must select and document the exact Microsoft production signing/certification path. Attestation signing and WHCP/HLK certification are distinct paths and must not be conflated.

Pass criteria require loading and acquiring without disabling production security controls.

Test-signed alpha artifacts do not satisfy this gate.

---

## 9. Forensic Interoperability Gate

Acquire real test images from supported Windows builds and validate with current supported versions of downstream tools.

At minimum exercise representative Volatility 3 operations such as system information and process enumeration, and initialize/mount with MemProcFS where supported.

Pass criteria include:

- parsable physical layout;
- no offset/alignment discrepancy attributable to PhylaRAM;
- kernel hints do not create false confidence when absent or invalid;
- unreadable provenance remains available to the examiner;
- independent bundle verification passes before downstream analysis.

Synthetic fixture tests help but do not substitute for this gate.

---

## 10. Release Rule

A public production-readiness claim requires all applicable gates to be green for the exact release candidate.

If a gate is unrun, blocked, or inconclusive, documentation must say so explicitly.
