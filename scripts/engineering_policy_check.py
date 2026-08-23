#!/usr/bin/env python3
"""Small repository-policy checks for invariants cheap enough to automate.

This is intentionally not a substitute for code review, static analysis, or
runtime validation. It prevents accidental reintroduction of a few architectural
defects whose absence can be checked directly from the source tree.
"""

from __future__ import annotations

from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def require(condition: bool, message: str, failures: list[str]) -> None:
    if not condition:
        failures.append(message)


def main() -> int:
    failures: list[str] = []

    require(
        (ROOT / "ENGINEERING_STANDARD.md").is_file(),
        "ENGINEERING_STANDARD.md is missing",
        failures,
    )
    require(
        "ENGINEERING_STANDARD.md" in read("CONTRIBUTING.md"),
        "CONTRIBUTING.md does not make ENGINEERING_STANDARD.md normative",
        failures,
    )

    driver = read("driver/driver.c")
    require(
        "WdfSynchronizationScopeDevice" in driver,
        "kernel file-context lifetime is no longer protected by device synchronization",
        failures,
    )

    driver_resource = read("cli/driver_resource.cpp")
    require(
        "sidecarPath" not in driver_resource,
        "adjacent driver sidecar override was reintroduced",
        failures,
    )
    require(
        "FindResourceW" in driver_resource,
        "embedded driver resource is no longer the implicit kernel-code source",
        failures,
    )

    main_cpp = read("cli/main.cpp")
    gui_cpp = read("cli/gui.cpp")
    require(
        "CaptureEvidenceToFile" in main_cpp,
        "CLI no longer uses the canonical evidence transaction",
        failures,
    )
    require(
        "CaptureEvidenceToFile" in gui_cpp,
        "GUI no longer uses the canonical evidence transaction",
        failures,
    )
    require(
        "--no-hash" not in main_cpp,
        "hash-free finalized capture option was reintroduced",
        failures,
    )

    acquisition_sources = "\n".join(
        read(path)
        for path in (
            "cli/acquire.cpp",
            "cli/map.cpp",
            "shared/interfaces.hpp",
        )
    )
    for forbidden in (
        "AnalyzeWaveletEntropy",
        "wavelet_entropy",
        "compliance_standards",
        "COMPLIANCE_REGISTRY",
    ):
        require(
            forbidden not in acquisition_sources,
            f"interpretive field/mechanism '{forbidden}' leaked into canonical acquisition provenance",
            failures,
        )

    require(
        not (ROOT / "shared/wavelet_classifier.hpp").exists(),
        "obsolete wavelet classifier remains in the acquisition core",
        failures,
    )
    require(
        not (ROOT / "shared/compliance_map.hpp").exists(),
        "obsolete compliance mapping remains in the acquisition core",
        failures,
    )

    transaction = read("cli/evidence_transaction.cpp")
    for required in (
        "FlushAndClose",
        "WriteMapJson",
        "WriteSha256Sidecar",
        "PromoteStagingFile",
        "EvidenceCaptureStatus::Incomplete",
    ):
        require(
            required in transaction,
            f"canonical evidence transaction is missing required phase '{required}'",
            failures,
        )

    verifier = read("tools/phylaram-verify/src/verifier.rs")
    for required in (
        "checked_add",
        "UnreadableOutsidePhysicalRun",
        "UnreadableOverlap",
        "UnreadableRepresentationMismatch",
        "HighestPhysicalEndMismatch",
    ):
        require(
            required in verifier,
            f"offline verifier lost required invariant '{required}'",
            failures,
        )

    require(
        not (ROOT / "docs/WHQL_AND_HVCI_GUIDE.md").exists(),
        "obsolete WHQL/attestation-conflating guide was reintroduced",
        failures,
    )
    require(
        not (ROOT / "scripts/prepare_whql_submission.ps1").exists(),
        "obsolete WHQL/attestation-conflating script was reintroduced",
        failures,
    )
    require(
        (ROOT / "docs/DRIVER_SIGNING_AND_HVCI.md").is_file(),
        "driver signing/HVCI guidance is missing",
        failures,
    )
    require(
        (ROOT / "scripts/prepare_driver_submission.ps1").is_file(),
        "driver submission preparation script is missing",
        failures,
    )

    if failures:
        print("Engineering policy check FAILED:", file=sys.stderr)
        for failure in failures:
            print(f"  - {failure}", file=sys.stderr)
        return 1

    print("Engineering policy check passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
