# Contributing to PhylaRAM

Thank you for your interest in improving PhylaRAM. Because PhylaRAM operates at Ring 0 and produces digital forensics evidence used in investigations and legal proceedings, contributions are held to high standards of correctness, safety, and evidence integrity.

---

## Core Engineering Invariants

1. **Evidence Immutability:** Never overwrite, truncate, or mutate existing evidence files. Use collision preflight and stage all writes to `<output>.partial`.
2. **Forensic Truth (`UNREADABLE != ZERO`):** Physical memory that cannot be read is **never** classified as zero-filled data. Isolate bad pages at 4 KiB and record their exact `NTSTATUS` in `<output>.map.json`.
3. **Physical Addressing:** Maintain the invariant `file offset == physical address`. Sparse holes represent unpopulated MMIO/PCIe regions.
4. **Ring 0 Safety:** No arbitrary physical or virtual memory read/write IOCTLs. User mode supplies only valid run index and offset. No `MmMapIoSpace`, no PTE editing, no hooking.
5. **Deterministic RAII:** All Win32 handles, services, and crypto providers in C++ must use RAII wrappers following the Rule of 5/0.

---

## Repository Structure & Language Policy

- **`driver/` (C / KMDF 1.x):** Kernel-mode driver `phylaram.sys`. Strict IRQL discipline, Direct I/O MDL mapping, memory range enumeration.
- **`cli/` (C++20):** Command-line acquisition engine `phylaram.exe`. SCM lifecycle, sparse raw writer, Windows CNG SHA-256 hashing, provenance map generation.
- **`shared/` (C / C++):** Shared ABI definitions (`shared/phylaram.h`, `shared/interfaces.hpp`). Explicit-width types, compile-time static assertions.
- **`tools/phylaram-verify/` (Rust):** Offline verification tool, schema validation, and property-based test suites.
- **`tests/` (C++20):** Portable unit and mock acquisition tests that run locally on any OS.

---

## Development & Testing Workflow

### Building and Running Tests Locally (macOS / Linux / Windows)

PhylaRAM's test suites and verification tools are portable and can be built and run on macOS, Linux, or Windows without WDK:

```bash
# Portable C++20 Unit Tests
clang++ -std=c++20 -Wall -Wextra -Werror tests/test_range_algebra.cpp -o /tmp/test_range_algebra && /tmp/test_range_algebra
clang++ -std=c++20 -Wall -Wextra -Werror tests/test_mock_acquire.cpp -o /tmp/test_mock_acquire && /tmp/test_mock_acquire
clang++ -std=c++20 -Wall -Wextra -Werror tests/test_map_json.cpp -o /tmp/test_map_json && /tmp/test_map_json
clang++ -std=c++20 -Wall -Wextra -Werror tests/test_cli_parser.cpp -o /tmp/test_cli_parser && /tmp/test_cli_parser

# Rust Verifier & Property Tests
cd tools/phylaram-verify
cargo clippy -- -D warnings
cargo test --verbose
```

### Windows Build (VS2022 + WDK)

```cmd
msbuild PhylaRAM.sln /p:Configuration=Release /p:Platform=x64 /v:minimal /m
```

---

## Pull Request Checklist

Before submitting a PR, verify:
- [ ] All portable C++ unit tests pass with zero warnings (`-Wall -Wextra -Werror`).
- [ ] `cargo test` and `cargo clippy -- -D warnings` pass in `tools/phylaram-verify`.
- [ ] Any modifications to `shared/phylaram.h` preserve 8-byte alignment and explicit struct sizing assertions.
- [ ] No temporary or compiler artifacts are committed.
- [ ] Commit messages clearly explain the technical rationale for the change.
