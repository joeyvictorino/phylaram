# Repository Security & Branch Protection Policy

> **Normative Reference:** [`ENGINEERING_STANDARD.md`](../ENGINEERING_STANDARD.md)  
> **Security Policy:** [`SECURITY.md`](../SECURITY.md)

Because PhylaRAM contains a Windows kernel-mode driver (`phylaram.sys`) and security-critical forensic acquisition software, the repository enforces and recommends the following security baseline:

---

## 1. Branch Protection & Merging Rules (`main`)

The `main` branch is the canonical, verified production branch. The recommended repository settings are:

1. **Require Status Checks to Pass Before Merging:**
   - `Portable Policy, C++20, Rust & Model Tests` (`macos-latest` or Linux)
   - `Windows MSVC & KMDF Build` (`windows-2022`)
   - `CodeQL Security Analysis` (`security-and-quality` query suite)
2. **Require Branches to Be Up to Date Before Merging:**
   - Fast-forward or linear squash merge to maintain a clear audit trail.
3. **Disallow Force Pushing:**
   - Force-pushes (`git push --force`) to `main` are strictly prohibited.
4. **Disallow Branch Deletion:**
   - Deletion of the `main` branch is blocked.
5. **Automatically Delete Head Branches:**
   - Ephemeral remediation and feature branches must be deleted upon PR merge to maintain clean repository hygiene.
6. **Require Signed Commits & Tags (Recommended):**
   - Release tags (`v*`) and merge commits should be GPG/SSH signed by maintainers.

---

## 2. Supply Chain & Toolchain Integrity

1. **Pinned Dependency Versions:**
   - WDK packages pinned in `packages.config` (`Microsoft.Windows.WDK.x64` version `10.0.26100.1`).
   - Rust dependencies pinned in `tools/phylaram-verify/Cargo.lock`.
2. **Authenticode Verification in CI:**
   - Every compiled driver and executable is test-signed in CI and verified using `signtool verify /pa /v` before artifact assembly.
3. **Zero Untracked Driver Overrides:**
   - The user-mode engine extracts its embedded driver from internal PE resources rather than trusting unauthenticated files on disk.

---

## 3. Coordinated Vulnerability Disclosure

- **Private Reporting:** Maintainers enable GitHub Private Vulnerability Reporting to permit confidential submission of security advisories.
- **Triage SLA:** Initial acknowledgment within 48 hours; assessment within 5 business days.
- **Public Disclosure:** Public release notes and CVE assignments occur strictly in coordination with the reporter after remediation is validated on `main`.
