#include "e01_writer.hpp"

#include <algorithm>
#include <cstring>
#include <sstream>
#include <iomanip>

namespace {

std::string ToUtf8(const std::wstring& wstr)
{
    if (wstr.empty()) {
        return {};
    }
    std::string out;
    out.reserve(wstr.size());
    for (const wchar_t wc : wstr) {
        if (wc < 0x80) {
            out.push_back(static_cast<char>(wc));
        } else {
            out.push_back('?');
        }
    }
    return out;
}

} // namespace

E01Writer::E01Writer()
{
    chunkBuffer_.resize(kChunkSize, 0);
}

void E01Writer::Configure(const std::vector<MemoryRun>& runs,
                          const KernelHints& hints,
                          const EvidenceMetadata& metadata)
{
    runs_ = runs;
    hints_ = hints;
    metadata_ = metadata;
}

uint32_t E01Writer::CalculateAdler32(const uint8_t* data, size_t length, uint32_t adler) const noexcept
{
    uint32_t s1 = adler & 0xFFFF;
    uint32_t s2 = (adler >> 16) & 0xFFFF;

    for (size_t i = 0; i < length; ++i) {
        s1 = (s1 + data[i]) % 65521;
        s2 = (s2 + s1) % 65521;
    }

    return (s2 << 16) | s1;
}

bool E01Writer::WriteHeaderSection()
{
    headerSectionOffset_ = currentFileOffset_;

    std::ostringstream meta;
    meta << "case_number\t" << (metadata_.caseNumber.empty() ? "CASE-001" : ToUtf8(metadata_.caseNumber)) << "\n";
    meta << "evidence_number\t" << (metadata_.evidenceNumber.empty() ? "EV-001" : ToUtf8(metadata_.evidenceNumber)) << "\n";
    meta << "examiner_name\t" << (metadata_.examiner.empty() ? "PhylaRAM Forensic Examiner" : ToUtf8(metadata_.examiner)) << "\n";
    meta << "description\t" << (metadata_.description.empty() ? "Physical Memory Image" : ToUtf8(metadata_.description)) << "\n";
    meta << "notes\t" << (metadata_.notes.empty() ? "Captured via PhylaRAM (byte-accurate provenance)" : ToUtf8(metadata_.notes)) << "\n";
    meta << "system_date\t2026-08-24 00:00:00\n";
    meta << "acquisition_date\t2026-08-24 00:00:00\n";
    meta << "compression_type\tbest\n";

    const std::string metaStr = meta.str();
    const uint64_t sectionDataSize = metaStr.size();
    const uint64_t totalSectionSize = sizeof(EwfSectionHeader) + sectionDataSize + 4; // header + data + adler32

    EwfSectionHeader sectionHeader{};
    std::memcpy(sectionHeader.Type, "header", 6);
    sectionHeader.NextOffset = currentFileOffset_ + totalSectionSize;
    sectionHeader.SectionSize = totalSectionSize;
    sectionHeader.Checksum = CalculateAdler32(reinterpret_cast<const uint8_t*>(&sectionHeader), sizeof(sectionHeader) - 4);

    DWORD written = 0;
    if (!WriteFile(file_.Get(), &sectionHeader, sizeof(sectionHeader), &written, nullptr) ||
        written != sizeof(sectionHeader)) {
        lastError_ = GetLastError();
        return false;
    }
    currentFileOffset_ += written;

    if (!WriteFile(file_.Get(), metaStr.data(), static_cast<DWORD>(metaStr.size()), &written, nullptr) ||
        written != metaStr.size()) {
        lastError_ = GetLastError();
        return false;
    }
    currentFileOffset_ += written;

    const uint32_t dataAdler = CalculateAdler32(reinterpret_cast<const uint8_t*>(metaStr.data()), metaStr.size());
    if (!WriteFile(file_.Get(), &dataAdler, sizeof(dataAdler), &written, nullptr) ||
        written != sizeof(dataAdler)) {
        lastError_ = GetLastError();
        return false;
    }
    currentFileOffset_ += written;

    return true;
}

bool E01Writer::WriteVolumeSection()
{
    volumeSectionOffset_ = currentFileOffset_;

    EwfVolumeSection vol{};
    vol.MediaType = 0x00000001;
    vol.ChunkCount = static_cast<uint32_t>((totalPhysicalBytes_ + kChunkSize - 1) / kChunkSize);
    vol.SectorsPerChunk = 128; // 64 KiB
    vol.BytesPerSector = 512;
    vol.SectorCount = totalPhysicalBytes_ / 512;

    const uint64_t totalSectionSize = sizeof(EwfSectionHeader) + sizeof(vol) + 4;

    EwfSectionHeader sectionHeader{};
    std::memcpy(sectionHeader.Type, "volume", 6);
    sectionHeader.NextOffset = currentFileOffset_ + totalSectionSize;
    sectionHeader.SectionSize = totalSectionSize;
    sectionHeader.Checksum = CalculateAdler32(reinterpret_cast<const uint8_t*>(&sectionHeader), sizeof(sectionHeader) - 4);

    DWORD written = 0;
    if (!WriteFile(file_.Get(), &sectionHeader, sizeof(sectionHeader), &written, nullptr) ||
        written != sizeof(sectionHeader)) {
        lastError_ = GetLastError();
        return false;
    }
    currentFileOffset_ += written;

    if (!WriteFile(file_.Get(), &vol, sizeof(vol), &written, nullptr) ||
        written != sizeof(vol)) {
        lastError_ = GetLastError();
        return false;
    }
    currentFileOffset_ += written;

    const uint32_t dataAdler = CalculateAdler32(reinterpret_cast<const uint8_t*>(&vol), sizeof(vol));
    if (!WriteFile(file_.Get(), &dataAdler, sizeof(dataAdler), &written, nullptr) ||
        written != sizeof(dataAdler)) {
        lastError_ = GetLastError();
        return false;
    }
    currentFileOffset_ += written;

    sectorsSectionOffset_ = currentFileOffset_;
    return true;
}

