# Security Policy

## Reporting Security Vulnerabilities

If you discover a security vulnerability in PhylaRAM (particularly in the kernel driver `phylaram.sys` or elevation mechanisms in `phylaram.exe`), please report it responsibly.

**Do NOT report security vulnerabilities through public GitHub issues.**

Please send security reports directly to:
- **Email:** [joey.victorino@gmail.com](mailto:joey.victorino@gmail.com)
- **Subject:** `[SECURITY] PhylaRAM Vulnerability Report`

Include:
1. Description of the vulnerability and its potential impact.
2. Exact steps, proof of concept (PoC), or environment details to reproduce.
3. Affected versions and platforms.

We will acknowledge receipt within 48 hours and work with you on a coordinated disclosure timeline.

---

## Security Model & Invariants

PhylaRAM is designed with defense-in-depth principles:
- **Administrator / SYSTEM Only:** The control device `\Device\PhylaRAM` enforces restrictive SDDL `D:P(A;;GA;;;SY)(A;;GA;;;BA)`. Non-elevated callers cannot open or communicate with the device.
- **Exclusive Acquisition:** Only one acquisition session may open the driver device at a time.
- **Prohibited Kernel Capabilities:** The driver does **not** expose arbitrary physical memory read/write or virtual address mapping. User mode may only request transfers by valid run index and run-relative offset.
- **Direct I/O Safety:** Output buffers are locked via MDLs (`METHOD_OUT_DIRECT`) and mapped with `MdlMappingNoExecute` at `PASSIVE_LEVEL`.
- **Atomic Operations:** Evidence is staged to `<output>.partial` and promoted atomically via `MoveFileExW` without replacement flags.
