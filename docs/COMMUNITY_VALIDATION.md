# Community Validation Guide

PhylaRAM needs more than unit tests and CI. It needs reproducible evidence from real Windows environments.

Community validation is therefore a first-class contribution path.

> **Current status:** PhylaRAM is alpha software. Use the test-signed build only in a dedicated lab or controlled test environment. Do not weaken Secure Boot, HVCI/Memory Integrity, or other security controls on a real evidence host merely to load the alpha driver.

## Highest-priority validation contributions

### 1. Driver Verifier stress

Run the documented Driver Verifier profile against `phylaram.sys` in a disposable Windows 10/11 test VM and execute repeated acquisitions.

Useful evidence includes:

- exact PhylaRAM commit SHA;
- Windows edition and build;
- VM platform and configuration;
- Driver Verifier settings;
- number of acquisition cycles;
- bugcheck status;
- pool/leak observations;
- relevant logs or screenshots that contain no sensitive data.

### 2. Physical hardware topology

We especially need real systems representing:

- 4 GB and 16 GB conventional systems;
- 64 GB systems with large PCIe MMIO / Resizable BAR apertures;
- 128 GB+ systems;
- NUMA / multi-socket systems;
- NTFS and ReFS destination volumes.

For each run, record physical RAM, CPU/platform, GPU/ReBAR state where relevant, filesystem, Windows build, commit SHA, acquisition result, verifier result, and resulting sparse allocation behavior.

### 3. Volatility 3 and MemProcFS interoperability

A useful interoperability report should start from a **real lab acquisition**, not a synthetic fixture.

Record:

- exact PhylaRAM commit SHA;
- Windows build that was acquired;
- SHA-256 of the resulting RAW image where disclosure is safe;
- `phylaram-verify` result;
- Volatility 3 version and commands tested;
- MemProcFS version and initialization result;
- any manual DTB/symbol intervention required;
- parsing failures or inconsistencies.

Do not publish sensitive memory images.

### 4. Kernel / evidence-model review

Reviewers can contribute without running a driver.

High-value review areas include:

- KMDF lifetime and synchronization;
- IOCTL validation and buffer semantics;
- frozen run-index trust boundary;
- partial-read convergence and byte accounting;
- `UNREADABLE != ZERO` semantics;
- evidence staging and publication;
- hostile-input behavior in `phylaram-verify`.

A strong review identifies the exact commit and records findings, even when the conclusion is that something is wrong.

## How to submit a validation result

Use the repository's **Validation Result** issue template from **New Issue**.

A useful report should contain enough information for another engineer to understand exactly what was tested and what was not.

Minimum fields:

```text
PhylaRAM commit SHA:
Validation category:
Windows edition/build:
Hardware or VM configuration:
Filesystem:
Commands executed:
Expected result:
Observed result:
phylaram-verify result:
Downstream tool versions/results:
Logs/artifacts:
Limitations:
```

## Evidence rules

External results are held to the same truthfulness standard as maintainer results.

- A model is not hardware validation.
- A successful compile is not runtime validation.
- A successful acquisition is not proof of downstream forensic interoperability.
- A test result is never presumed to pass.
- A failed test is valuable evidence and should not be hidden.
- Do not submit production evidence, credentials, PII, secrets, or proprietary memory contents.

## Recognition

With the contributor's permission, reproducible external validation may be referenced in [`VALIDATION_EVIDENCE.md`](VALIDATION_EVIDENCE.md) with attribution and the tested commit SHA.

Anonymous or organization-only attribution is acceptable when a contributor cannot be named publicly.

## Community lab access

Existing hardware, shared lab capacity, volunteered test time, engineering review, and repeatable external testing can close validation gaps without turning the project into a product or service.

The project benefits most from one thing:

> Independent people trying to prove the acquisition or verification model wrong, and publishing enough evidence for everyone else to evaluate the result.
