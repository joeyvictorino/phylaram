## Description

Provide a clear and concise description of the changes proposed in this pull request.

---

## Forensic & Engineering Checklist

Please verify that your pull request complies with [`ENGINEERING_STANDARD.md`](ENGINEERING_STANDARD.md):

- [ ] **Engineering Policy:** `python3 scripts/engineering_policy_check.py` passes with zero violations.
- [ ] **Compiler Cleanliness:** C++ code compiles with `/W4 /WX` (MSVC) and `-Wall -Wextra -Werror` (Clang).
- [ ] **Kernel Driver Invariants:** Preserves `WdfSynchronizationScopeDevice`, `MmCopyMemory` bounds checking, and `D:P(A;;GA;;;SY)(A;;GA;;;BA)` SDDL.
- [ ] **Forensic Integrity:** No heuristic classifications or data transforms added to the acquisition core.
- [ ] **Transaction Consistency:** CLI and GUI invoke the canonical `CaptureEvidenceToFile` transaction.
- [ ] **Rust Verifier:** `cargo fmt --check`, `cargo clippy -- -D warnings`, and `cargo test` pass in `tools/phylaram-verify`.
- [ ] **Testing:** Unit tests, model fixtures, or hardware validation results are included.

---

## Related Issues

Fixes / Closes #(issue number)
