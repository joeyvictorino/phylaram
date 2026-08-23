#include "../shared/interfaces.hpp"
#include <cassert>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <string>

static std::string Hex64(uint64_t value)
{
    std::ostringstream ss;
    ss << "0x" << std::hex << std::uppercase << value;
    return ss.str();
}

static std::string Hex32(uint32_t value)
{
    std::ostringstream ss;
    ss << "0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << value;
    return ss.str();
}

static std::string SerializeMapJson(const AcquisitionSummary& s)
{
    std::ostringstream f;
    const char* status = s.completed ?
        ((s.unreadableBytes != 0 || s.topologyChanged) ? "incomplete" : "complete") : "failed";

    f << "{\n";
    f << "  \"producer\": \"PhylaRAM\",\n";
    f << "  \"producer_version\": \"0.1.0-alpha\",\n";
    f << "  \"schema\": \"phylaram-map-2\",\n";
    f << "  \"status\": \"" << status << "\",\n";
    f << "  \"logical_size\": " << s.logicalSize << ",\n";
    f << "  \"physical_bytes\": " << s.physicalBytes << ",\n";
    f << "  \"acquired_bytes\": " << s.acquiredBytes << ",\n";
    f << "  \"unreadable_bytes\": " << s.unreadableBytes << ",\n";
    f << "  \"topology_changed\": " << (s.topologyChanged ? "true" : "false") << ",\n";
    f << "  \"sha256\": \"" << s.sha256 << "\",\n";

    if (s.hints.available) {
        f << "  \"kernel_hints\": {\n";
        f << "    \"hypervisor_present\": " << (s.hints.hypervisorPresent ? "true" : "false") << ",\n";
        f << "    \"directory_table_base\": \"" << Hex64(s.hints.directoryTableBase) << "\",\n";
        f << "    \"kpcr_address\": \"" << Hex64(s.hints.kpcrAddress) << "\",\n";
        f << "    \"kernel_base\": \"" << Hex64(s.hints.kernelBase) << "\",\n";
        f << "    \"kernel_size\": " << s.hints.kernelSize << ",\n";
        f << "    \"major_version\": " << s.hints.majorVersion << ",\n";
        f << "    \"minor_version\": " << s.hints.minorVersion << ",\n";
        f << "    \"build_number\": " << s.hints.buildNumber << ",\n";
        f << "    \"processors\": " << s.hints.numberOfProcessors << "\n";
        f << "  },\n";
    }

    if (s.entropy.totalBytesAnalyzed > 0) {
        f << "  \"wavelet_entropy\": {\n";
        f << "    \"identity_density\": " << s.entropy.identityDensity << ",\n";
        f << "    \"transition_energy\": " << s.entropy.transitionEnergy << ",\n";
        f << "    \"bigram_entropy\": " << s.entropy.bigramEntropy << ",\n";
        f << "    \"prediction_confidence\": " << s.entropy.predictionConfidence << ",\n";
        f << "    \"orbit_hash\": \"" << Hex64(s.entropy.orbitHash) << "\",\n";
        f << "    \"category\": \"" << s.entropy.categoryName << "\"\n";
        f << "  },\n";
    }

    f << "  \"compliance_standards\": {\n";
    f << "    \"frameworks\": [\"MITRE ATT&CK (Enterprise)\", \"NIST SP 800-53 Rev 5\", \"NIST CSF v1.1\"],\n";
    f << "    \"mappings\": [\n";
    for (size_t i = 0; i < phylaram::GetComplianceRegistryCount(); ++i) {
        const auto& c = phylaram::COMPLIANCE_REGISTRY[i];
        f << "      {\"capability\": \"" << c.capabilityKey
          << "\", \"description\": \"" << c.description
          << "\", \"mitre_attack\": \"" << c.mitreAttack
          << "\", \"nist_sp_800_53\": \"" << c.nistSp80053
          << "\", \"nist_csf\": \"" << c.nistCsf << "\"}";
        if (i + 1 != phylaram::GetComplianceRegistryCount()) f << ',';
        f << "\n";
    }
    f << "    ]\n";
    f << "  },\n";

    f << "  \"ranges\": [\n";
    for (size_t i = 0; i < s.ranges.size(); ++i) {
        const auto& r = s.ranges[i];
        f << "    {\"driver_run\": " << r.driverIndex
          << ", \"start\": \"" << Hex64(r.base)
          << "\", \"length\": " << r.length << "}";
        if (i + 1 != s.ranges.size()) f << ',';
        f << "\n";
    }
    f << "  ],\n";

    f << "  \"unreadable\": [\n";
    for (size_t i = 0; i < s.unreadable.size(); ++i) {
        const auto& u = s.unreadable[i];
        f << "    {\"start\": \"" << Hex64(u.start)
          << "\", \"length\": " << u.length
          << ", \"ntstatus\": \"" << Hex32(static_cast<uint32_t>(u.status)) << "\"}";
        if (i + 1 != s.unreadable.size()) f << ',';
        f << "\n";
    }
    f << "  ]\n";
    f << "}\n";

    return f.str();
}

