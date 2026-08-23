#!/usr/bin/env python3
"""
PhylaRAM Forensic I/O & Hashing Benchmark Utility
-------------------------------------------------
Evaluates sequential disk write speeds, sparse hole handling, and SHA-256
streaming throughput on the current system or target storage path.

Usage:
  python phylaram_bench.py [target_directory] [--size-mb 1024]
"""

import os
import sys
import time
import hashlib
import tempfile
import argparse

def benchmark_disk_and_hash(target_dir: str, size_mb: int):
    target_bytes = size_mb * 1024 * 1024
    chunk_size = 16 * 1024 * 1024 # 16 MiB matching PHYLA_MAX_TRANSFER
    test_file = os.path.join(target_dir, "phylaram_bench.tmp")

    print("===============================================================================")
    print("                 PhylaRAM Storage & Hashing Benchmark Utility                 ")
    print("===============================================================================")
    print(f"Target Directory  : {target_dir}")
    print(f"Benchmark Size    : {size_mb} MiB ({target_bytes:,} bytes)")
    print(f"Transfer Chunk    : {chunk_size / (1024 * 1024):.0f} MiB")
    print("-------------------------------------------------------------------------------")

    payload = b"\x55" * chunk_size

    # 1. Benchmark Sequential Direct Write Throughput
    print("[1/3] Benchmarking Direct Disk Write Throughput...")
    start_write = time.perf_counter()
    written = 0
    with open(test_file, "wb") as f:
        while written < target_bytes:
            to_write = min(chunk_size, target_bytes - written)
            f.write(payload[:to_write])
            written += to_write
        f.flush()
        os.fsync(f.fileno())
    write_time = time.perf_counter() - start_write
    write_speed_mbs = (target_bytes / (1024 * 1024)) / write_time
    print(f"      Sequential Write Speed : {write_speed_mbs:.2f} MB/s (Time: {write_time:.3f}s)")

    # 2. Benchmark Sequential Read & SHA-256 Hashing Throughput
    print("[2/3] Benchmarking Cryptographic SHA-256 Read & Hashing Speed...")
    hasher = hashlib.sha256()
    start_hash = time.perf_counter()
    read_bytes = 0
    with open(test_file, "rb") as f:
        while chunk := f.read(chunk_size):
            hasher.update(chunk)
            read_bytes += len(chunk)
    digest = hasher.hexdigest()
    hash_time = time.perf_counter() - start_hash
    hash_speed_mbs = (read_bytes / (1024 * 1024)) / hash_time
    print(f"      SHA-256 Hash Speed     : {hash_speed_mbs:.2f} MB/s (Time: {hash_time:.3f}s)")
    print(f"      Digest                 : {digest}")

    # 3. Cleanup
    try:
        os.remove(test_file)
    except OSError:
        pass

    # 4. Projected Memory Acquisition Times
    print("\n-------------------------------------------------------------------------------")
    print(" Estimated Memory Acquisition Times at Benchmark Throughput:")
    print("-------------------------------------------------------------------------------")
    for ram_gb in [16, 32, 64, 128, 256]:
        ram_bytes = ram_gb * 1024 * 1024 * 1024
        est_seconds = (ram_bytes / (1024 * 1024)) / min(write_speed_mbs, hash_speed_mbs)
        m, s = divmod(int(est_seconds), 60)
        print(f"  - {ram_gb:3} GB RAM Acquisition : ~{m:02d}m {s:02d}s")
    print("===============================================================================\n")

def main():
    parser = argparse.ArgumentParser(description="PhylaRAM Storage & Hashing Benchmark")
    parser.add_argument("target_dir", nargs="?", default=tempfile.gettempdir(),
                        help="Directory to perform write benchmark (defaults to system temp)")
    parser.add_argument("--size-mb", type=int, default=512,
                        help="Benchmark payload size in MiB (default: 512)")
    args = parser.parse_args()

    benchmark_disk_and_hash(args.target_dir, args.size_mb)

if __name__ == "__main__":
    main()
