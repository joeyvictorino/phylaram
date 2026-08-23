# PhylaRAM

> **Live physical-memory acquisition for Windows with explicit unreadable-byte provenance, physical-address-preserving RAW output, and independent offline verification.**

[![CI](https://github.com/joeyvictorino/phylaram/actions/workflows/ci.yml/badge.svg)](https://github.com/joeyvictorino/phylaram/actions/workflows/ci.yml)
[![Pre-release](https://img.shields.io/github/v/release/joeyvictorino/phylaram?include_prereleases&label=pre-release)](https://github.com/joeyvictorino/phylaram/releases)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

> [!WARNING]
> **Alpha software. Do not use PhylaRAM as the sole acquisition mechanism for production evidence yet.** The repository still has open Windows validation gates for Driver Verifier stress, physical-hardware/topology coverage, production Microsoft driver signing with Secure Boot/HVCI, and real forensic-tool interoperability. CI artifacts are test-signed for controlled test environments.

PhylaRAM currently targets:

- Windows 10 version 2004 or later
- Windows 11
- x64 only

The normative engineering and review requirements are in [`ENGINEERING_STANDARD.md`](ENGINEERING_STANDARD.md). Current validation status is tracked in [`docs/STATUS.md`](docs/STATUS.md).

---

## Acquisition Model

PhylaRAM's acquisition design is intentionally narrow.

### Physical addressing

For populated physical memory, the RAW file uses:

```text
file offset == physical address
```

Unpopulated physical-address gaps are represented as sparse holes when the destination filesystem supports sparse files.

### `UNREADABLE != ZERO`

If a bulk physical read is partial, PhylaRAM preserves every transferred byte and retries the unresolved interval at page granularity. An unreadable physical span is recorded in `memory.raw.map.json` with its location, length, and observed `NTSTATUS`.

The flat RAW representation has no native symbol for “unknown,” so an unreadable interval reads as zero bytes in the logical file. **Those zeros are representation bytes, not an observation that RAM contained zero.** The provenance map carries that distinction.

For that reason, raw stdout acquisition is intentionally unsupported: a byte stream without its provenance map cannot preserve `UNREADABLE != ZERO`.

### Frozen run-index protocol

User mode does not submit arbitrary physical addresses to the driver. A read identifies:

- a run index from the session's frozen physical-memory topology;
- an offset within that run;
- a bounded requested length.

The driver derives the physical address after validating the request against the frozen run.

### Terminal topology check

At session end the driver re-enumerates physical-memory ranges and compares them to the acquisition-start snapshot. A topology change is preserved as an incomplete result rather than silently reported as complete.

---

## Finalized Evidence Bundle

A finalized capture always consists of all three files:

```text
memory.raw
memory.raw.map.json
memory.raw.sha256
```

SHA-256 and provenance are mandatory. There is no supported hash-free finalized bundle.

Publication is transactional at the application level: acquisition occurs into staging files, the hash and sidecars are finalized and flushed, and the canonical RAW filename is published only after the other staged components have been finalized. Known finalization failures do not produce a successful terminal result.

### Terminal outcomes

| Exit code | Meaning |
| ---: | --- |
| `0` | **Complete:** acquisition reached the end, byte accounting balanced, topology was unchanged, and the evidence bundle finalized. |
| `2` | **Incomplete:** the finalized bundle records unreadable memory and/or a topology change. |
| `1` | **Failed/cancelled:** no successful final evidence bundle was published, or post-capture driver cleanup failed. |

A successful producer exit does **not** replace independent verification. Run `phylaram-verify` before relying on the bundle.

---

## Usage

Run from an elevated Command Prompt or PowerShell:

```cmd
phylaram.exe C:\evidence\memory.raw
```

Optional rate limiting is specified in MiB/s:

```cmd
phylaram.exe C:\evidence\memory.raw --rate-limit 100
```

Other commands:

```cmd
phylaram.exe --dry-run
phylaram.exe --dry-run --json
phylaram.exe --gui
phylaram.exe --help
```

`--dry-run` opens the driver, captures topology/kernel-hint telemetry, performs the session-end topology comparison, and creates no evidence bundle.

The native GUI is a presentation layer over the same capture transaction used by the CLI. It does not maintain a second evidence-finalization implementation.

---

## Provenance Map

The current writer emits `phylaram-map-2`. See [`docs/MAP_SCHEMA.md`](docs/MAP_SCHEMA.md) for the field contract.

The canonical map contains acquisition facts such as:

- producer/schema version;
- complete vs incomplete terminal status;
- logical size;
- physical/acquired/unreadable byte counts;
- topology-change result;
- SHA-256;
- frozen physical-memory ranges;
- unreadable spans and NTSTATUS values;
- optional live kernel hints.

Analytic heuristics, entropy classifications, ATT&CK mappings, and compliance interpretations are not provenance facts and are not part of the canonical acquisition map.

---

## Offline Verification

`tools/phylaram-verify` is an independent Rust verifier:

```bash
phylaram-verify memory.raw memory.raw.map.json memory.raw.sha256
```

It verifies, among other invariants:

- RAW size equals the map's logical size;
- physical ranges are non-zero, ordered, non-overlapping, arithmetically valid, and bounded by logical size;
- the highest physical range end equals logical size;
- `driver_run` values form the expected unique zero-based domain;
- physical byte totals equal the checked sum of ranges;
- unreadable spans are ordered, non-overlapping, and wholly contained in reported physical RAM;
- unreadable totals and acquired/unreadable accounting balance exactly;
- RAW bytes represented as unreadable are zero-backed in the flat representation;
- terminal status matches topology/unreadable conditions;
- the logical RAW SHA-256 matches the map and optional `.sha256` sidecar.

Verification establishes internal consistency of the supplied bundle. It does not by itself establish host identity, chain of custody, legal admissibility, or the truth of metadata supplied outside the verified relationships.

---

## Kernel Hints

The current driver can record optional acquisition-time hints including:

- System-process CR3 / Directory Table Base;
- executing processor KPCR address;
- NTOSKRNL base and image size;
- Windows version/build;
- active processor count;
- CPUID hypervisor-present bit.

These are analysis hints, not substitutes for independent validation by downstream tooling.

---

## Driver Trust and Signing

The elevated executable extracts the driver embedded in its own image into a protected temporary directory. An adjacent `phylaram.sys` does not silently override that embedded resource.

Current CI/pre-release binaries use test signing for controlled test environments. Production Windows deployment requires completion of the repository's Microsoft driver-signing and Secure Boot/HVCI validation gate. Microsoft attestation signing and Windows Hardware Compatibility Program certification are distinct signing/certification paths; the project will document the exact chosen production path when that gate is completed.

Do not disable production security controls on an evidence host merely to make an alpha driver load.

---

## Building

Requirements:

- Visual Studio 2022 / MSVC v143
- Windows Driver Kit toolchain restored as configured by the repository
- Rust toolchain for `phylaram-verify`

Windows build:

```cmd
nuget restore packages.config -PackagesDirectory packages
msbuild PhylaRAM.sln /p:Configuration=Release /p:Platform=x64 /v:minimal /m
```

Portable validation used by CI includes:

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

Passing these checks does not close Windows-only hardware/runtime validation gates.

---

## Validation Status

See [`docs/STATUS.md`](docs/STATUS.md) and the open validation-gate issues before using an artifact outside a lab.

In particular, a green compile/test CI run is not equivalent to:

- Driver Verifier stress validation;
- physical 4 GB through 128 GB+/NUMA/ReBAR validation;
- production Microsoft signing under Secure Boot and HVCI;
- real acquired-image interoperability across supported Windows builds.

Those claims become true only when those tests actually run and their results are recorded.

---

## License

PhylaRAM is licensed under the [MIT License](LICENSE).
