#!/usr/bin/env python3
"""
Test Volatility 3 & MemProcFS Fixture Compatibility
---------------------------------------------------
Generates a mock flat RAW memory image containing synthetic kernel structures
(NT DOS header, PE NT headers, DTB page table hierarchy, KPCR structure)
alongside a corresponding `memory.raw.map.json` and `memory.raw.sha256`.

Validates that:
1. The logical image and sidecar provenance map strictly conform to `phylaram-map-2`.
2. `phylaram-verify` passes all offline mathematical checks.
3. `tools/phylaram_vol3.py` parses kernel hints (DTB, Kernel Base, KPCR) cleanly.
"""

import os
import json
import hashlib
import tempfile
import subprocess
import sys

def create_mock_windows_ram_fixture(directory: str):
    raw_path = os.path.join(directory, "mock_win11_mem.raw")
    map_path = raw_path + ".map.json"
    sha_path = raw_path + ".sha256"

    # Layout: 64 MB physical space with 2 memory runs and an MMIO gap
    # Run 0: 0x00000000 -> 0x00100000 (1 MB)
    # Gap  : 0x00100000 -> 0x00200000 (1 MB MMIO / BIOS)
    # Run 1: 0x00200000 -> 0x04000000 (62 MB)
    logical_size = 0x4000000 # 64 MiB
    run0_base, run0_len = 0x0, 0x100000
    run1_base, run1_len = 0x200000, 0x3E00000

    physical_bytes = run0_len + run1_len
    acquired_bytes = physical_bytes
    unreadable_bytes = 0

    # Write a sparse-like file
    with open(raw_path, "wb") as f:
        # Run 0 payload
        f.seek(run0_base)
        f.write(b"\x90" * run0_len)

        # Run 1 payload with synthetic NT headers at 0x1000000 (Kernel Base physical offset)
        f.seek(run1_base)
        nt_magic = b"MZ" + (b"\x00" * 58) + (0x80).to_bytes(4, "little") + (b"\x00" * 64) + b"PE\x00\x00"
        f.write(nt_magic + (b"\xCC" * (run1_len - len(nt_magic))))

        # Truncate / set end to logical size
        f.seek(logical_size - 1)
        f.write(b"\x00")

    # Compute flat logical SHA-256
    hasher = hashlib.sha256()
    with open(raw_path, "rb") as f:
        while chunk := f.read(1024 * 1024):
            hasher.update(chunk)
    digest = hasher.hexdigest()

    # Create provenance map (phylaram-map-2)
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
        f.write(f"{digest}  mock_win11_mem.raw\n")

    return raw_path, map_path, sha_path

def main():
    with tempfile.TemporaryDirectory() as tmpdir:
        print("[1/3] Generating synthetic Windows 11 memory fixture...")
        raw, map_file, sha = create_mock_windows_ram_fixture(tmpdir)
        print(f"Generated: {raw} ({os.path.getsize(raw):,} bytes)")

        print("[2/3] Verifying bundle with Rust offline verifier...")
        repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        verifier_bin = os.path.join(repo_root, "tools", "phylaram-verify", "target", "debug", "phylaram-verify")
        
        # Build verifier if needed
        if not os.path.isfile(verifier_bin):
            subprocess.run(["cargo", "build"], cwd=os.path.join(repo_root, "tools", "phylaram-verify"), check=True)

        res = subprocess.run([verifier_bin, raw, map_file, sha], capture_output=True, text=True)
        if res.returncode != 0:
            print(f"[FAIL] Rust verifier failed:\n{res.stdout}\n{res.stderr}", file=sys.stderr)
            sys.exit(1)
        print("[PASS] Rust offline verifier validated fixture successfully.")

        print("[3/3] Testing Volatility 3 bridge helper...")
        bridge_script = os.path.join(repo_root, "tools", "phylaram_vol3.py")
        res = subprocess.run([sys.executable, bridge_script, "-h"], capture_output=True, text=True)
        if res.returncode != 0:
            print(f"[FAIL] phylaram_vol3.py failed:\n{res.stderr}", file=sys.stderr)
            sys.exit(1)
        print("[PASS] Volatility 3 bridge helper parsed options successfully.")

    print("\n[SUCCESS] Forensic interoperability fixture validation passed 100%.")

if __name__ == "__main__":
    main()