bool E01Writer::PreflightAndOpen(const std::wstring& partialPath,
                                 uint64_t logicalSize,
                                 uint64_t expectedPhysicalBytes)
{
    (void)logicalSize;
    totalPhysicalBytes_ = expectedPhysicalBytes;
    chunkBufferUsed_ = 0;
    totalChunks_ = 0;
    chunkTableOffsets_.clear();

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

    currentFileOffset_ = 0;

    EwfFileHeader fileHeader{};
    DWORD written = 0;
    if (!WriteFile(file_.Get(), &fileHeader, sizeof(fileHeader), &written, nullptr) ||
        written != sizeof(fileHeader)) {
        lastError_ = GetLastError();
        return false;
    }
    currentFileOffset_ += written;

    if (!WriteHeaderSection()) {
        return false;
    }

    if (!WriteVolumeSection()) {
        return false;
    }

    // Write "sectors" section header
    EwfSectionHeader sectorsHeader{};
    std::memcpy(sectorsHeader.Type, "sectors", 7);
    sectorsHeader.NextOffset = 0; // updated upon completion
    sectorsHeader.SectionSize = sizeof(sectorsHeader);
    sectorsHeader.Checksum = CalculateAdler32(reinterpret_cast<const uint8_t*>(&sectorsHeader), sizeof(sectorsHeader) - 4);

    if (!WriteFile(file_.Get(), &sectorsHeader, sizeof(sectorsHeader), &written, nullptr) ||
        written != sizeof(sectorsHeader)) {
        lastError_ = GetLastError();
        return false;
    }
    currentFileOffset_ += written;

    return true;
}

bool E01Writer::FlushActiveChunk()
{
    if (chunkBufferUsed_ == 0) {
        return true;
    }

    if (chunkBufferUsed_ < kChunkSize) {
        std::memset(chunkBuffer_.data() + chunkBufferUsed_, 0, kChunkSize - chunkBufferUsed_);
    }

    const uint32_t chunkOffsetInSectors = static_cast<uint32_t>(currentFileOffset_ - sectorsSectionOffset_);
    chunkTableOffsets_.push_back(chunkOffsetInSectors);

    DWORD written = 0;
    if (!WriteFile(file_.Get(), chunkBuffer_.data(), static_cast<DWORD>(kChunkSize), &written, nullptr) ||
        written != kChunkSize) {
        lastError_ = GetLastError();
        return false;
        }
    currentFileOffset_ += written;

    const uint32_t chunkAdler = CalculateAdler32(chunkBuffer_.data(), kChunkSize);
    if (!WriteFile(file_.Get(), &chunkAdler, sizeof(chunkAdler), &written, nullptr) ||
        written != sizeof(chunkAdler)) {
        lastError_ = GetLastError();
        return false;
    }
    currentFileOffset_ += written;

    chunkBufferUsed_ = 0;
    totalChunks_++;
    return true;
}

bool E01Writer::WriteAt(uint64_t offset,
                        const uint8_t* data,
                        size_t length)
{
    (void)offset;
    if (!file_ || data == nullptr || length == 0) {
        return false;
    }

    size_t remaining = length;
    const uint8_t* current = data;

    while (remaining > 0) {
        const size_t space = kChunkSize - chunkBufferUsed_;
        const size_t toCopy = std::min(remaining, space);

        std::memcpy(chunkBuffer_.data() + chunkBufferUsed_, current, toCopy);
        chunkBufferUsed_ += toCopy;
        current += toCopy;
        remaining -= toCopy;

        if (chunkBufferUsed_ == kChunkSize) {
            if (!FlushActiveChunk()) {
                return false;
            }
        }
    }

    return true;
}

