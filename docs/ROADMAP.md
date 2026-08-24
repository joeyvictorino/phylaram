# PhylaRAM Project Roadmap

> **Current Version:** `0.1.0-alpha` (Test-Signed Pre-Release)  
> **Normative Standard:** [`ENGINEERING_STANDARD.md`](../ENGINEERING_STANDARD.md)  
> **Status Matrix:** [`docs/STATUS.md`](STATUS.md)

PhylaRAM's development follows a strict, verification-gated path from Alpha to Production Release.

---

## Phase 1: Core Integrity & Alpha Pre-Release (Current Milestone) ✅

- [x] KMDF kernel driver with `WdfSynchronizationScopeDevice` and `MmCopyMemory` physical acquisition.
- [x] Frozen run-index read protocol preventing arbitrary physical address submission from user mode.
- [x] Canonical provenance map schema (`phylaram-map-2`) distinguishing unreadable memory from observed zeros.
- [x] Independent Rust verifier (`phylaram-verify`) with property tests and mathematical invariants.
- [x] Minimalist, high-DPI Win32 desktop GUI with live telemetry inspector.
- [x] Unified single evidence capture transaction (`CaptureEvidenceToFile`) shared by CLI and GUI.
- [x] Automated CI builds on GitHub Actions with WDK NuGet toolset, test-signing, and CodeQL security analysis.

---

## Phase 2: Production Hardware & Security Validation Gates (In Progress) ⏳

- [ ] **Gate 2: Static Driver Verifier (SDV)**
  - Execute full WDK Static Driver Verifier rule set on dedicated WDK build host.
- [ ] **Gate 3: Driver Verifier Dynamic Stress (100 Cycles)**
  - Run 100 consecutive acquisition cycles on Windows 10/11 test VMs under Special Pool, Force IRQL, and Deadlock Detection.
- [ ] **Gate 4: Physical Hardware & Topology Matrix**
  - Bare-metal validation on 4 GB, 16 GB, 64 GB (ReBAR active), and 128 GB+ (2-socket NUMA) hardware configurations.
  - Verification of NTFS and ReFS sparse file extent allocation.
- [ ] **Gate 5: Production Microsoft Driver Signing & HVCI Hardening**
  - Obtain EV code signing certificate and submit `phylaram.sys` to Microsoft Hardware Dev Center for Attestation Signing.
  - Verify execution on Windows 11 Enterprise with Secure Boot, VBS, and HVCI (Memory Integrity) enforced.
- [ ] **Gate 6: Real-World Acquired RAM in Volatility 3 & MemProcFS**
  - Acquire physical RAM images from diverse Windows 10/11 builds and verify analysis with `windows.pslist`, `windows.info`, and MemProcFS VMM initialization.

---

## Phase 3: Post-1.0 Production & Downstream Tooling (Future) 🔮

- [ ] **Offline Format Converters:** Independent offline tool to convert verified flat RAW images + map into AFF4 or Microsoft Crash Dump (`.dmp`) without modifying the primary evidence acquisition core.
- [ ] **Remote Evidence Streamer:** Authenticated TLS evidence streamer with companion map streaming for live enterprise incident response.
- [ ] **Linux / macOS Kernel Modules:** Extend PhylaRAM provenance semantics and `phylaram-map-2` schema to Linux LiME-compatible and macOS memory acquisition.
