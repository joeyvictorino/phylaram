#include "../shared/interfaces.hpp"

#include <cassert>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace {

std::string Hex64(uint64_t value)
{
    std::ostringstream stream;
    stream << "0x" << std::hex << std::uppercase << value;
    return stream.str();
}

std::string Hex32(uint32_t value)
{
    std::ostringstream stream;
    stream << "0x" << std::hex << std::uppercase << std::setw(8)
           << std::setfill('0') << value;
    return stream.str();
}

std::string SerializeFinalizedMap(const AcquisitionSummary& summary)
{
    assert(summary.completed);
    assert(summary.sha256.size() == 64);

    const char* const status =
        summary.unreadableBytes != 0 || summary.topologyChanged
            ? "incomplete"
            : "complete";

    std::ostringstream output;
    output << "{\n"
           << "  \"producer\": \"PhylaRAM\",\n"
           << "  \"producer_version\": \"0.1.0-alpha\",\n"
           << "  \"schema\": \"phylaram-map-2\",\n"
           << "  \"status\": \"" << status << "\",\n"
           << "  \"logical_size\": " << summary.logicalSize << ",\n"
           << "  \"physical_bytes\": " << summary.physicalBytes << ",\n"
           << "  \"acquired_bytes\": " << summary.acquiredBytes << ",\n"
           << "  \"unreadable_bytes\": " << summary.unreadableBytes << ",\n"
           << "  \"topology_changed\": "
           << (summary.topologyChanged ? "true" : "false") << ",\n"
           << "  \"sha256\": \"" << summary.sha256 << "\",\n";

    if (summary.hints.available) {
        output << "  \"kernel_hints\": {\n"
               << "    \"hypervisor_present\": "
               << (summary.hints.hypervisorPresent ? "true" : "false") << ",\n"
               << "    \"directory_table_base\": \""
               << Hex64(summary.hints.directoryTableBase) << "\",\n"
               << "    \"kpcr_address\": \""
               << Hex64(summary.hints.kpcrAddress) << "\",\n"
               << "    \"kernel_base\": \""
               << Hex64(summary.hints.kernelBase) << "\",\n"
               << "    \"kernel_size\": " << summary.hints.kernelSize << ",\n"
               << "    \"major_version\": " << summary.hints.majorVersion << ",\n"
               << "    \"minor_version\": " << summary.hints.minorVersion << ",\n"
               << "    \"build_number\": " << summary.hints.buildNumber << ",\n"
               << "    \"processors\": "
               << summary.hints.numberOfProcessors << "\n"
               << "  },\n";
    }

    output << "  \"ranges\": [\n";
    for (size_t index = 0; index < summary.ranges.size(); ++index) {
        const MemoryRun& range = summary.ranges[index];
        output << "    {\"driver_run\": " << range.driverIndex
               << ", \"start\": \"" << Hex64(range.base)
               << "\", \"length\": " << range.length << "}";
        if (index + 1 != summary.ranges.size()) {
            output << ',';
        }
        output << "\n";
    }
    output << "  ],\n  \"unreadable\": [\n";

    for (size_t index = 0; index < summary.unreadable.size(); ++index) {
        const UnreadableSpan& span = summary.unreadable[index];
        output << "    {\"start\": \"" << Hex64(span.start)
               << "\", \"length\": " << span.length
               << ", \"ntstatus\": \""
               << Hex32(static_cast<uint32_t>(span.status)) << "\"}";
        if (index + 1 != summary.unreadable.size()) {
            output << ',';
        }
        output << "\n";
    }

    output << "  ]\n}\n";
    return output.str();
}

constexpr char kSha256[] =
    "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";

} // namespace

int main()
{
    {
        AcquisitionSummary summary;
        summary.completed = true;
        summary.logicalSize = 0x300000;
        summary.physicalBytes = 0x200000;
        summary.acquiredBytes = 0x200000;
        summary.sha256 = kSha256;
        summary.hints.available = true;
        summary.hints.hypervisorPresent = true;
        summary.hints.directoryTableBase = 0x1AA000;
        summary.hints.kpcrAddress = 0xFFFFF80123450000ULL;
        summary.hints.kernelBase = 0xFFFFF80112340000ULL;
        summary.hints.kernelSize = 12582912;
        summary.hints.buildNumber = 22631;
        summary.hints.numberOfProcessors = 16;
        summary.ranges = {
            {0, 0x1000, 0xFF000},
            {1, 0x200000, 0x100000},
        };

        const std::string json = SerializeFinalizedMap(summary);
        assert(json.find("\"schema\": \"phylaram-map-2\"") != std::string::npos);
        assert(json.find("\"status\": \"complete\"") != std::string::npos);
        assert(json.find("\"directory_table_base\": \"0x1AA000\"") != std::string::npos);
        assert(json.find("\"wavelet_entropy\"") == std::string::npos);
        assert(json.find("\"compliance_standards\"") == std::string::npos);
    }

    {
        AcquisitionSummary summary;
        summary.completed = true;
        summary.logicalSize = 0x3000;
        summary.physicalBytes = 0x3000;
        summary.acquiredBytes = 0x2000;
        summary.unreadableBytes = 0x1000;
        summary.sha256 = kSha256;
        summary.ranges = {{0, 0, 0x3000}};
        summary.unreadable = {{0x1000, 0x1000, static_cast<long>(0xC000009C)}};

        const std::string json = SerializeFinalizedMap(summary);
        assert(json.find("\"status\": \"incomplete\"") != std::string::npos);
        assert(json.find("\"unreadable_bytes\": 4096") != std::string::npos);
        assert(json.find("\"ntstatus\": \"0xC000009C\"") != std::string::npos);
    }

    {
        AcquisitionSummary summary;
        summary.completed = true;
        summary.topologyChanged = true;
        summary.logicalSize = 0x1000;
        summary.physicalBytes = 0x1000;
        summary.acquiredBytes = 0x1000;
        summary.sha256 = kSha256;
        summary.ranges = {{0, 0, 0x1000}};

        const std::string json = SerializeFinalizedMap(summary);
        assert(json.find("\"status\": \"incomplete\"") != std::string::npos);
        assert(json.find("\"topology_changed\": true") != std::string::npos);
    }

    std::cout << "[PASS] Canonical provenance-map contract tests passed.\n";
    return 0;
}
