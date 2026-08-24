#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../shared/interfaces.hpp"
#include "phylaram.hpp"

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
    // PhysicalMemoryBlock starts at offset 0x00F8 (248)
    uint32_t NumberOfRuns = 0;
    uint32_t ReservedRuns = 0;
    uint64_t NumberOfPages = 0;
    PhysicalMemoryRun64 Runs[32] = {};
    uint8_t Reserved2[3192] = { 0 };
    // Offset 0x0F88 (3976)
    uint32_t DumpType = 1;                    // DUMP_TYPE_FULL (Complete Memory Dump)
    uint8_t Reserved3[116] = { 0 };
};
#pragma pack(pop)

static_assert(sizeof(DumpHeader64) == 4096, "DumpHeader64 must be exactly 4096 bytes");

class ZdmpWriter final : public IRawWriter {
public:
    ZdmpWriter() = default;
    ~ZdmpWriter() override { Close(); }

    ZdmpWriter(const ZdmpWriter&) = delete;
    ZdmpWriter& operator=(const ZdmpWriter&) = delete;
    ZdmpWriter(ZdmpWriter&&) noexcept = default;
    ZdmpWriter& operator=(ZdmpWriter&&) noexcept = default;

    void Configure(const std::vector<MemoryRun>& runs,
                   const KernelHints& hints,
                   const EvidenceMetadata& metadata) override;

    bool PreflightAndOpen(const std::wstring& partialPath,
                          uint64_t logicalSize,
                          uint64_t expectedPhysicalBytes) override;
    bool WriteAt(uint64_t offset,
                 const uint8_t* data,
                 size_t length) override;
    bool FlushAndClose() override;
    void Close() override;

    [[nodiscard]] bool IsSparse() const noexcept override { return false; }
    [[nodiscard]] uint32_t LastError() const noexcept override { return lastError_; }

private:
    uint64_t CalculateFileOffset(uint64_t physicalAddress) const noexcept;

    ScopedHandle file_;
    std::vector<MemoryRun> runs_;
    std::vector<uint64_t> runFileOffsets_;
    KernelHints hints_;
    EvidenceMetadata metadata_;
    uint64_t totalPhysicalBytes_ = 0;
    DWORD lastError_ = ERROR_SUCCESS;
};
