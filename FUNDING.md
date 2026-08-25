# PhylaRAM Funding & Sustainability

PhylaRAM is and is intended to remain **free, open-source software under the MIT License**.

The project does not need a paid feature gate to be sustainable. What does cost money is the trust infrastructure around a Windows kernel and DFIR project: production driver signing, hardware validation, interoperability testing, CI/storage, and independent review.

## Funding principle

Funding supports **validation and maintenance**, not favorable outcomes.

A sponsor, donor, employer, vendor, or service customer does not buy:

- a passing validation result;
- suppression of a defect or limitation;
- priority over forensic correctness;
- private influence over the evidence model;
- an endorsement from the project;
- exclusive access to core PhylaRAM functionality.

If a sponsor-funded test fails, the failure is still a failure and should be recorded honestly.

## What needs funding

The current cost centers are deliberately narrow:

1. **Windows production driver signing**
   - EV code-signing certificate and associated identity requirements;
   - Microsoft Hardware Developer Program submission work;
   - Secure Boot, VBS, and HVCI validation once a production-signed path is available.

2. **Real hardware validation**
   - 4 GB through 128 GB+ systems;
   - PCIe MMIO / Resizable BAR layouts;
   - NUMA configurations;
   - NTFS and ReFS sparse-file behavior.

3. **Independent forensic interoperability**
   - real Windows 10/11 memory images;
   - Volatility 3 and MemProcFS validation;
   - repeatable evidence and logs tied to exact commit SHAs.

4. **Independent review and test infrastructure**
   - Windows lab capacity;
   - reproducible Driver Verifier runs;
   - archival storage for non-sensitive validation artifacts;
   - third-party engineering review where useful.

## Lean operating target

A practical initial planning target is **approximately USD $3,000-$5,000 per year** for signing, validation, infrastructure, and small hardware expenses.

That is a planning range, not a vendor quote. Actual costs should be recorded when incurred rather than estimated as facts.

The project should prefer borrowed lab capacity, donated test time, existing hardware, and reproducible community validation before buying new equipment.

## The highest-value contribution may not be money

In-kind validation can be more valuable than cash.

Examples:

- run the Driver Verifier stress protocol on a dedicated Windows test VM;
- test PhylaRAM on a 64 GB ReBAR workstation;
- test a 128 GB+ or NUMA system;
- acquire a lab image and validate it with Volatility 3 or MemProcFS;
- review the KMDF trust boundary, acquisition semantics, or evidence publication model;
- provide temporary access to suitable test hardware or a Windows driver lab.

See [`docs/COMMUNITY_VALIDATION.md`](docs/COMMUNITY_VALIDATION.md) for the evidence required for a useful external validation result.

## Commercial services and the open-source boundary

PhylaRAM itself remains MIT-licensed and freely usable.

Separate commercial services may exist around the project, including deployment assistance, integration engineering, training, enterprise support, readiness assessment, or environment-specific validation. Victorino LLC may provide such services independently.

Payment for a service does not change the license, create a private edition of the core project, or alter validation findings.

## Sponsorship model

If project sponsorship is enabled, the preferred structure is intentionally simple:

- **Community support:** recurring or one-time contributions toward routine project costs;
- **Supporting organization:** ongoing support for open-source maintenance and validation;
- **Validation sponsor:** funding for a clearly defined signing, hardware, interoperability, or review milestone.

Any sponsored validation should identify the sponsor, scope, commit SHA, methodology, result, and limitations.

The sponsor funds the test. The sponsor does not control the result.

## Microsoft and other affiliations

Any maintainer biography, former employment, professional network, or alumni status is biographical context only.

PhylaRAM is an independent open-source project. Nothing in this repository should imply Microsoft sponsorship, certification, approval, or endorsement unless an exact Microsoft-issued artifact supports that specific claim.

## Transparency

Material project funding used for validation should be tied, where practical, to a public milestone or validation record.

The goal is simple:

> Keep the code free. Fund the work required to prove that the code deserves trust.
