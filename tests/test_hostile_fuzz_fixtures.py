#!/usr/bin/env python3
"""
PhylaRAM Hostile Input & Corrupt Fixture Test Suite
---------------------------------------------------
Systematically constructs corrupted, tampered, and adversarial evidence bundles
and proves that `phylaram-verify` strictly rejects every single invalid condition.

Covers:
  1. Valid baseline bundle (Must PASS)
  2. Truncated RAW file (Must REJECT)
  3. Inflated RAW file (Must REJECT)
  4. Bit-flipped RAW byte / SHA-256 digest mismatch (Must REJECT)
  5. Tampered SHA-256 sidecar (Must REJECT)
  6. Overlapping physical memory runs (Must REJECT)
  7. Out-of-order physical memory runs (Must REJECT)
  8. Zero-length memory run (Must REJECT)
  9. Run integer overflow at UINT64_MAX (Must REJECT)
 10. Unreadable span outside physical RAM bounds (Must REJECT)
 11. Overlapping unreadable spans (Must REJECT)
 12. Non-zero data in RAW file at unreadable span offset (Must REJECT)
 13. Map JSON syntax error / malformed JSON (Must REJECT)
 14. Missing required field in map JSON (Must REJECT)
 15. Contradictory status (claimed 'complete' with unreadable > 0) (Must REJECT)
 16. Contradictory status (claimed 'complete' with topology_changed=True) (Must REJECT)
"""

import os
import sys
import json
import hashlib
import tempfile
import subprocess
import shutil

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VERIFIER_DIR = os.path.join(REPO_ROOT, "tools", "phylaram-verify")
VERIFIER_BIN = os.path.join(VERIFIER_DIR, "target", "debug", "phylaram-verify")

def ensure_verifier_binary():
    if not os.path.isfile(VERIFIER_BIN):
        print("[SETUP] Building Rust verifier binary...")
        subprocess.run(["cargo", "build"], cwd=VERIFIER_DIR, check=True)

def create_base_bundle(tmpdir, prefix="base"):
    raw_path = os.path.join(tmpdir, f"{prefix}.raw")
    map_path = raw_path + ".map.json"
    sha_path = raw_path + ".sha256"

    # Layout: 16 MB logical with 2 runs
    # Run 0: 0x00000000 -> 0x00400000 (4 MB)
    # Gap  : 0x00400000 -> 0x00800000 (4 MB MMIO gap)
    # Run 1: 0x00800000 -> 0x01000000 (8 MB)
    logical_size = 0x01000000 # 16 MB
    run0_base, run0_len = 0x0, 0x00400000
    run1_base, run1_len = 0x00800000, 0x00800000

    physical_bytes = run0_len + run1_len
    acquired_bytes = physical_bytes
    unreadable_bytes = 0

    with open(raw_path, "wb") as f:
        f.seek(run0_base)
        f.write(b"\xAA" * run0_len)
        f.seek(run1_base)
        f.write(b"\xBB" * run1_len)
        f.seek(logical_size - 1)
        f.write(b"\x00")

    hasher = hashlib.sha256()
    with open(raw_path, "rb") as f:
        while chunk := f.read(1024 * 1024):
            hasher.update(chunk)
    digest = hasher.hexdigest()

    map_dict = {
        "producer": "PhylaRAM",
        "producer_version": "0.1.0-alpha",
        "schema": "phylaram-map-2",
        "status": "complete",
        "logical_size": logical_size,
        "physical_bytes": physical_bytes,
        "acquired_bytes": acquired_bytes,
        "unreadable_bytes": unreadable_bytes,
        "topology_changed": False,
        "sha256": digest,
        "kernel_hints": {
            "hypervisor_present": False,
            "directory_table_base": "0x1AA000",
            "kpcr_address": "0xFFFFF78000000000",
            "kernel_base": "0xFFFFF80004000000",
            "kernel_size": 12582912,
            "major_version": 10,
            "minor_version": 0,
            "build_number": 22631,
            "processors": 8
        },
        "ranges": [
            {"driver_run": 0, "start": hex(run0_base).upper(), "length": run0_len},
            {"driver_run": 1, "start": hex(run1_base).upper(), "length": run1_len}
        ],
        "unreadable": []
    }

    with open(map_path, "w", encoding="utf-8") as f:
        json.dump(map_dict, f, indent=2)

    with open(sha_path, "w", encoding="utf-8") as f:
        f.write(f"{digest}  {os.path.basename(raw_path)}\n")

    return raw_path, map_path, sha_path, map_dict, digest

