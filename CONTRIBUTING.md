# Contributing to PhylaRAM

Thank you for your interest in improving PhylaRAM. Because PhylaRAM operates at Ring 0 and produces digital forensics evidence used in investigations and legal proceedings, contributions are held to high standards of correctness, safety, and evidence integrity.

All human-written and AI-generated production code, tests, tooling, documentation, and code reviews **MUST comply with [`ENGINEERING_STANDARD.md`](ENGINEERING_STANDARD.md)**. The requirements below are repository-specific invariants and remain in force where they are stronger or more specific.

---

## Core Engineering Invariants

1. **Evidence Immutability:** Never overwrite, truncate, or mutate existing evidence files. Use collision preflight and stage all writes to `<output>.partial`.
2. **Forensic Truth (`UNREADABLE != ZERO`):** Physical memory that cannot be read is **never** classified as zero-filled data. Isolate bad pages at 4 KiB and record their exact `NTSTATUS` in `<output>.map.json`.
3. **Physical Addressing:** Maintain the invariant `file offset == physical address`. Sparse holes represent unpopulated MMIO/PCIe regions.
4. **Ring 0 Safety:** No arbitrary physical or virtual memory read/write IOCTLs. User mode supplies only a valid run index, offset within that frozen run, and bounded length. No `MmMapIoSpace`, PTE editing, or hooking for RAM acquisition.
5. **Deterministic Ownership:** Win32 handles, services, cryptographic providers, threads, temporary driver files, and staged evidence resources must have explicit deterministic ownership and failure cleanup.
6. **Truthful Publication:** CLI and GUI must use the canonical evidence transaction. No presentation layer may independently decide that evidence is complete or publish sidecars through a competing lifecycle.
7. **Mandatory Integrity:** Every finalized PhylaRAM evidence bundle includes RAW, provenance map, and SHA-256 sidecar. Hash-free or provenance-free finalized capture modes are not supported.

---

## Repository Structure & Language Policy

- **`driver/` (C17 / KMDF 1.15):** Kernel-mode driver `phylaram.sys`. Strict IRQL discipline, Direct I/O MDL mapping, frozen memory-range enumeration, synchronized file-context lifetime.
- **`cli/` (C++20):** Acquisition engine, native GUI, SCM lifecycle, sparse RAW writer, Windows CNG SHA-256 hashing, and transactional evidence publication.
- **`shared/` (C / C++):** Shared ABI and cross-component domain interfaces. ABI layout uses compile-time assertions.
- **`tools/phylaram-verify/` (Rust 2021):** Independent offline verification, schema validation, range/unreadable geometry checks, and property/regression tests.
- **`tests/` (C++20 / Python 3):** Portable unit/model tests plus platform/fixture validation harnesses.
- **`scripts/` (PowerShell / batch):** Build, test-signing, stress, and validation automation.

---

## Development & Testing Workflow

### Portable validation

```bash
clang++ -std=c++20 -Wall -Wextra -Werror tests/test_range_algebra.cpp -o /tmp/test_range_algebra && /tmp/test_range_algebra
clang++ -std=c++20 -Wall -Wextra -Werror tests/test_mock_acquire.cpp -o /tmp/test_mock_acquire && /tmp/test_mock_acquire
clang++ -std=c++20 -Wall -Wextra -Werror tests/test_map_json.cpp -o /tmp/test_map_json && /tmp/test_map_json
clang++ -std=c++20 -Wall -Wextra -Werror tests/test_cli_parser.cpp -o /tmp/test_cli_parser && /tmp/test_cli_parser

cd tools/phylaram-verify
cargo fmt --check
cargo clippy -- -D warnings
cargo test --verbose
```

### Windows build

```cmd
msbuild PhylaRAM.sln /p:Configuration=Release /p:Platform=x64 /v:minimal /m
```

Passing portable tests does not close Windows-only validation gates. Driver Verifier, production signing/HVCI, physical topology, and forensic-tool interoperability must be recorded only after those environments actually run them.

---

## Pull Request Checklist

Before submitting a PR, verify:

- [ ] The final merge gate in `ENGINEERING_STANDARD.md` has been applied to all relevant changes.
- [ ] Portable C++ unit tests pass with zero warnings (`-Wall -Wextra -Werror`).
- [ ] `cargo fmt --check`, `cargo test`, and `cargo clippy -- -D warnings` pass in `tools/phylaram-verify`.
- [ ] Modifications to `shared/phylaram.h` preserve explicit ABI sizing and trigger a protocol compatibility review.
- [ ] No temporary, compiler, evidence, or signing artifacts are committed unintentionally.
- [ ] Fixed defects have regression coverage where practical.
- [ ] No unexecuted validation gate is described as passed.
- [ ] Commit messages explain the technical rationale for material changes.
