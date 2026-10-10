#!/usr/bin/env python3
"""
PhylaRAM portable test battery runner
-------------------------------------
Runs every check that does not need Windows, a kernel driver, or physical
hardware, in the same order as the portable CI job:

  - engineering invariant policy check (scripts/engineering_policy_check.py)
  - C++20 unit/model tests under tests/ (compiled with -Wall -Wextra -Werror)
  - Rust verifier: cargo fmt --check, cargo clippy -D warnings, cargo test
  - Python fixture/topology/hostile-input suites
  - converter and bridge scripts (option parsing only: -h)

Passing this does NOT close any Windows or hardware validation gate; see
docs/STATUS.md. Requires: python3, a C++20 compiler (clang++ by default, or set
CXX), and a Rust toolchain.
"""

import os
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
VERIFIER_DIR = REPO_ROOT / "tools" / "phylaram-verify"
CXX = os.environ.get("CXX", "clang++")
EXE_SUFFIX = ".exe" if os.name == "nt" else ""


def cpp_test(name: str, build_dir: Path):
    exe = build_dir / (name + EXE_SUFFIX)
    return [
        [CXX, "-std=c++20", "-Wall", "-Wextra", "-Werror", str(REPO_ROOT / "tests" / (name + ".cpp")), "-o", str(exe)],
        [str(exe)],
    ]


def build_steps(build_dir: Path):
    py = sys.executable
    cpp_names = [
        ("C++20 range algebra tests", "test_range_algebra"),
        ("C++20 mock read and fault injection tests", "test_mock_acquire"),
        ("C++20 map-2 JSON contract tests", "test_map_json"),
        ("C++20 CLI contract parser tests", "test_cli_parser"),
        ("C++20 ZDMP crash dump format tests", "test_zdmp_format"),
        ("C++20 E01 container format tests", "test_e01_format"),
    ]
    steps = [("Engineering invariant policy check", [[py, "scripts/engineering_policy_check.py"]], REPO_ROOT)]
    steps += [(title, cpp_test(name, build_dir), REPO_ROOT) for title, name in cpp_names]
    steps += [
        (
            "Rust verifier: fmt, clippy, tests",
            [
                ["cargo", "fmt", "--check"],
                ["cargo", "clippy", "--all-targets", "--", "-D", "warnings"],
                ["cargo", "test"],
            ],
            VERIFIER_DIR,
        ),
        ("Synthetic Volatility 3 fixture and bridge", [[py, "tests/test_volatility_fixture.py"]], REPO_ROOT),
        ("Memory topology matrix simulation", [[py, "tests/test_topology_matrix.py"]], REPO_ROOT),
        ("Hostile input and corrupt fixture rejection", [[py, "tests/test_hostile_fuzz_fixtures.py"]], REPO_ROOT),
        ("Offline converter option parser", [[py, "tools/phylaram_convert.py", "-h"]], REPO_ROOT),
        ("MemProcFS bridge option parser", [[py, "tools/phylaram_memprocfs.py", "-h"]], REPO_ROOT),
        ("Volatility 3 bridge option parser", [[py, "tools/phylaram_vol3.py", "-h"]], REPO_ROOT),
    ]
    return steps


def run_step(number: int, total: int, title: str, commands, cwd: Path) -> bool:
    print(f"\n[{number}/{total}] {title}...")
    start = time.time()
    for command in commands:
        result = subprocess.run(command, cwd=cwd, text=True, capture_output=True)
        if result.returncode != 0:
            print(f"  FAIL ({time.time() - start:.2f}s): {' '.join(command)}")
            if result.stdout.strip():
                print(f"  STDOUT:\n{result.stdout}")
            if result.stderr.strip():
                print(f"  STDERR:\n{result.stderr}")
            return False
    print(f"  PASS ({time.time() - start:.2f}s)")
    return True


def main() -> int:
    missing = [tool for tool in (CXX, "cargo") if shutil.which(tool) is None]
    if missing:
        print(f"Missing required tool(s) on PATH: {', '.join(missing)}", file=sys.stderr)
        return 2

    print("PhylaRAM portable test battery (does not close any Windows or hardware gate)")
    with tempfile.TemporaryDirectory(prefix="phylaram-tests-") as build:
        steps = build_steps(Path(build))
        started = time.time()
        for number, (title, commands, cwd) in enumerate(steps, 1):
            if not run_step(number, len(steps), title, commands, cwd):
                print(f"\nABORT: failed at step {number}: {title}")
                return 1
    print(f"\nAll {len(steps)} portable steps passed in {time.time() - started:.2f}s")
    return 0


if __name__ == "__main__":
    sys.exit(main())
