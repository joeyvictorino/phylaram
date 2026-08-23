#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>

struct MemoryRun {
    uint32_t driverIndex = 0;
    uint64_t base = 0;
    uint64_t length = 0;
};

struct ReadResult {
    uint64_t physicalAddress = 0;
    uint32_t requested = 0;
    uint32_t copied = 0;
    long copyStatus = 0;
    std::vector<uint8_t> data;
};

struct UnreadableSpan {
    uint64_t start = 0;
    uint64_t length = 0;
    long status = 0;
};

struct KernelHints {
    bool available = false;
    bool hypervisorPresent = false;
    uint32_t majorVersion = 0;
    uint32_t minorVersion = 0;
    uint32_t buildNumber = 0;
    uint32_t numberOfProcessors = 0;
    uint64_t directoryTableBase = 0; // System process CR3
    uint64_t kpcrAddress = 0;        // Current CPU KPCR (not necessarily CPU 0)
    uint64_t kernelBase = 0;         // NTOSKRNL base
    uint64_t kernelSize = 0;         // NTOSKRNL image size
};

#include "compliance_map.hpp"
#include "wavelet_classifier.hpp"

struct AcquisitionSummary {
    bool completed = false;
    bool topologyChanged = false;
    uint64_t logicalSize = 0;
    uint64_t physicalBytes = 0;
    uint64_t acquiredBytes = 0;
    uint64_t unreadableBytes = 0;
    std::string sha256;
    KernelHints hints;
    phylaram::WaveletEntropyMetrics entropy;
    std::vector<MemoryRun> ranges;
    std::vector<UnreadableSpan> unreadable;
};

class IDeviceSession {
public:
    virtual ~IDeviceSession() = default;
    virtual bool Open() = 0;
    virtual void Close() = 0;
    virtual bool Query(uint64_t& highestEnd, uint64_t& totalBytes, std::vector<MemoryRun>& runs) = 0;
    virtual bool QueryHints(KernelHints& hints) = 0;
    virtual bool Read(uint32_t runIndex, uint64_t offset, uint32_t length, ReadResult& result) = 0;
    virtual bool End(bool& topologyChanged) = 0;
    virtual uint32_t LastError() const noexcept = 0;
};

class IRawWriter {
public:
    virtual ~IRawWriter() = default;
    virtual bool PreflightAndOpen(const std::wstring& partialPath,
                                  uint64_t logicalSize,
                                  uint64_t expectedPhysicalBytes) = 0;
    virtual bool WriteAt(uint64_t offset, const uint8_t* data, size_t length) = 0;
    virtual bool FlushAndClose() = 0;
    virtual void Close() = 0;
    virtual bool IsSparse() const noexcept = 0;
    virtual uint32_t LastError() const noexcept = 0;
};

class IHasher {
public:
    virtual ~IHasher() = default;
    virtual bool Initialize() = 0;
    virtual bool Update(const uint8_t* data, size_t length) = 0;
    virtual bool UpdateZeros(uint64_t length) = 0;
    virtual bool Finish(std::string& hex) = 0;
    virtual void Reset() = 0;
};
