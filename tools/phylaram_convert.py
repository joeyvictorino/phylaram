#!/usr/bin/env python3
"""
PhylaRAM Offline Evidence Format Converter
-------------------------------------------
Converts a canonical PhylaRAM evidence capture (`memory.raw` + `memory.raw.map.json`)
into downstream forensic containers:
  - ZDMP / DMP : Microsoft 64-bit Complete Memory Crash Dump
  - E01        : Expert Witness Compression Format (EnCase EWF)

Usage:
  python phylaram_convert.py <input.raw> --to zdmp [-o output.zdmp]
  python phylaram_convert.py <input.raw> --to e01  [-o output.e01] [--case CASE-01] [--examiner EXAMINER]
"""

import sys
import os
import json
import struct
import zlib
import argparse
import hashlib

def adler32(data: bytes, adler: int = 1) -> int:
    return zlib.adler32(data, adler) & 0xFFFFFFFF

def load_map(raw_path: str) -> dict:
    candidates = [
        raw_path + ".map.json",
        raw_path.rsplit(".", 1)[0] + ".map.json" if "." in raw_path else raw_path + ".map.json"
    ]
    for c in candidates:
        if os.path.isfile(c):
            with open(c, "r", encoding="utf-8") as f:
                return json.load(f)
    return {}

def convert_to_zdmp(raw_path: str, output_path: str, map_data: dict):
    print(f"[CONVERT] Converting {raw_path} -> {output_path} (Microsoft Complete Crash Dump)...")
    hints = map_data.get("kernel_hints", {})
    ranges = map_data.get("ranges", [])
    
    # If map has no ranges, infer single contiguous run
    if not ranges:
        raw_size = os.path.getsize(raw_path)
        ranges = [{"driver_run": 0, "start": "0x0", "length": raw_size}]

    num_runs = min(len(ranges), 32)
    total_physical_bytes = sum(int(r["length"]) for r in ranges)
    total_pages = total_physical_bytes // 4096

    # Build 4096-byte DUMP_HEADER64
    header = bytearray(4096)

    # Offsets:
    # 0x0000: 'PAGE' (0x45474150)
    # 0x0004: 'DU64' (0x34365544)
    # 0x0008: MajorVersion (15)
    # 0x000C: MinorVersion (BuildNumber)
    # 0x0010: DirectoryTableBase (uint64)
    # 0x0030: MachineImageType (0x8664)
    # 0x0034: NumberProcessors
    # 0x0038: BugCheckCode (0x161)
    # 0x0060: VersionUser (32 bytes)
    # 0x00F8: PhysicalMemoryBlock (NumberOfRuns, Reserved, NumberOfPages, Runs[...])
    # 0x0F88: DumpType (1)

    struct.pack_into("<I", header, 0x0000, 0x45474150) # 'PAGE'
    struct.pack_into("<I", header, 0x0004, 0x34365544) # 'DU64'
    struct.pack_into("<I", header, 0x0008, 15)
    build_number = hints.get("build_number", 22631)
    struct.pack_into("<I", header, 0x000C, build_number)

    dtb_str = hints.get("directory_table_base", "0x0")
    dtb = int(dtb_str, 16) if isinstance(dtb_str, str) else int(dtb_str)
    struct.pack_into("<Q", header, 0x0010, dtb)

    kbase_str = hints.get("kernel_base", "0x0")
    kbase = int(kbase_str, 16) if isinstance(kbase_str, str) else int(kbase_str)
    struct.pack_into("<Q", header, 0x0020, kbase)

    struct.pack_into("<I", header, 0x0030, 0x8664) # AMD64
    num_cpus = hints.get("processors", 1)
    struct.pack_into("<I", header, 0x0034, num_cpus)
    struct.pack_into("<I", header, 0x0038, 0x161)   # LIVE_SYSTEM_DUMP

    version_str = b"PhylaRAM 0.1.0-alpha\x00"
    header[0x0060:0x0060 + len(version_str)] = version_str

    # PhysicalMemoryBlock at 0x00F8
    struct.pack_into("<IIQ", header, 0x00F8, num_runs, 0, total_pages)
    run_offset = 0x00F8 + 16
    for i in range(num_runs):
        base_int = int(ranges[i]["start"], 16) if isinstance(ranges[i]["start"], str) else int(ranges[i]["start"])
        len_int = int(ranges[i]["length"])
        base_page = base_int // 4096
        page_count = len_int // 4096
        struct.pack_into("<QQ", header, run_offset + (i * 16), base_page, page_count)

    struct.pack_into("<I", header, 0x0F88, 1) # Complete Dump

    with open(output_path, "wb") as out_f, open(raw_path, "rb") as in_f:
        out_f.write(header)
        for r in ranges:
            start = int(r["start"], 16) if isinstance(r["start"], str) else int(r["start"])
            length = int(r["length"])
            in_f.seek(start)
            remaining = length
            while remaining > 0:
                chunk_len = min(remaining, 1024 * 1024)
                data = in_f.read(chunk_len)
                if not data:
                    break
                out_f.write(data)
                remaining -= len(data)

    print(f"[SUCCESS] ZDMP/DMP created: {output_path} ({os.path.getsize(output_path):,} bytes)")

