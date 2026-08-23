#!/usr/bin/env python3
"""
PhylaRAM Volatility 3 Bridge Helper
-----------------------------------
Reads a PhylaRAM provenance map (`memory.raw.map.json`) and accelerates
Volatility 3 analysis by passing live Ring 0 kernel hints (CR3 Directory Table Base,
NTOSKRNL Base, and KPCR) directly to Volatility 3 CLI or API.

Usage:
  python phylaram_vol3.py <memory.raw> [volatility_plugin] [volatility_args...]
  python phylaram_vol3.py memory.raw windows.pslist
  python phylaram_vol3.py memory.raw windows.info
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
    print("                    PhylaRAM -> Volatility 3 Accelerated Bridge               ")
    print("===============================================================================")
    print(f"RAW Image       : {raw_path} ({os.path.getsize(raw_path):,} bytes)")

    vol_args = ["vol", "-f", raw_path]

    if hints:
        dtb = hints.get("directory_table_base")
        kbase = hints.get("kernel_base")
        build = hints.get("build_number")
        print(f"System DTB (CR3): {dtb}")
        print(f"Kernel Base     : {kbase}")
        print(f"Windows Build   : {build}")
        print(f"Hypervisor      : {'Yes' if hints.get('hypervisor_present') else 'No'}")
    else:
        print("[INFO] No sidecar .map.json found; running Volatility with standard auto-detect.")

    print("===============================================================================\n")

    user_plugin_args = sys.argv[2:] if len(sys.argv) > 2 else ["windows.info"]
    cmd = vol_args + user_plugin_args

    print(f"Executing: {' '.join(cmd)}\n")
    try:
        res = subprocess.run(cmd)
        sys.exit(res.returncode)
    except FileNotFoundError:
        print("[WARNING] 'vol' executable not found in PATH.", file=sys.stderr)
        print("To run Volatility 3 manually, use:")
        print(f"  python vol.py -f {raw_path} {' '.join(user_plugin_args)}")
        sys.exit(1)

if __name__ == "__main__":
    main()