def run_verifier(raw, map_file, sha):
    cmd = [VERIFIER_BIN, raw, map_file, sha]
    res = subprocess.run(cmd, capture_output=True, text=True)
    return res.returncode == 0, res.stdout, res.stderr

def run_test(name, mutate_fn, should_pass=False):
    with tempfile.TemporaryDirectory() as tmpdir:
        raw, map_file, sha, map_dict, digest = create_base_bundle(tmpdir)
        mutate_fn(raw, map_file, sha, map_dict, digest)
        ok, stdout, stderr = run_verifier(raw, map_file, sha)

        if ok == should_pass:
            print(f"  [PASS] {name} -> Correctly {'ACCEPTED' if should_pass else 'REJECTED'}")
            return True
        else:
            print(f"  [FAIL] {name} -> Expected {'PASS' if should_pass else 'REJECT'}, but got {'PASS' if ok else 'FAIL'}")
            if not ok:
                print(f"         Error log: {stderr.strip() or stdout.strip()}")
            return False

def main():
    ensure_verifier_binary()
    print("===============================================================================")
    print("        PhylaRAM Hostile / Corrupt Input Rejection Test Suite                 ")
    print("===============================================================================")

    tests = []

    # 1. Baseline
    tests.append(("Baseline Valid Bundle", lambda r, m, s, d, h: None, True))

    # 2. Truncated RAW
    def mutate_truncate_raw(r, m, s, d, h):
        size = os.path.getsize(r)
        with open(r, "r+b") as f:
            f.truncate(size - 4096)
    tests.append(("Truncated RAW File", mutate_truncate_raw, False))

    # 3. Inflated RAW
    def mutate_inflate_raw(r, m, s, d, h):
        with open(r, "ab") as f:
            f.write(b"\x00" * 4096)
    tests.append(("Inflated RAW File", mutate_inflate_raw, False))

    # 4. Bit-flipped RAW byte
    def mutate_bitflip_raw(r, m, s, d, h):
        with open(r, "r+b") as f:
            f.seek(1024)
            byte = f.read(1)
            flipped = bytes([byte[0] ^ 0xFF])
            f.seek(1024)
            f.write(flipped)
    tests.append(("Bit-Flipped RAW Byte (Hash Mismatch)", mutate_bitflip_raw, False))

    # 5. Tampered SHA sidecar
    def mutate_tamper_sha(r, m, s, d, h):
        with open(s, "w") as f:
            f.write("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef  test.raw\n")
    tests.append(("Tampered SHA-256 Sidecar", mutate_tamper_sha, False))

    # 6. Overlapping runs
    def mutate_overlapping_runs(r, m, s, d, h):
        d["ranges"][1]["start"] = "0x00300000" # overlaps with run 0 (0..0x400000)
        with open(m, "w") as f:
            json.dump(d, f, indent=2)
    tests.append(("Overlapping Memory Runs in Map", mutate_overlapping_runs, False))

    # 7. Out of order runs
    def mutate_unordered_runs(r, m, s, d, h):
        d["ranges"][0], d["ranges"][1] = d["ranges"][1], d["ranges"][0]
        with open(m, "w") as f:
            json.dump(d, f, indent=2)
    tests.append(("Unordered Memory Runs in Map", mutate_unordered_runs, False))

    # 8. Zero-length run
    def mutate_zero_len_run(r, m, s, d, h):
        d["ranges"][0]["length"] = 0
        with open(m, "w") as f:
            json.dump(d, f, indent=2)
    tests.append(("Zero-Length Run in Map", mutate_zero_len_run, False))

    # 9. Integer overflow in run
    def mutate_overflow_run(r, m, s, d, h):
        d["ranges"][1]["start"] = "0xFFFFFFFFFFFFFFFF"
        d["ranges"][1]["length"] = 4096
        with open(m, "w") as f:
            json.dump(d, f, indent=2)
    tests.append(("Integer Overflow in Run Range", mutate_overflow_run, False))

    # 10. Unreadable span outside physical bounds
    def mutate_unreadable_outside(r, m, s, d, h):
        d["status"] = "incomplete"
        d["unreadable_bytes"] = 4096
        d["acquired_bytes"] -= 4096
        d["unreadable"] = [{
            "start": "0x00500000", # in the MMIO hole!
            "length": 4096,
            "status_code": "0xC0000185"
        }]
        with open(m, "w") as f:
            json.dump(d, f, indent=2)
    tests.append(("Unreadable Span in Hardware MMIO Gap", mutate_unreadable_outside, False))

    # 11. Overlapping unreadable spans
    def mutate_overlapping_unreadable(r, m, s, d, h):
        d["status"] = "incomplete"
        d["unreadable_bytes"] = 8192
        d["acquired_bytes"] -= 8192
        d["unreadable"] = [
            {"start": "0x00001000", "length": 4096, "status_code": "0xC0000185"},
            {"start": "0x00001800", "length": 4096, "status_code": "0xC0000185"}
        ]
        with open(m, "w") as f:
            json.dump(d, f, indent=2)
    tests.append(("Overlapping Unreadable Spans", mutate_overlapping_unreadable, False))

    # 12. Non-zero bytes at unreadable offset
    def mutate_nonzero_unreadable(r, m, s, d, h):
        d["status"] = "incomplete"
        d["unreadable_bytes"] = 4096
        d["acquired_bytes"] -= 4096
        d["unreadable"] = [{
            "start": "0x00001000",
            "length": 4096,
            "status_code": "0xC0000185"
        }]
        # The base bundle filled run 0 with 0xAA, so offset 0x1000 contains 0xAA (non-zero!)
        with open(m, "w") as f:
            json.dump(d, f, indent=2)
    tests.append(("Non-Zero Bytes at Unreadable Offset", mutate_nonzero_unreadable, False))

    # 13. Map JSON syntax error
    def mutate_json_syntax(r, m, s, d, h):
        with open(m, "w") as f:
            f.write("{\n  \"producer\": \"PhylaRAM\",\n  INVALID_JSON\n")
    tests.append(("Malformed JSON Map Syntax", mutate_json_syntax, False))

    # 14. Missing required field
    def mutate_missing_field(r, m, s, d, h):
        del d["logical_size"]
        with open(m, "w") as f:
            json.dump(d, f, indent=2)
    tests.append(("Missing Required 'logical_size' Field", mutate_missing_field, False))

    # 15. Contradictory status (complete with unreadable > 0)
    def mutate_contradictory_status_unreadable(r, m, s, d, h):
        d["status"] = "complete" # contradiction!
        d["unreadable_bytes"] = 4096
        d["acquired_bytes"] -= 4096
        d["unreadable"] = [{
            "start": "0x00001000",
            "length": 4096,
            "status_code": "0xC0000185"
        }]
        # Zero out the unreadable range in RAW
        with open(r, "r+b") as f:
            f.seek(0x1000)
            f.write(b"\x00" * 4096)
        # Update hash
        hasher = hashlib.sha256()
        with open(r, "rb") as f:
            while chunk := f.read(1024 * 1024):
                hasher.update(chunk)
        new_digest = hasher.hexdigest()
        d["sha256"] = new_digest
        with open(m, "w") as f:
            json.dump(d, f, indent=2)
        with open(s, "w") as f:
            f.write(f"{new_digest}  {os.path.basename(r)}\n")
    tests.append(("Contradictory Status: Complete with Unreadable Bytes", mutate_contradictory_status_unreadable, False))

    # 16. Contradictory status (complete with topology_changed=True)
    def mutate_contradictory_status_topology(r, m, s, d, h):
        d["status"] = "complete" # contradiction!
        d["topology_changed"] = True
        with open(m, "w") as f:
            json.dump(d, f, indent=2)
    tests.append(("Contradictory Status: Complete with Topology Changed", mutate_contradictory_status_topology, False))

    passed = 0
    total = len(tests)
    for name, mutate_fn, should_pass in tests:
        if run_test(name, mutate_fn, should_pass):
            passed += 1

    print("===============================================================================")
    print(f"  Results: {passed}/{total} Scenarios Handled Correctly ({passed/total*100:.1f}%)")
    print("===============================================================================")

    if passed != total:
        sys.exit(1)
    print("\n[SUCCESS] Hostile input rejection tests passed 100% soundly.\n")

if __name__ == "__main__":
    main()