def convert_to_e01(raw_path: str, output_path: str, map_data: dict, case="CASE-001", examiner="PhylaRAM"):
    print(f"[CONVERT] Converting {raw_path} -> {output_path} (Expert Witness E01 Container)...")
    ranges = map_data.get("ranges", [])
    if not ranges:
        raw_size = os.path.getsize(raw_path)
        ranges = [{"driver_run": 0, "start": "0x0", "length": raw_size}]

    total_physical_bytes = sum(int(r["length"]) for r in ranges)
    chunk_size = 65536 # 64 KiB
    total_chunks = (total_physical_bytes + chunk_size - 1) // chunk_size

    with open(output_path, "wb") as out_f, open(raw_path, "rb") as in_f:
        # File header (13 bytes)
        file_hdr = b"EVF\x09\x0d\x0a\xff\x00\x01\x00\x00\x00\x00"
        out_f.write(file_hdr)

        # Header Section
        meta_str = (
            f"case_number\t{case}\n"
            f"evidence_number\tEV-01\n"
            f"examiner_name\t{examiner}\n"
            f"description\tPhysical Memory Image (E01)\n"
            f"notes\tConverted via PhylaRAM\n"
            f"system_date\t2026-08-24 00:00:00\n"
            f"acquisition_date\t2026-08-24 00:00:00\n"
            f"compression_type\tbest\n"
        ).encode("utf-8")

        hdr_sec_size = 76 + len(meta_str) + 4
        hdr_sec = bytearray(76)
        hdr_sec[0:6] = b"header"
        struct.pack_into("<QQ", hdr_sec, 16, 13 + hdr_sec_size, hdr_sec_size)
        hdr_chk = adler32(hdr_sec[:72])
        struct.pack_into("<I", hdr_sec, 72, hdr_chk)

        out_f.write(hdr_sec)
        out_f.write(meta_str)
        out_f.write(struct.pack("<I", adler32(meta_str)))

        # Volume Section
        vol_data = bytearray(992)
        struct.pack_into("<IIIIQ", vol_data, 0, 1, 0, total_chunks, 128, total_physical_bytes // 512)
        vol_sec_size = 76 + 992 + 4
        vol_sec_offset = 13 + hdr_sec_size
        vol_sec = bytearray(76)
        vol_sec[0:6] = b"volume"
        struct.pack_into("<QQ", vol_sec, 16, vol_sec_offset + vol_sec_size, vol_sec_size)
        vol_chk = adler32(vol_sec[:72])
        struct.pack_into("<I", vol_sec, 72, vol_chk)

        out_f.write(vol_sec)
        out_f.write(vol_data)
        out_f.write(struct.pack("<I", adler32(vol_data)))

        # Sectors Section
        sectors_sec_offset = vol_sec_offset + vol_sec_size
        sectors_sec = bytearray(76)
        sectors_sec[0:7] = b"sectors"
        # will update NextOffset after writing chunks
        out_f.write(sectors_sec)

        chunk_offsets = []
        md5_hasher = hashlib.md5()
        sha256_hasher = hashlib.sha256()

        for r in ranges:
            start = int(r["start"], 16) if isinstance(r["start"], str) else int(r["start"])
            length = int(r["length"])
            in_f.seek(start)
            rem = length
            while rem > 0:
                read_sz = min(rem, chunk_size)
                buf = in_f.read(read_sz)
                if len(buf) < chunk_size:
                    buf = buf + b"\x00" * (chunk_size - len(buf))

                md5_hasher.update(buf)
                sha256_hasher.update(buf)

                rel_offset = out_f.tell() - sectors_sec_offset
                chunk_offsets.append(rel_offset)

                out_f.write(buf)
                out_f.write(struct.pack("<I", adler32(buf)))
                rem -= read_sz

        end_of_sectors = out_f.tell()
        sectors_sec_size = end_of_sectors - sectors_sec_offset

        # Rewrite sectors section header
        out_f.seek(sectors_sec_offset)
        struct.pack_into("<QQ", sectors_sec, 16, end_of_sectors, sectors_sec_size)
        sectors_chk = adler32(sectors_sec[:72])
        struct.pack_into("<I", sectors_sec, 72, sectors_chk)
        out_f.write(sectors_sec)
        out_f.seek(end_of_sectors)

        # Table Section
        tbl_data = bytearray(len(chunk_offsets) * 4)
        for idx, off in enumerate(chunk_offsets):
            struct.pack_into("<I", tbl_data, idx * 4, off)

        tbl_sec_size = 76 + len(tbl_data) + 4
        tbl_sec = bytearray(76)
        tbl_sec[0:5] = b"table"
        struct.pack_into("<QQ", tbl_sec, 16, end_of_sectors + tbl_sec_size, tbl_sec_size)
        tbl_chk = adler32(tbl_sec[:72])
        struct.pack_into("<I", tbl_sec, 72, tbl_chk)

        out_f.write(tbl_sec)
        out_f.write(tbl_data)
        out_f.write(struct.pack("<I", adler32(tbl_data)))

        # Hash Section
        hash_data = bytearray(36)
        hash_data[0:16] = md5_hasher.digest()
        hash_sec_size = 76 + 36
        hash_sec = bytearray(76)
        hash_sec[0:4] = b"hash"
        struct.pack_into("<QQ", hash_sec, 16, end_of_sectors + tbl_sec_size + hash_sec_size, hash_sec_size)
        hash_chk = adler32(hash_sec[:72])
        struct.pack_into("<I", hash_sec, 72, hash_chk)

        out_f.write(hash_sec)
        out_f.write(hash_data)

        # Done Section
        done_sec = bytearray(76)
        done_sec[0:4] = b"done"
        struct.pack_into("<QQ", done_sec, 16, 0, 76)
        done_chk = adler32(done_sec[:72])
        struct.pack_into("<I", done_sec, 72, done_chk)
        out_f.write(done_sec)

    print(f"[SUCCESS] E01 container created: {output_path} ({os.path.getsize(output_path):,} bytes)")

def main():
    parser = argparse.ArgumentParser(description="PhylaRAM Offline Evidence Format Converter")
    parser.add_argument("input_raw", help="Input .raw memory image")
    parser.add_argument("--to", choices=["zdmp", "dmp", "e01"], required=True, help="Target format")
    parser.add_argument("-o", "--output", help="Output file path (default: <input>.<format>)")
    parser.add_argument("--case", default="CASE-001", help="Case number (for E01)")
    parser.add_argument("--examiner", default="PhylaRAM Examiner", help="Examiner name (for E01)")

    args = parser.parse_args()

    if not os.path.isfile(args.input_raw):
        print(f"[ERROR] Input file not found: {args.input_raw}", file=sys.stderr)
        sys.exit(1)

    out_path = args.output
    if not out_path:
        base = args.input_raw.rsplit(".", 1)[0] if "." in args.input_raw else args.input_raw
        out_path = f"{base}.{args.to}"

    map_data = load_map(args.input_raw)

    if args.to in ("zdmp", "dmp"):
        convert_to_zdmp(args.input_raw, out_path, map_data)
    elif args.to == "e01":
        convert_to_e01(args.input_raw, out_path, map_data, case=args.case, examiner=args.examiner)

if __name__ == "__main__":
    main()
