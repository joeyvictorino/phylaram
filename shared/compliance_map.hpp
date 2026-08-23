#pragma once

#include <string>
#include <vector>

namespace phylaram {

// Security Standards Compliance Mapping Entry
// Authoritative compile-time stable identifiers for SIEM and DFIR compliance integration.
struct ComplianceMapping {
    const char* capabilityKey;      // Internal capability or event type
    const char* description;        // Forensic capability description
    const char* mitreAttack;        // Space-separated MITRE ATT&CK (Enterprise) technique IDs
    const char* nistSp80053;        // Space-separated NIST SP 800-53 Rev 5 controls
    const char* nistCsf;            // Space-separated NIST Cybersecurity Framework v1.1 subcategories
};

// Static air-gap safe compliance mapping registry
inline const ComplianceMapping COMPLIANCE_REGISTRY[] = {
    {
        "PHYSICAL_MEMORY_ACQUISITION",
        "Raw physical RAM capture via non-destructive kernel MDL mapping",
        "T1005 T1003 T1562.001",
        "AU-2 SI-4 SI-7 SI-16 AC-3",
        "DE.AE-2 DE.CM-4 RS.AN-1 PR.DS-1"
    },
    {
        "KERNEL_HINTS_DISCOVERY",
        "Live Ring 0 System DTB (CR3), KPCR, and NT Kernel base discovery",
        "T1082 T1057 T1012",
        "SI-4 SI-7 SI-16 AU-12",
        "DE.AE-2 DE.CM-4 ID.AM-1"
    },
    {
        "MEMORY_TOPOLOGY_ENUMERATION",
        "MmGetPhysicalMemoryRangesEx2 hardware run and MMIO aperture mapping",
        "T1082 T1614",
        "SI-4 SI-16 CM-8",
        "ID.AM-1 DE.AE-2"
    },
    {
        "EVIDENCE_PROVENANCE_MAPPING",
        "Deterministic physical address to file offset mapping with per-page error telemetry",
        "T1070.004 T1005",
        "AU-2 AU-9 AU-10 AU-12 SI-7",
        "PR.DS-1 PR.DS-5 RS.AN-1"
    },
    {
        "CRYPTOGRAPHIC_INTEGRITY_HASH",
        "Zero-gap continuous logical SHA-256 evidence digest",
        "T1565.001 T1005",
        "SI-7 AU-9 AU-10 SC-13",
        "PR.DS-5 RS.AN-1"
    },
    {
        "WAVELET_ENTROPY_CLASSIFICATION",
        "5-state quaternionic Haar wavelet transition lattice triage and anomaly detection",
        "T1055 T1027 T1203 T1620",
        "SI-4 SI-7 SI-16 DE-1",
        "DE.AE-2 DE.CM-4 RS.AN-1"
    }
};

inline size_t GetComplianceRegistryCount() {
    return sizeof(COMPLIANCE_REGISTRY) / sizeof(COMPLIANCE_REGISTRY[0]);
}

} // namespace phylaram
