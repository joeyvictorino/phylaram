# Production Driver Signing, Secure Boot, and HVCI

**Status:** Release-gate guidance for PhylaRAM alpha  
**Authoritative external source:** Microsoft Windows Hardware documentation

This guide intentionally distinguishes Microsoft **attestation signing** from **Windows Hardware Compatibility Program (WHCP) / HLK certification**. They are different submission paths and must not be described as “WHQL attestation signing.”

As of Microsoft guidance updated in April 2026, a Hardware Dev Center account used for either attestation signing or WHCP certification must have an associated valid EV code-signing certificate. Microsoft also documents attestation signing and HLK/WHCP certification as separate workflows.

Authoritative references:

- <https://learn.microsoft.com/windows-hardware/drivers/dashboard/code-signing-reqs>
- <https://learn.microsoft.com/windows-hardware/drivers/dashboard/code-signing-attestation>
- <https://learn.microsoft.com/windows-hardware/test/hlk/>
- <https://learn.microsoft.com/windows-hardware/drivers/dashboard/code-signing-validate>

---

## 1. Release Requirement

A production PhylaRAM release must use a Microsoft-signed kernel driver that has been validated on the supported Windows security configuration.

The release gate requires:

- Secure Boot enabled;
- VBS enabled where applicable;
- HVCI / Memory Integrity enabled;
- normal Defender/code-integrity policy active;
- no test-signing mode;
- no requirement to weaken security controls to load the driver;
- clean acquisition and cleanup behavior under those controls.

Source code being designed with HVCI-compatible practices is not proof that this gate passes.

---

## 2. Choose the Signing Path Deliberately

### Attestation signing

Microsoft documents attestation signing as a Partner Center submission path based on a signed CAB containing the driver package. Current Microsoft guidance notes limitations for attestation-signed drivers targeting retail audiences and Windows Update publication.

Use attestation only if its current Microsoft eligibility and distribution semantics match the release goal.

### WHCP / HLK certification

WHCP qualification requires the appropriate Windows Hardware Lab Kit tests and a signed HLK submission package. This is the certification path associated with Windows hardware compatibility qualification.

Use WHCP when its certification/distribution guarantees are required.

### Rule

The release owner must record which path was selected, why, what Microsoft documentation governed the decision, and the exact returned signed artifacts validated by the release tests.

---

## 3. Preparation Script

`scripts/prepare_driver_submission.ps1` prepares an attestation-style CAB from the current built driver package. It is a packaging aid, not proof of Microsoft approval or successful signing.

The script must fail if required tooling or package files are missing. A warning followed by a “ready” message is not acceptable for a release artifact.

Example:

```powershell
pwsh -File scripts\prepare_driver_submission.ps1 -CertificateThumbprint <thumbprint>
```

The EV/private-key operation must follow the certificate provider's secure process. Do not export or place private signing material in this repository or CI artifacts.

---

## 4. Validate Returned Microsoft Signatures

After Partner Center returns a signed package:

1. preserve the submission/result identifiers in the release record;
2. verify the returned package corresponds to the exact release driver build;
3. validate the Microsoft signature using Microsoft's documented validation process;
4. install only the returned production-signed package on the security-control test systems;
5. run the Secure Boot/VBS/HVCI acquisition gate;
6. retain test evidence tied to the exact release commit and driver hash.

A successful Partner Center submission does not by itself prove runtime correctness.

---

## 5. HVCI Engineering Checklist

Code review must examine:

- no runtime code modification or hooking;
- no PTE-remapping acquisition mechanism;
- no arbitrary kernel-memory interface;
- compatible executable/non-executable memory behavior;
- supported WDK/DDI usage;
- CFG/Spectre and other configured compiler hardening;
- correct synchronization and IRQL behavior;
- clean Driver Verifier results;
- secure device SDDL and exclusive access;
- no production dependency on test-signing or disabled security controls.

Each item is an engineering requirement to validate, not a pre-declared “pass.”

---

## 6. Test-Signed Alpha Builds

CI may create self-signed artifacts for controlled test environments. Those artifacts are not production driver-signing evidence.

Do not disable Secure Boot, HVCI, or other security controls on a real evidence host merely to load an alpha artifact. Use a disposable dedicated test VM or other explicitly designated lab system for test-signing scenarios.
