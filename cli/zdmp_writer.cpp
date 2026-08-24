#include "zdmp_writer.hpp"

#include <algorithm>
#include <cstring>

void ZdmpWriter::Configure(const std::vector<MemoryRun>& runs,
                           const KernelHints& hints,
                           const EvidenceMetadata& metadata)
{
    runs_ = runs;
    hints_ = hints;
    metadata_ = metadata;

    runFileOffsets_.clear();
    runFileOffsets_.reserve(runs.size());

    uint64_t currentOffset = sizeof(DumpHeader64); // 4096 bytes
    for (const auto& run : runs_) {
        runFileOffsets_.push_back(currentOffset);
        currentOffset += run.length;
    }
}

bool ZdmpWriter::PreflightAndOpen(const std::wstring& partialPath,
                                  uint64_t logicalSize,
                                  uint64_t expectedPhysicalBytes)
{
    (void)logicalSize;
    totalPhysicalBytes_ = expectedPhysicalBytes;

    file_.Reset(CreateFileW(partialPath.c_str(),
                            GENERIC_READ | GENERIC_WRITE,
                            0,
                            nullptr,
                            CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL,
                            nullptr));

    if (!file_) {
        lastError_ = GetLastError();
        return false;
    }

    DumpHeader64 header{};
    header.Signature = 0x45474150;          // 'PAGE'
    header.ValidDump = 0x34365544;          // 'DU64'
    header.MajorVersion = 15;
    header.MinorVersion = hints_.buildNumber != 0 ? hints_.buildNumber : 22631;
    header.DirectoryTableBase = hints_.directoryTableBase;
    header.MachineImageType = 0x8664;       // AMD64
    header.NumberProcessors = hints_.numberOfProcessors != 0 ? hints_.numberOfProcessors : 1;
    header.BugCheckCode = 0x161;            // LIVE_SYSTEM_DUMP
    header.DumpType = 1;                    // DUMP_TYPE_FULL

    if (hints_.kernelBase != 0) {
        header.PsLoadedModuleList = hints_.kernelBase;
    }

    std::strncpy(header.VersionUser, "PhylaRAM 0.1.0-alpha", sizeof(header.VersionUser) - 1);

    header.NumberOfRuns = static_cast<uint32_t>(std::min<size_t>(runs_.size(), 32));
    header.NumberOfPages = expectedPhysicalBytes / 4096;

    for (size_t i = 0; i < header.NumberOfRuns; ++i) {
        header.Runs[i].BasePage = runs_[i].base / 4096;
        header.Runs[i].PageCount = runs_[i].length / 4096;
    }

    DWORD written = 0;
    if (!WriteFile(file_.Get(), &header, sizeof(header), &written, nullptr) ||
        written != sizeof(header)) {
        lastError_ = GetLastError();
        return false;
    }

    return true;
}

uint64_t ZdmpWriter::CalculateFileOffset(uint64_t physicalAddress) const noexcept
{
    for (size_t i = 0; i < runs_.size(); ++i) {
        const auto& run = runs_[i];
        if (physicalAddress >= run.base && physicalAddress < run.base + run.length) {
            return runFileOffsets_[i] + (physicalAddress - run.base);
        }
    }
    return sizeof(DumpHeader64);
}

bool ZdmpWriter::WriteAt(uint64_t offset,
                         const uint8_t* data,
                         size_t length)
{
    if (!file_ || data == nullptr || length == 0) {
        return false;
    }

    const uint64_t fileOffset = CalculateFileOffset(offset);

    LARGE_INTEGER li;
    li.QuadPart = static_cast<LONGLONG>(fileOffset);
    if (!SetFilePointerEx(file_.Get(), li, nullptr, FILE_BEGIN)) {
        lastError_ = GetLastError();
        return false;
    }

    size_t remaining = length;
    const uint8_t* current = data;

    while (remaining > 0) {
        const DWORD chunkSize = static_cast<DWORD>(
            std::min<size_t>(remaining, static_cast<size_t>(MAXDWORD)));
        DWORD written = 0;

        if (!WriteFile(file_.Get(), current, chunkSize, &written, nullptr) ||
            written != chunkSize) {
            lastError_ = GetLastError();
            return false;
        }

        remaining -= written;
        current += written;
    }

    return true;
}

bool ZdmpWriter::FlushAndClose()
{
    if (!file_) {
        return true;
    }

    if (!FlushFileBuffers(file_.Get())) {
        lastError_ = GetLastError();
        file_.Reset();
        return false;
    }

    file_.Reset();
    return true;
}

void ZdmpWriter::Close()
{
    file_.Reset();
}
