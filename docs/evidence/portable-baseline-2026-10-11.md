# PhylaRAM portable baseline evidence

Internal R&D; recorded October 11, 2026 (America/Los_Angeles). Publication is user-verified in the UI. This follow-up uses the existing setup workspace; it is not proof of a fresh isolated normal task, snapshot restoration, or a recurring dispatcher.

## Revision and instructions

Repository: joeyvictorino/phylaram; checkout: /workspace/phylaram.
Observed starting branch: work. Full base commit: d8734cf12451ae52d8913d1775e1ae17215c006e.
Starting state: clean, including no untracked checkout files; git status --porcelain=v1 returned empty output with exit 0. Ignored tool/build outputs are retained.
Evidence branch: evidence/portable-baseline-2026-10-11, created from the base for this report only.
Read CONTRIBUTING.md, ENGINEERING_STANDARD.md, docs/STATUS.md and tests/run_all_tests.py. No AGENTS.md applies in the checkout or its ancestors; the discovered Cargo dependency AGENTS.md is outside the project scope.
No reproducible portable-code defect was identified by the completed checks. No source, tests, manifests or lockfiles were changed; no code patch is needed.

## Tools and missing-evidence checks

Collected version metadata only in this follow-up; each command below exited 0:
- g++ --version: Debian 14.2.0-19, GCC 14.2.0.
- python3 --version: Python 3.12.14.
- rustc --version: rustc 1.99.0 (b940084d7 2026-09-28).
- cargo --version: cargo 1.99.0 (5f94df478 2026-08-27).
- git branch --show-current: work.
- git rev-parse --verify HEAD^{commit}: the full base above.
- git status --porcelain=v1: empty.
Read-only git ls-remote origin HEAD returned the same base using existing Git authentication.
Tool evidence: follow-up command chunk a3f307 (per-command JSON exit codes).

Common setup shell:
```bash
export CARGO_HOME=/workspace/.phylaram-tools/cargo
export RUSTUP_HOME=/workspace/.phylaram-tools/rustup
export PATH=/workspace/.phylaram-tools/cargo/bin:$PATH
export CXX=g++
export CARGO_BUILD_JOBS=4
cd /workspace/phylaram
```
The repeatable setup script additionally exports RUSTUP_TOOLCHAIN=1.99.0. The initial run used stable, resolving to exactly the same compiler. Python resolved to /opt/codex/runtimes/codex-primary-runtime/dependencies/python/bin/python3.

## Reused completed checks

No passing suite or release-verifier check was rerun in this follow-up. Evidence below is transcribed from retained setup tool outputs, not newly generated raw logs. The original successful runner suppresses subcommand stdout; per-step exit 0 is established by its PASS handling, which requires every subprocess return code to be zero.

Exact suite invocations, cwd /workspace/phylaram:
1. python3 -u tests/run_all_tests.py: exit 0, all 14 steps passed in 12.96 s (session 68969; completion chunk 97f67c).
2. bash /workspace/phylaram-setup.sh: exit 0; script internally ran python3 tests/run_all_tests.py, all 14 steps passed in 6.00 s (session 53961; completion chunk c8d902). The retained script is copied beside this report as setup-script-executed.sh. Its set -euo pipefail establishes success for dependency/build/compileall commands, not merely the last shell command.

| Step | Check | First run seconds | Script run seconds | Outcome |
| --- | --- | ---: | ---: | --- |
| 1 | Engineering invariant policy | 0.02 | 0.02 | PASS, exit 0 |
| 2 | C++20 range algebra | 0.36 | 0.40 | PASS, exit 0 |
| 3 | C++20 mock reads/fault injection | 0.49 | 0.65 | PASS, exit 0 |
| 4 | C++20 map-2 JSON contract | 0.31 | 0.34 | PASS, exit 0 |
| 5 | C++20 CLI parser contract | 0.27 | 0.31 | PASS, exit 0 |
| 6 | C++20 ZDMP format | 0.21 | 0.24 | PASS, exit 0 |
| 7 | C++20 E01 format | 0.21 | 0.22 | PASS, exit 0 |
| 8 | Rust fmt, Clippy, tests | 7.75 | 0.35 | PASS, each command exit 0 |
| 9 | Synthetic bundle and Volatility bridge | 1.14 | 1.17 | PASS, exit 0 |
| 10 | Simulated memory topology | 1.10 | 1.23 | PASS, exit 0 |
| 11 | Hostile/corrupt input rejection | 1.05 | 1.01 | PASS, exit 0 |
| 12 | Converter option parser | 0.02 | 0.02 | PASS, exit 0 |
| 13 | MemProcFS bridge option parser | 0.02 | 0.02 | PASS, exit 0 |
| 14 | Volatility bridge option parser | 0.02 | 0.02 | PASS, exit 0 |

