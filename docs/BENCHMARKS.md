# PhylaRAM Performance & Benchmark Methodology

> **Normative Standard:** [`ENGINEERING_STANDARD.md`](../ENGINEERING_STANDARD.md)  
> **Status:** Methodology defined. Actual physical bare-metal hardware benchmark data will be recorded in this document as physical lab gates run.

PhylaRAM is designed for high-throughput, low-impact live physical memory acquisition. In keeping with the project's strict forensic integrity standard, this document defines the exact measurement methodology and metrics. **No benchmark numbers or competitive performance claims are published without documented physical test evidence.**

---

## 1. Benchmark Metrics & Instrumentation

When benchmarking acquisition performance on physical hardware or test virtual machines, the following 10 metrics are measured and recorded:

| Metric | Unit | Measurement Method |
| :--- | :--- | :--- |
| **Physical RAM Size** | GiB / Bytes | Queried via `MmGetPhysicalMemoryRangesEx2` sum of populated ranges. |
| **Logical RAW Size** | GiB / Bytes | High-water mark of highest populated physical memory end address. |
| **Actual Disk Allocation** | GiB / Bytes | Measured on NTFS/ReFS via `GetCompressedFileSizeW` to verify sparse hole allocation. |
| **Elapsed Acquisition Time** | Seconds | High-resolution wall-clock time from driver session start to evidence finalization. |
| **Effective Throughput** | MiB/s | `(Total Physical Bytes Acquired) / (Elapsed Seconds * 1024 * 1024)`. |
| **Peak Working Set** | MiB | Measured via `GetProcessMemoryInfo` (`PeakWorkingSetSize`) for `phylaram.exe`. |
| **CPU Utilization** | % Core | Measured across user mode (SHA-256 computation) and kernel mode (`MmCopyMemory`). |
| **SHA-256 Pipeline Overhead** | Milliseconds | Delta between pure I/O throughput and dual-pipeline hash computation. |
| **Rate-Limiter Accuracy** | % Variance | Delta between configured `--rate-limit <MB/s>` and actual observed acquisition rate. |
| **Unreadable Page Count** | Pages / Bytes | Isolated 4 KiB unreadable hardware pages recorded in `memory.raw.map.json`. |

---

## 2. Benchmark Execution Procedure

To execute a benchmark run on a test system:

### 1. Unlimited Throughput Benchmark (Fast Path)
```powershell
$start = [System.Diagnostics.Stopwatch]::StartNew()
.\phylaram.exe C:\benchmarks\unlimited_mem.raw
$start.Stop()
Write-Host "Elapsed: $($start.Elapsed.TotalSeconds) seconds"
.\phylaram-verify.exe C:\benchmarks\unlimited_mem.raw C:\benchmarks\unlimited_mem.raw.map.json C:\benchmarks\unlimited_mem.raw.sha256
```

### 2. Rate-Limited Pacing Accuracy Benchmark (e.g. 100 MiB/s)
```powershell
$start = [System.Diagnostics.Stopwatch]::StartNew()
.\phylaram.exe C:\benchmarks\paced_100_mem.raw --rate-limit 100
$start.Stop()
Write-Host "Elapsed: $($start.Elapsed.TotalSeconds) seconds"
.\phylaram-verify.exe C:\benchmarks\paced_100_mem.raw C:\benchmarks\paced_100_mem.raw.map.json C:\benchmarks\paced_100_mem.raw.sha256
```

### 3. Sparse File Disk Allocation Verification
```powershell
$logicalSize = (Get-Item C:\benchmarks\unlimited_mem.raw).Length
$allocatedSize = (Get-Item C:\benchmarks\unlimited_mem.raw).Target.Length # or fsutil file layout
Write-Host "Logical Size: $logicalSize bytes | Allocated on Disk: $allocatedSize bytes"
```

---

## 3. Physical Hardware Benchmark Results Table

| Run ID | Execution Date | Git Commit | OS Build | Physical RAM | CPU & Topology | Storage Target | Effective Rate | Disk Allocation | Status |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| `BM-001` | *Pending* | `b588deb` | Windows 11 23H2 | 16 GB DDR5 | AMD Ryzen 7 (1P/8C) | NVMe (NTFS) | *Pending Lab Run* | *Pending* | ⏳ Open |
| `BM-002` | *Pending* | `b588deb` | Windows 11 23H2 | 64 GB DDR4 | Intel i9 (ReBAR Active) | NVMe (NTFS) | *Pending Lab Run* | *Pending* | ⏳ Open |
| `BM-003` | *Pending* | `b588deb` | Windows Server 2022 | 128 GB DDR4 | 2-Socket NUMA (2P/32C) | SAS SSD (ReFS) | *Pending Lab Run* | *Pending* | ⏳ Open |

---

## 4. Competitive Comparison Policy

PhylaRAM does not publish comparative benchmarks against third-party tools (such as WinPmem, DumpIt, Magnet RAM Capture, or Belkasoft Live RAM Capturer) unless:
1. All tools are run on identical physical hardware under the exact same OS build and background workload;
2. Command-line invocations, storage targets, and cryptographic hashing options are identical and documented;
3. Raw data, log outputs, and provenance maps are preserved and verifiable.
