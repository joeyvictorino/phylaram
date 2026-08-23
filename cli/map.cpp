#include "phylaram.hpp"
#include <sstream>
#include <iomanip>

static std::string NarrowUtf8(const std::wstring& s)
{
    if (s.empty()) {
        return {};
    }
    int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
    if (n <= 0) {
        return {};
    }
    std::string out(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), n, nullptr, nullptr);
    return out;
}

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

static bool WriteAtomicString(const std::wstring& targetPath, const std::string& content)
{
    ScopedHandle file(CreateFileW(targetPath.c_str(),
                                  GENERIC_WRITE,
                                  0,
                                  nullptr,
                                  CREATE_NEW,
                                  FILE_ATTRIBUTE_NORMAL,
                                  nullptr));
    if (!file) {
        return false;
    }

    DWORD total = static_cast<DWORD>(content.size());
    const char* ptr = content.data();

    while (total > 0) {
        DWORD written = 0;
        if (!WriteFile(file.Get(), ptr, total, &written, nullptr) || written == 0) {
            file.Reset();
            DeleteFileW(targetPath.c_str());
            return false;
        }
        ptr += written;
        total -= written;
    }

    if (!FlushFileBuffers(file.Get())) {
        file.Reset();
        DeleteFileW(targetPath.c_str());
        return false;
    }

    return true;
}

bool WriteMapJson(const std::wstring& path, const AcquisitionSummary& s)
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

    return WriteAtomicString(path, f.str());
}

bool WriteSha256Sidecar(const std::wstring& path, const std::wstring& rawFileName, const std::string& sha256)
{
    std::string content = sha256 + "  " + NarrowUtf8(rawFileName) + "\n";
    return WriteAtomicString(path, content);
}

bool PromoteStagingFile(const std::wstring& stagingPath, const std::wstring& finalPath)
{
    // Move without MOVEFILE_REPLACE_EXISTING ensuring atomic promotion that fails if destination exists
    return MoveFileExW(stagingPath.c_str(), finalPath.c_str(), MOVEFILE_WRITE_THROUGH) != FALSE;
}