No failed or skipped battery steps. The separate completed command cargo test --locked --offline --manifest-path tools/phylaram-verify/Cargo.toml passed 9 tests: 4 unit, 2 end-to-end, 3 property; main and doc-test targets each had zero tests (not counted as validation). Source: session 33995, completion chunk 61a65f.

Other completed commands from cwd /workspace/phylaram:
- cargo fetch --locked --manifest-path tools/phylaram-verify/Cargo.toml: succeeded in the setup script.
- cargo build --locked --manifest-path tools/phylaram-verify/Cargo.toml: succeeded in the setup script.
- cargo build --release --locked --manifest-path tools/phylaram-verify/Cargo.toml: exit 0, optimized release built; separate set -eu shell session 33995 and subsequent script success.
- cargo test --locked --offline --manifest-path tools/phylaram-verify/Cargo.toml: exit 0, 9 passed.
- python3 -m compileall -q scripts tests tools: exit 0 in the set -eu shell and setup script.
- git status --short: empty after those completed runs.

## Exact release functional check and observed result

Completed during setup from /workspace/phylaram; shell and checked subprocess exit 0; tool chunk 9afddb. Temporary fixture files were cleaned by TemporaryDirectory; they are not retained evidence from an actual acquired host.

```bash
python3 - <<'PY'
import importlib.util, tempfile, subprocess
from pathlib import Path
root=Path('/workspace/phylaram')
spec=importlib.util.spec_from_file_location('fixture',root/'tests/test_volatility_fixture.py')
module=importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
with tempfile.TemporaryDirectory(prefix='phylaram-release-check-') as tmp:
    paths=module.create_mock_windows_ram_fixture(tmp)
    subprocess.run([str(root/'tools/phylaram-verify/target/release/phylaram-verify'), *paths], check=True)
PY
```

Actual verifier argv:
```text
/workspace/phylaram/tools/phylaram-verify/target/release/phylaram-verify /tmp/phylaram-release-check-ztmaz8s0/mock_win11_mem.raw /tmp/phylaram-release-check-ztmaz8s0/mock_win11_mem.raw.map.json /tmp/phylaram-release-check-ztmaz8s0/mock_win11_mem.raw.sha256
```

Observed: [VALID] Evidence bundle verified successfully; schema phylaram-map-2; producer PhylaRAM v0.1.0-alpha; complete; logical 67,108,864 bytes; physical/acquired 66,060,288 bytes; unreadable 0; topology_changed false; 2 RAM runs; 0 unreadable spans.
SHA-256: cbc8617249f51f6f85127c13159052ed5623bd008c8472a7b51840a2c734e744.
The synthetic kernel hints printed by the verifier are fixture data, not observations from a live Windows host.

## Runner command contract

The actual runner uses the Python executable above. From repository root:
- python3 scripts/engineering_policy_check.py.
- For each name test_range_algebra, test_mock_acquire, test_map_json, test_cli_parser, test_zdmp_format, test_e01_format:
  g++ -std=c++20 -Wall -Wextra -Werror /workspace/phylaram/tests/<name>.cpp -o <runner-temporary-directory>/<name>, then executes that binary.
- From /workspace/phylaram/tools/phylaram-verify: cargo fmt --check; cargo clippy --all-targets -- -D warnings; cargo test.
- From repository root: python3 tests/test_volatility_fixture.py; python3 tests/test_topology_matrix.py; python3 tests/test_hostile_fuzz_fixtures.py; python3 tools/phylaram_convert.py -h; python3 tools/phylaram_memprocfs.py -h; python3 tools/phylaram_vol3.py -h.
The runner's temporary C++ build path was not printed and cannot be recovered from the retained output; the template above is explicitly not a claimed exact historical argv. Successful step output was suppressed, so fixture-case counts are not claimed from raw logs.

## Boundaries and next product task

Portable readiness is demonstrated, not comprehensive correctness or a security audit. Windows build/packaging, CodeQL/WDK analysis, Driver Verifier, physical topology, production signing/Secure Boot/HVCI, and real-image interoperability were not executed by this work order; docs/STATUS.md gates remain open. Option parsing does not demonstrate running Volatility or MemProcFS.

Single next product task: create a reproducible, read-only Gate 6 interoperability validation package using one authorized, already-acquired real Windows PhylaRAM RAW/map/SHA bundle. Record bundle hashes and provenance, exact Windows/tool revisions and commands, verifier outcome, and Volatility 3/MemProcFS results against a known lab ground truth; preserve mismatches and scope limits. Obtaining an authorized real bundle is a prerequisite, not a completed result. This moves beyond the present synthetic evidence and supports a credible, narrowly scoped paid audit through independently reviewable findings and explicit acceptance criteria. No customer, revenue, production readiness, or legal-admissibility claim is made.

No main merge, deployment, driver installation, production security change, paid resource, client message, environment-configuration change, or republication occurred. Evidence is preserved as a documentation-only repository commit on the evidence branch, with an external-to-checkout Git bundle for recovery. Remote persistence status and exact artifact paths/commit are reported in the delivery reply.
