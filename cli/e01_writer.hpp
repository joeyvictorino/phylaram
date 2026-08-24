#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../shared/interfaces.hpp"
#include "phylaram.hpp"

#pragma pack(push, 1)
struct EwfFileHeader {
    uint8_t Signature[8] = { 'E', 'V', 'F', 0x09, 0x0D, 0x0A, 0xFF, 0x00 };
    uint8_t FieldsStart = 0x01;
    uint16_t SegmentNumber = 1;
    uint16_t FieldsEnd = 0x00;
};

struct EwfSectionHeader {
    char Type[16] = { 0 };
    uint64_t NextOffset = 0;
    uint64_t SectionSize = 0;
    uint8_t Padding[40] = { 0 };
    uint32_t Checksum = 0;
};

struct EwfVolumeSection {
    uint32_t MediaType = 0x00000001;          // RAM / Disk Media
    uint32_t Unknown1 = 0;
    uint32_t ChunkCount = 0;
    uint32_t SectorsPerChunk = 128;           // 64 KiB (128 * 512)
    uint32_t BytesPerSector = 512;
    uint64_t SectorCount = 0;
    uint8_t Unknown2[8] = { 0 };
    uint8_t Guid[16] = { 0 };
    uint8_t Reserved[940] = { 0 };
};
#pragma pack(pop)

static_assert(sizeof(EwfFileHeader) == 13, "EwfFileHeader must be 13 bytes");
static_assert(sizeof(EwfSectionHeader) == 76, "EwfSectionHeader must be 76 bytes");
static_assert(sizeof(EwfVolumeSection) == 992, "EwfVolumeSection must be 992 bytes");

class E01Writer final : public IRawWriter {
public:
    E01Writer();
    ~E01Writer() override { Close(); }

    E01Writer(const E01Writer&) = delete;
    E01Writer& operator=(const E01Writer&) = delete;
    E01Writer(E01Writer&&) noexcept = default;
    E01Writer& operator=(E01Writer&&) noexcept = default;

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
    bool FlushActiveChunk();
    bool WriteHeaderSection();
    bool WriteVolumeSection();
    bool WriteTableSection();
    bool WriteHashSection();
    bool WriteDoneSection();
    uint32_t CalculateAdler32(const uint8_t* data, size_t length, uint32_t adler = 1) const noexcept;

    ScopedHandle file_;
    std::vector<MemoryRun> runs_;
    KernelHints hints_;
    EvidenceMetadata metadata_;
    uint64_t totalPhysicalBytes_ = 0;
    uint64_t currentFileOffset_ = 0;

    // 64 KiB chunk buffering
    static constexpr size_t kChunkSize = 65536;
    std::vector<uint8_t> chunkBuffer_;
    size_t chunkBufferUsed_ = 0;
    uint32_t totalChunks_ = 0;
    std::vector<uint32_t> chunkTableOffsets_;

    // Section tracking
    uint64_t headerSectionOffset_ = 0;
    uint64_t volumeSectionOffset_ = 0;
    uint64_t sectorsSectionOffset_ = 0;
    uint64_t tableSectionOffset_ = 0;
    uint64_t hashSectionOffset_ = 0;
    uint64_t doneSectionOffset_ = 0;

    DWORD lastError_ = ERROR_SUCCESS;
};