int main() {
    // Test 1: Complete acquisition JSON with kernel hints and wavelet entropy
    {
        AcquisitionSummary s;
        s.completed = true;
        s.topologyChanged = false;
        s.logicalSize = 17179869184ULL;
        s.physicalBytes = 16909336576ULL;
        s.acquiredBytes = 16909336576ULL;
        s.unreadableBytes = 0;
        s.sha256 = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
        s.hints.available = true;
        s.hints.hypervisorPresent = true;
        s.hints.directoryTableBase = 0x1AA000;
        s.hints.kpcrAddress = 0xFFFFF80123450000ULL;
        s.hints.kernelBase = 0xFFFFF80112340000ULL;
        s.hints.kernelSize = 12582912;
        s.hints.buildNumber = 22631;
        s.hints.numberOfProcessors = 16;
        s.entropy.totalBytesAnalyzed = 65536;
        s.entropy.identityDensity = 0.52f;
        s.entropy.transitionEnergy = 0.72f;
        s.entropy.bigramEntropy = 2.85f;
        s.entropy.predictionConfidence = 0.71f;
        s.entropy.orbitHash = 0x123456789ABCDEF0ULL;
        s.entropy.categoryName = "Structural Code / Page Tables / Kernel Headers";
        s.ranges = {{0, 0x1000, 651264}, {1, 0x100000, 16908685312ULL}};

        std::string json = SerializeMapJson(s);
        assert(json.find("\"producer\": \"PhylaRAM\"") != std::string::npos);
        assert(json.find("\"schema\": \"phylaram-map-2\"") != std::string::npos);
        assert(json.find("\"status\": \"complete\"") != std::string::npos);
        assert(json.find("\"kernel_hints\"") != std::string::npos);
        assert(json.find("\"directory_table_base\": \"0x1AA000\"") != std::string::npos);
        assert(json.find("\"compliance_standards\"") != std::string::npos);
        assert(json.find("\"MITRE ATT&CK (Enterprise)\"") != std::string::npos);
        assert(json.find("\"T1005 T1003 T1562.001\"") != std::string::npos);
        assert(json.find("\"wavelet_entropy\"") != std::string::npos);
        assert(json.find("\"orbit_hash\": \"0x123456789ABCDEF0\"") != std::string::npos);
        assert(json.find("\"start\": \"0x1000\"") != std::string::npos);
    }

    // Test 2: Incomplete acquisition due to unreadable page
    {
        AcquisitionSummary s;
        s.completed = true;
        s.topologyChanged = false;
        s.logicalSize = 1000000;
        s.physicalBytes = 1000000;
        s.acquiredBytes = 995904;
        s.unreadableBytes = 4096;
        s.sha256 = "deadbeef";
        s.ranges = {{0, 0x1000, 1000000}};
        s.unreadable = {{0x20004000, 4096, static_cast<long>(0xC000009C)}};

        std::string json = SerializeMapJson(s);
        assert(json.find("\"status\": \"incomplete\"") != std::string::npos);
        assert(json.find("\"unreadable_bytes\": 4096") != std::string::npos);
        assert(json.find("\"ntstatus\": \"0xC000009C\"") != std::string::npos);
        assert(json.find("\"compliance_standards\"") != std::string::npos);
    }

    // Test 3: Incomplete acquisition due to topology mutation
    {
        AcquisitionSummary s;
        s.completed = true;
        s.topologyChanged = true;
        s.logicalSize = 1000000;
        s.physicalBytes = 1000000;
        s.acquiredBytes = 1000000;
        s.unreadableBytes = 0;
        s.sha256 = "deadbeef";

        std::string json = SerializeMapJson(s);
        assert(json.find("\"status\": \"incomplete\"") != std::string::npos);
        assert(json.find("\"topology_changed\": true") != std::string::npos);
    }

    std::cout << "[PASS] Map JSON serialization and schema contract tests passed successfully.\n";
    return 0;
}
