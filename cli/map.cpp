#include "phylaram.hpp"

#include <algorithm>
#include <cctype>
#include <climits>
#include <iomanip>
#include <sstream>

namespace {

constexpr size_t kWriteChunkBytes = 1024u * 1024u;

std::string NarrowUtf8(const std::wstring& value)
{
    if (value.empty() || value.size() > static_cast<size_t>(INT_MAX)) {
        return {};
    }

    const int sourceCharacters = static_cast<int>(value.size());
    const int outputBytes = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        value.data(),
        sourceCharacters,
        nullptr,
        0,
        nullptr,
        nullptr);
    if (outputBytes <= 0) {
        return {};
    }

    std::string output(static_cast<size_t>(outputBytes), '\0');
    const int converted = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        value.data(),
        sourceCharacters,
        output.data(),
        outputBytes,
        nullptr,
        nullptr);
    if (converted != outputBytes) {
        return {};
    }
    return output;
}

std::string Hex64(uint64_t value)
{
    std::ostringstream stream;
    stream << "0x" << std::hex << std::uppercase << value;
    return stream.str();
}

std::string Hex32(uint32_t value)
{
    std::ostringstream stream;
    stream << "0x" << std::hex << std::uppercase
           << std::setw(8) << std::setfill('0') << value;
    return stream.str();
}

bool IsSha256Hex(const std::string& value)
{
    return value.size() == 64 &&
           std::all_of(
               value.begin(),
               value.end(),
               [](unsigned char character) {
                   return std::isxdigit(character) != 0;
               });
}

bool WriteAtomicString(const std::wstring& targetPath,
                       const std::string& content)
{
    ScopedHandle file(CreateFileW(
        targetPath.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL,
        nullptr));
    if (!file) {
        return false;
    }

    size_t offset = 0;
    while (offset < content.size()) {
        const DWORD requested = static_cast<DWORD>(
            std::min(content.size() - offset, kWriteChunkBytes));
        DWORD written = 0;
        if (!WriteFile(
                file.Get(),
                content.data() + offset,
                requested,
                &written,
                nullptr) ||
            written == 0) {
            file.Reset();
            DeleteFileW(targetPath.c_str());
            return false;
        }
        offset += written;
    }

    if (!FlushFileBuffers(file.Get())) {
        file.Reset();
        DeleteFileW(targetPath.c_str());
        return false;
    }

    return true;
}

} // namespace

bool WriteMapJson(const std::wstring& path, const AcquisitionSummary& summary)
{
    if (!summary.completed ||
        !IsSha256Hex(summary.sha256) ||
        summary.ranges.empty() ||
        summary.acquiredBytes > UINT64_MAX - summary.unreadableBytes ||
        summary.acquiredBytes + summary.unreadableBytes != summary.physicalBytes) {
        return false;
    }

    const char* const status =
        summary.unreadableBytes != 0 || summary.topologyChanged
            ? "incomplete"
            : "complete";

    std::ostringstream output;
    output << "{\n";
    output << "  \"producer\": \"PhylaRAM\",\n";
    output << "  \"producer_version\": \"0.1.0-alpha\",\n";
    output << "  \"schema\": \"phylaram-map-2\",\n";
    output << "  \"status\": \"" << status << "\",\n";
    output << "  \"logical_size\": " << summary.logicalSize << ",\n";
    output << "  \"physical_bytes\": " << summary.physicalBytes << ",\n";
    output << "  \"acquired_bytes\": " << summary.acquiredBytes << ",\n";
    output << "  \"unreadable_bytes\": " << summary.unreadableBytes << ",\n";
    output << "  \"topology_changed\": "
           << (summary.topologyChanged ? "true" : "false") << ",\n";
    output << "  \"sha256\": \"" << summary.sha256 << "\",\n";

    if (summary.hints.available) {
        output << "  \"kernel_hints\": {\n";
        output << "    \"hypervisor_present\": "
               << (summary.hints.hypervisorPresent ? "true" : "false")
               << ",\n";
        output << "    \"directory_table_base\": \""
               << Hex64(summary.hints.directoryTableBase) << "\",\n";
        output << "    \"kpcr_address\": \""
               << Hex64(summary.hints.kpcrAddress) << "\",\n";
        output << "    \"kernel_base\": \""
               << Hex64(summary.hints.kernelBase) << "\",\n";
        output << "    \"kernel_size\": " << summary.hints.kernelSize
               << ",\n";
        output << "    \"major_version\": " << summary.hints.majorVersion
               << ",\n";
        output << "    \"minor_version\": " << summary.hints.minorVersion
               << ",\n";
        output << "    \"build_number\": " << summary.hints.buildNumber
               << ",\n";
        output << "    \"processors\": "
               << summary.hints.numberOfProcessors << "\n";
        output << "  },\n";
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
    output << "  ],\n";

    output << "  \"unreadable\": [\n";
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
    output << "  ]\n";
    output << "}\n";

    return WriteAtomicString(path, output.str());
}

bool WriteSha256Sidecar(const std::wstring& path,
                        const std::wstring& rawFileName,
                        const std::string& sha256)
{
    if (!IsSha256Hex(sha256)) {
        return false;
    }

    const std::string fileName = NarrowUtf8(rawFileName);
    if (fileName.empty()) {
        return false;
    }

    return WriteAtomicString(path, sha256 + "  " + fileName + "\n");
}

bool PromoteStagingFile(const std::wstring& stagingPath,
                        const std::wstring& finalPath)
{
    return MoveFileExW(
               stagingPath.c_str(),
               finalPath.c_str(),
               MOVEFILE_WRITE_THROUGH) != FALSE;
}
