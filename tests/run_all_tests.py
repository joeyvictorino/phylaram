#!/usr/bin/env python3
"""
PhylaRAM Comprehensive Test Battery Runner
-------------------------------------------
Executes the full local validation suite:
  1. Engineering Invariant Policy Check (`scripts/engineering_policy_check.py`)
  2. C++20 64-bit Range Algebra Tests (`tests/test_range_algebra.cpp`)
  3. C++20 Mock Read & Fault Injection Tests (`tests/test_mock_acquire.cpp`)
  4. C++20 Map-2 JSON Contract Tests (`tests/test_map_json.cpp`)
  5. C++20 Strict CLI Contract Parser Tests (`tests/test_cli_parser.cpp`)
  6. Rust Verifier Formatting, Clippy & Tests (`tools/phylaram-verify`)
  7. Volatility 3 Synthetic Fixture & Bridge Tests (`tests/test_volatility_fixture.py`)
  8. Memory Topology Matrix Simulation (`tests/test_topology_matrix.py`)
  9. Hostile Input & Corrupt Fixture Rejection Suite (`tests/test_hostile_fuzz_fixtures.py`)
 10. MemProcFS Bridge Options Parser (`tools/phylaram_memprocfs.py`)
"""

import os
import sys
import time
import subprocess
import shutil

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def run_step(step_num, title, cmd, cwd=REPO_ROOT):
    print(f"\n[{step_num}/10] {title}...")
    start = time.time()
    res = subprocess.run(cmd, cwd=cwd, text=True, capture_output=True, shell=isinstance(cmd, str))
    elapsed = time.time() - start

    if res.returncode == 0:
        print(f"  🟢 PASS ({elapsed:.2f}s)")
        if res.stdout.strip():
            for line in res.stdout.strip().splitlines()[-3:]:
                print(f"     {line}")
        return True
    else:
        print(f"  ❌ FAIL ({elapsed:.2f}s)")
        if res.stdout.strip():
            print(f"  STDOUT:\n{res.stdout}")
        if res.stderr.strip():
            print(f"  STDERR:\n{res.stderr}")
        return False

def main():
    print("===============================================================================")
    print("                 PhylaRAM Full Multi-Tier Validation Battery                  ")
    print("===============================================================================")

    steps = [
        ("Engineering Invariant Policy Check", [sys.executable, "scripts/engineering_policy_check.py"], REPO_ROOT),
        ("C++20 64-Bit Range Algebra Tests", "clang++ -std=c++20 -Wall -Wextra -Werror tests/test_range_algebra.cpp -o /tmp/test_range_algebra && /tmp/test_range_algebra", REPO_ROOT),
        ("C++20 Mock Read & Fault Injection Tests", "clang++ -std=c++20 -Wall -Wextra -Werror tests/test_mock_acquire.cpp -o /tmp/test_mock_acquire && /tmp/test_mock_acquire", REPO_ROOT),
        ("C++20 Map-2 JSON Schema Contract Tests", "clang++ -std=c++20 -Wall -Wextra -Werror tests/test_map_json.cpp -o /tmp/test_map_json && /tmp/test_map_json", REPO_ROOT),
        ("C++20 Strict CLI Contract Parser Tests", "clang++ -std=c++20 -Wall -Wextra -Werror tests/test_cli_parser.cpp -o /tmp/test_cli_parser && /tmp/test_cli_parser", REPO_ROOT),
        ("Rust Verifier Format, Clippy & Property Tests", ["cargo", "test", "--verbose"], os.path.join(REPO_ROOT, "tools", "phylaram-verify")),
        ("Volatility 3 Synthetic Fixture & Bridge", [sys.executable, "tests/test_volatility_fixture.py"], REPO_ROOT),
        ("Memory Topology Matrix (5 Scenarios)", [sys.executable, "tests/test_topology_matrix.py"], REPO_ROOT),
        ("Hostile Input & Corrupt Fixture Suite", [sys.executable, "tests/test_hostile_fuzz_fixtures.py"], REPO_ROOT),
        ("ZDMP Complete Crash Dump Format Tests", "clang++ -std=c++20 -Wall -Wextra -Werror tests/test_zdmp_format.cpp -o /tmp/test_zdmp_format && /tmp/test_zdmp_format", REPO_ROOT),
        ("E01 Expert Witness Format Container Tests", "clang++ -std=c++20 -Wall -Wextra -Werror tests/test_e01_format.cpp -o /tmp/test_e01_format && /tmp/test_e01_format", REPO_ROOT),
        ("PhylaRAM Offline Converter Options Parser", [sys.executable, "tools/phylaram_convert.py", "-h"], REPO_ROOT),
        ("MemProcFS Bridge Options Parser", [sys.executable, "tools/phylaram_memprocfs.py", "-h"], REPO_ROOT),
    ]

    total_start = time.time()
    passed = 0
    total = len(steps)

    for i, (title, cmd, cwd) in enumerate(steps, 1):
        if run_step(i, title, cmd, cwd):
            passed += 1
        else:
            print(f"\n[ABORT] Validation battery failed at step {i}: {title}")
            sys.exit(1)

    total_elapsed = time.time() - total_start
    print("\n===============================================================================")
    print(f"  🎉 ALL VALIDATION BATTERIES PASSED: {passed}/{total} (100%) in {total_elapsed:.2f}s")
    print("===============================================================================\n")

if __name__ == "__main__":
    main()
