#include "phylaram.hpp"
#include <cstring>
#include <vector>

#pragma pack(push, 1)
struct PHYSICAL_MEMORY_RUN64 {
    uint64_t BasePage;
    uint64_t PageCount;
};

struct PHYSICAL_MEMORY_DESCRIPTOR64 {
    uint32_t NumberOfRuns;
    uint32_t NumberOfPages;
    PHYSICAL_MEMORY_RUN64 Run[1]; // Flexible array member
};

struct DUMP_HEADER64 {
    uint32_t Signature;              // 'PAGE' = 0x45474150
    uint32_t ValidDump;              // 'PMUD' = 0x44554D50
    uint32_t MajorVersion;
    uint32_t MinorVersion;
    uint64_t DirectoryTableBase;
    uint64_t PfnDataBase;
    uint64_t PsLoadedModuleList;
    uint64_t PsActiveProcessHead;
    uint32_t MachineImageType;        // 0x8664 = x64
    uint32_t NumberProcessors;
    uint32_t BugCheckCode;            // 0 = Live Manual Capture
    uint32_t BugCheckCodeParameter[4];
    uint8_t  VersionUser[32];
    uint32_t DumpType;               // 1 = DUMP_TYPE_FULL
    uint32_t Comment[128];
    uint8_t  Reserved1[1024];
    uint64_t RequiredDumpSpace;
    uint8_t  Reserved2[1024];
    // PhysicalMemoryBlock is embedded at offset 0x800 or dynamically placed
};
#pragma pack(pop)

bool WriteCrashDumpHeader(HANDLE file, const AcquisitionSummary& summary)
{
    if (file == INVALID_HANDLE_VALUE || file == nullptr) {
        return false;
    }

    std::vector<uint8_t> header(8192, 0); // 8 KiB standard DUMP_HEADER64
    auto* dh = reinterpret_cast<DUMP_HEADER64*>(header.data());

    dh->Signature = 0x45474150; // 'PAGE'
    dh->ValidDump = 0x44554D50; // 'PMUD'
    dh->MajorVersion = summary.hints.majorVersion ? summary.hints.majorVersion : 10;
    dh->MinorVersion = summary.hints.buildNumber ? summary.hints.buildNumber : 22631;
    dh->DirectoryTableBase = summary.hints.directoryTableBase;
    dh->MachineImageType = 0x8664; // x64
    dh->NumberProcessors = summary.hints.numberOfProcessors ? summary.hints.numberOfProcessors : 4;
    dh->BugCheckCode = 0x00000160; // MANUALLY_INITIATED_CRASH
    dh->DumpType = 1;              // DUMP_TYPE_FULL
    dh->RequiredDumpSpace = 8192 + summary.logicalSize;

    // Place physical memory descriptor at standard offset 0xF88 in DUMP_HEADER64
    size_t descOffset = 0xF88;
    if (descOffset + sizeof(uint32_t) * 2 + summary.ranges.size() * sizeof(PHYSICAL_MEMORY_RUN64) <= header.size()) {
        auto* desc = reinterpret_cast<PHYSICAL_MEMORY_DESCRIPTOR64*>(header.data() + descOffset);
        desc->NumberOfRuns = static_cast<uint32_t>(summary.ranges.size());
        desc->NumberOfPages = static_cast<uint32_t>(summary.physicalBytes / PHYLA_PAGE_SIZE);

        for (size_t i = 0; i < summary.ranges.size(); ++i) {
            desc->Run[i].BasePage = summary.ranges[i].base / PHYLA_PAGE_SIZE;
            desc->Run[i].PageCount = summary.ranges[i].length / PHYLA_PAGE_SIZE;
        }
    }

    LARGE_INTEGER pos{};
    pos.QuadPart = 0;
    if (!SetFilePointerEx(file, pos, nullptr, FILE_BEGIN)) {
        return false;
    }

    DWORD written = 0;
    return WriteFile(file, header.data(), static_cast<DWORD>(header.size()), &written, nullptr) && written == header.size();
}
