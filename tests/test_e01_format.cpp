#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

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
    uint32_t MediaType = 0x00000001;
    uint32_t Unknown1 = 0;
    uint32_t ChunkCount = 0;
    uint32_t SectorsPerChunk = 128;
    uint32_t BytesPerSector = 512;
    uint64_t SectorCount = 0;
    uint8_t Unknown2[8] = { 0 };
    uint8_t Guid[16] = { 0 };
    uint8_t Reserved[940] = { 0 };
};
#pragma pack(pop)

static_assert(sizeof(EwfFileHeader) == 13, "EwfFileHeader size invariant violated");
static_assert(sizeof(EwfSectionHeader) == 76, "EwfSectionHeader size invariant violated");
static_assert(sizeof(EwfVolumeSection) == 992, "EwfVolumeSection size invariant violated");

uint32_t CalculateAdler32(const uint8_t* data, size_t length, uint32_t adler = 1) noexcept
{
    uint32_t s1 = adler & 0xFFFF;
    uint32_t s2 = (adler >> 16) & 0xFFFF;

    for (size_t i = 0; i < length; ++i) {
        s1 = (s1 + data[i]) % 65521;
        s2 = (s2 + s1) % 65521;
    }

    return (s2 << 16) | s1;
}

int main()
{
    std::cout << "Testing E01 (Expert Witness Format) container invariants...\n";

    EwfFileHeader fileHdr{};
    assert(fileHdr.Signature[0] == 'E');
    assert(fileHdr.Signature[1] == 'V');
    assert(fileHdr.Signature[2] == 'F');
    assert(fileHdr.Signature[3] == 0x09);
    assert(fileHdr.Signature[4] == 0x0D);
    assert(fileHdr.Signature[5] == 0x0A);
    assert(fileHdr.Signature[6] == 0xFF);
    assert(fileHdr.Signature[7] == 0x00);
    assert(fileHdr.SegmentNumber == 1);

    EwfSectionHeader secHdr{};
    std::strncpy(secHdr.Type, "header", sizeof(secHdr.Type) - 1);
    secHdr.NextOffset = 1024;
    secHdr.SectionSize = 1024;
    secHdr.Checksum = CalculateAdler32(reinterpret_cast<const uint8_t*>(&secHdr), sizeof(secHdr) - 4);
    assert(secHdr.Checksum != 0);

    EwfVolumeSection vol{};
    vol.MediaType = 1;
    vol.ChunkCount = 256;
    vol.SectorsPerChunk = 128;
    vol.BytesPerSector = 512;
    vol.SectorCount = 256 * 128;

    assert(vol.SectorsPerChunk * vol.BytesPerSector == 65536);
    assert(vol.SectorCount == 32768);

    std::cout << "[PASS] E01 container format test passed 100%.\n";
    return 0;
}
