# PhylaRAM Release Verification Checklist

> **Normative Reference:** [`ENGINEERING_STANDARD.md`](../ENGINEERING_STANDARD.md)  
> **Status Matrix:** [`docs/STATUS.md`](STATUS.md)

This checklist governs the release process for PhylaRAM. No artifact may be published as a release without satisfying every criterion in the relevant release tier.

---

## Tier 1: Public Alpha Pre-Release Checklist (Test-Signed)

*Applicable to pre-release test builds (e.g. `v0.1.0-alpha.N`).*

- [ ] **Git HEAD Audit:**
  - Working directory is clean (`git status` reports no untracked or uncommitted changes).
  - Target release commit is pushed to `main` on GitHub.
- [ ] **Automated CI Validation:**
  - `Portable Policy, C++20, Rust & Model Tests` passed 100% green on `macos-latest`.
  - `Windows MSVC & KMDF Build` passed 100% green on `windows-2022`.
  - `CodeQL Security Analysis` passed with zero security alerts.
- [ ] **Compiler & Code Quality Baseline:**
  - C++ source builds with `/W4 /WX /guard:cf /Qspectre` on MSVC and `-Wall -Wextra -Werror` on Clang.
  - Rust verifier passes `cargo fmt --check`, `cargo clippy --all-targets -- -D warnings`, and `cargo test`.
  - Python policy check (`scripts/engineering_policy_check.py`) passes with zero violations.
- [ ] **Artifact Assembly & Test Signing:**
  - `phylaram.sys`, `phylaram.exe`, and `phylaram-verify.exe` are test-signed with CI certificate.
  - Test-signing is verified via `signtool verify /pa /v`.
  - Release archive `PhylaRAM-<version>-x64-TEST-SIGNED.zip` and `.sha256` digest are generated.
  - Bundle contains `README-FIRST.txt`, `README.txt`, `LICENSE.txt`, `PhylaRAM_TestCert.cer`, and `install_test_cert.bat`.
- [ ] **Prominent Test-Signing Warnings:**
  - Pre-release notes prominently state: **TEST-SIGNED ALPHA — LAB/TEST VMS ONLY**.
  - Warning clearly instructs users **not to disable Secure Boot or HVCI on a real evidence host**.
  - Known open validation gates are explicitly listed.

---

## Tier 2: Production 1.0 Release Checklist (Production-Signed)

*Mandatory prerequisites before PhylaRAM can be tagged as a production release.*

- [ ] **Gate 2: Static Driver Verifier (SDV):**
  - Full WDK SDV rule suite passes on `driver/phylaram.vcxproj` with zero defects.
- [ ] **Gate 3: Driver Verifier Runtime Stress:**
  - 100 consecutive acquisition cycles under full memory load with Special Pool, Force IRQL, and Deadlock Detection enabled. Zero bugchecks, zero memory leaks, zero pool corruption.
- [ ] **Gate 4: Physical Hardware Matrix:**
  - Physical memory acquisitions verified on bare-metal systems:
    - 4 GB low-memory configuration
    - 16 GB standard workstation
    - 64 GB gaming/workstation with active Resizable BAR (ReBAR) GPU aperture
    - 128 GB+ enterprise dual-socket NUMA server
  - Sparse NTFS and ReFS extent allocation verified via `GetCompressedFileSizeW`.
- [ ] **Gate 5: Microsoft Production Driver Signing & HVCI:**
  - Extended Validation (EV) code signing certificate obtained.
  - Driver submitted to Microsoft Hardware Dev Center and received valid Microsoft Attestation / WHCP signature.
  - Driver verified to load and acquire RAM seamlessly on Windows 11 Enterprise with **Secure Boot**, **Virtualization-Based Security (VBS)**, and **Memory Integrity (HVCI)** fully enforced without enabling test-signing mode.
- [ ] **Gate 6: Real-World Forensic Tool Interoperability:**
  - Real acquired Windows 10/11 physical memory images analyzed and verified with Volatility 3 (`windows.pslist`, `windows.info`, `windows.malfind`) and MemProcFS.
- [ ] **Documentation & Zero Overclaiming:**
  - All claims in `README.md`, `docs/STATUS.md`, and `docs/VALIDATION_EVIDENCE.md` match actual recorded hardware evidence.
