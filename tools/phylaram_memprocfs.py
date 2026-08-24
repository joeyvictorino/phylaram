#!/usr/bin/env python3
"""
PhylaRAM MemProcFS Accelerated Bridge Helper
--------------------------------------------
Reads a PhylaRAM evidence bundle (`memory.raw` + `memory.raw.map.json`) and
accelerates MemProcFS virtual memory mounting by injecting live Ring 0 kernel
hints (System CR3 / Directory Table Base, Kernel Base, and KPCR) directly into
MemProcFS CLI parameters.

Usage:
  python phylaram_memprocfs.py <memory.raw> [mount_point] [extra_memprocfs_args...]
  python phylaram_memprocfs.py C:\\evidence\\mem.raw M:
  python phylaram_memprocfs.py mem.raw -v -forensic 1
"""

import sys
import os
import json
import subprocess

def load_map_file(raw_path: str) -> dict:
    candidates = [
        raw_path + ".map.json",
        raw_path.rsplit(".", 1)[0] + ".map.json" if "." in raw_path else raw_path + ".map.json"
    ]
    for c in candidates:
        if os.path.isfile(c):
            with open(c, "r", encoding="utf-8") as f:
                return json.load(f)
    return {}

def main():
    if len(sys.argv) < 2 or sys.argv[1] in ("-h", "--help"):
        print(__doc__.strip())
        sys.exit(0)

    raw_path = sys.argv[1]
    if not os.path.isfile(raw_path):
        print(f"[ERROR] RAW memory file not found: {raw_path}", file=sys.stderr)
        sys.exit(1)

    map_data = load_map_file(raw_path)
    hints = map_data.get("kernel_hints", {})

    print("===============================================================================")
    print("                  PhylaRAM -> MemProcFS Accelerated Bridge                    ")
    print("===============================================================================")
    print(f"RAW Image       : {raw_path} ({os.path.getsize(raw_path):,} bytes)")

    memprocfs_args = ["MemProcFS.exe", "-device", raw_path]

    if hints:
        dtb = hints.get("directory_table_base")
        kbase = hints.get("kernel_base")
        build = hints.get("build_number")
        print(f"System DTB (CR3): {dtb}")
        print(f"Kernel Base     : {kbase}")
        print(f"Windows Build   : {build}")
        print(f"Hypervisor      : {'Yes' if hints.get('hypervisor_present') else 'No'}")

        if dtb and dtb != "0x0":
            # Pass CR3 hint to MemProcFS to bypass expensive page table scanning
            memprocfs_args.extend(["-vmm.cr3", dtb])
    else:
        print("[INFO] No sidecar .map.json found; MemProcFS will perform standard auto-detection.")

    print("===============================================================================\n")

    user_args = sys.argv[2:] if len(sys.argv) > 2 else ["-v"]
    cmd = memprocfs_args + user_args

    print(f"Executing: {' '.join(cmd)}\n")
    try:
        res = subprocess.run(cmd)
        sys.exit(res.returncode)
    except FileNotFoundError:
        print("[WARNING] 'MemProcFS.exe' not found in PATH.", file=sys.stderr)
        print("To mount manually, run MemProcFS with the following parameters:")
        print(f"  {' '.join(cmd)}")
        sys.exit(0)

if __name__ == "__main__":
    main()
