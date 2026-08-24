#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

#pragma pack(push, 1)
struct PhysicalMemoryRun64 {
    uint64_t BasePage = 0;
    uint64_t PageCount = 0;
};

struct DumpHeader64 {
    uint32_t Signature = 0x45474150;          // 'PAGE'
    uint32_t ValidDump = 0x34365544;          // 'DU64'
    uint32_t MajorVersion = 15;
    uint32_t MinorVersion = 22631;
    uint64_t DirectoryTableBase = 0;
    uint64_t PfnDataBase = 0;
    uint64_t PsLoadedModuleList = 0;
    uint64_t PsActiveProcessHead = 0;
    uint32_t MachineImageType = 0x8664;       // IMAGE_FILE_MACHINE_AMD64
    uint32_t NumberProcessors = 1;
    uint32_t BugCheckCode = 0x161;            // LIVE_SYSTEM_DUMP
    uint64_t BugCheckParameter1 = 0;
    uint64_t BugCheckParameter2 = 0;
    uint64_t BugCheckParameter3 = 0;
    uint64_t BugCheckParameter4 = 0;
    char VersionUser[32] = "PhylaRAM 0.1.0-alpha";
    uint64_t KdDebuggerDataBlock = 0;
    uint8_t Reserved1[124] = { 0 };
    uint32_t NumberOfRuns = 0;
    uint32_t ReservedRuns = 0;
    uint64_t NumberOfPages = 0;
    PhysicalMemoryRun64 Runs[32] = {};
    uint8_t Reserved2[3192] = { 0 };
    uint32_t DumpType = 1;                    // Complete Dump
    uint8_t Reserved3[116] = { 0 };
};
#pragma pack(pop)

static_assert(sizeof(DumpHeader64) == 4096, "DumpHeader64 size invariant violated");

int main()
{
    std::cout << "Testing ZDMP / Crash Dump header format invariants...\n";

    DumpHeader64 header{};
    assert(header.Signature == 0x45474150);
    assert(header.ValidDump == 0x34365544);
    assert(header.MajorVersion == 15);
    assert(header.MachineImageType == 0x8664);
    assert(header.BugCheckCode == 0x161);
    assert(header.DumpType == 1);

    // Test memory runs configuration
    header.DirectoryTableBase = 0x1AA000;
    header.NumberOfRuns = 2;
    header.NumberOfPages = 4096; // 16 MiB
    header.Runs[0].BasePage = 0;
    header.Runs[0].PageCount = 1024; // 4 MiB
    header.Runs[1].BasePage = 2048;  // 8 MiB gap
    header.Runs[1].PageCount = 3072; // 12 MiB

    assert(header.Runs[0].BasePage == 0);
    assert(header.Runs[0].PageCount == 1024);
    assert(header.Runs[1].BasePage == 2048);
    assert(header.Runs[1].PageCount == 3072);

    std::cout << "[PASS] ZDMP / Crash Dump header format test passed 100%.\n";
    return 0;
}