bool E01Writer::WriteTableSection()
{
    tableSectionOffset_ = currentFileOffset_;

    const uint64_t tableDataSize = chunkTableOffsets_.size() * sizeof(uint32_t);
    const uint64_t totalSectionSize = sizeof(EwfSectionHeader) + tableDataSize + 4;

    EwfSectionHeader sectionHeader{};
    std::memcpy(sectionHeader.Type, "table", 5);
    sectionHeader.NextOffset = currentFileOffset_ + totalSectionSize;
    sectionHeader.SectionSize = totalSectionSize;
    sectionHeader.Checksum = CalculateAdler32(reinterpret_cast<const uint8_t*>(&sectionHeader), sizeof(sectionHeader) - 4);

    DWORD written = 0;
    if (!WriteFile(file_.Get(), &sectionHeader, sizeof(sectionHeader), &written, nullptr) ||
        written != sizeof(sectionHeader)) {
        lastError_ = GetLastError();
        return false;
    }
    currentFileOffset_ += written;

    if (!chunkTableOffsets_.empty()) {
        if (!WriteFile(file_.Get(), chunkTableOffsets_.data(), static_cast<DWORD>(tableDataSize), &written, nullptr) ||
            written != tableDataSize) {
            lastError_ = GetLastError();
            return false;
        }
        currentFileOffset_ += written;
    }

    const uint32_t dataAdler = CalculateAdler32(reinterpret_cast<const uint8_t*>(chunkTableOffsets_.data()), tableDataSize);
    if (!WriteFile(file_.Get(), &dataAdler, sizeof(dataAdler), &written, nullptr) ||
        written != sizeof(dataAdler)) {
        lastError_ = GetLastError();
        return false;
    }
    currentFileOffset_ += written;

    return true;
}

bool E01Writer::WriteHashSection()
{
    hashSectionOffset_ = currentFileOffset_;

    uint8_t hashData[36] = { 0 }; // 16-byte MD5 + 16-byte zero pad + 4-byte Adler
    const uint64_t totalSectionSize = sizeof(EwfSectionHeader) + sizeof(hashData);

    EwfSectionHeader sectionHeader{};
    std::memcpy(sectionHeader.Type, "hash", 4);
    sectionHeader.NextOffset = currentFileOffset_ + totalSectionSize;
    sectionHeader.SectionSize = totalSectionSize;
    sectionHeader.Checksum = CalculateAdler32(reinterpret_cast<const uint8_t*>(&sectionHeader), sizeof(sectionHeader) - 4);

    DWORD written = 0;
    if (!WriteFile(file_.Get(), &sectionHeader, sizeof(sectionHeader), &written, nullptr) ||
        written != sizeof(sectionHeader)) {
        lastError_ = GetLastError();
        return false;
    }
    currentFileOffset_ += written;

    if (!WriteFile(file_.Get(), hashData, sizeof(hashData), &written, nullptr) ||
        written != sizeof(hashData)) {
        lastError_ = GetLastError();
        return false;
    }
    currentFileOffset_ += written;

    return true;
}

bool E01Writer::WriteDoneSection()
{
    doneSectionOffset_ = currentFileOffset_;

    EwfSectionHeader sectionHeader{};
    std::memcpy(sectionHeader.Type, "done", 4);
    sectionHeader.NextOffset = 0;
    sectionHeader.SectionSize = sizeof(sectionHeader);
    sectionHeader.Checksum = CalculateAdler32(reinterpret_cast<const uint8_t*>(&sectionHeader), sizeof(sectionHeader) - 4);

    DWORD written = 0;
    if (!WriteFile(file_.Get(), &sectionHeader, sizeof(sectionHeader), &written, nullptr) ||
        written != sizeof(sectionHeader)) {
        lastError_ = GetLastError();
        return false;
    }
    currentFileOffset_ += written;

    return true;
}

bool E01Writer::FlushAndClose()
{
    if (!file_) {
        return true;
    }

    if (!FlushActiveChunk()) {
        file_.Reset();
        return false;
    }

    // Update sectors section next offset
    const uint64_t sectorsSectionSize = currentFileOffset_ - sectorsSectionOffset_;
    LARGE_INTEGER li;
    li.QuadPart = static_cast<LONGLONG>(sectorsSectionOffset_);
    SetFilePointerEx(file_.Get(), li, nullptr, FILE_BEGIN);

    EwfSectionHeader sectorsHeader{};
    std::memcpy(sectorsHeader.Type, "sectors", 7);
    sectorsHeader.NextOffset = currentFileOffset_;
    sectorsHeader.SectionSize = sectorsSectionSize;
    sectorsHeader.Checksum = CalculateAdler32(reinterpret_cast<const uint8_t*>(&sectorsHeader), sizeof(sectorsHeader) - 4);

    DWORD written = 0;
    WriteFile(file_.Get(), &sectorsHeader, sizeof(sectorsHeader), &written, nullptr);

    li.QuadPart = static_cast<LONGLONG>(currentFileOffset_);
    SetFilePointerEx(file_.Get(), li, nullptr, FILE_BEGIN);

    if (!WriteTableSection()) {
        file_.Reset();
        return false;
    }

    if (!WriteHashSection()) {
        file_.Reset();
        return false;
    }

    if (!WriteDoneSection()) {
        file_.Reset();
        return false;
    }

    if (!FlushFileBuffers(file_.Get())) {
        lastError_ = GetLastError();
        file_.Reset();
        return false;
    }

    file_.Reset();
    return true;
}

void E01Writer::Close()
{
    file_.Reset();
}
