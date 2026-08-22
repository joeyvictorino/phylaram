#include "../shared/interfaces.hpp"
#include <cassert>
#include <map>
#include <cstring>
#include <atomic>
#include <iostream>
#include <algorithm>

#define PHYLA_MAX_TRANSFER (16u * 1024u * 1024u)
#define PHYLA_PAGE_SIZE 4096u

struct AcquisitionConfig {
    bool quiet = false;
    uint32_t rateLimitMBps = 0;
};

static void AddUnreadable(AcquisitionSummary& summary, uint64_t start, uint64_t length, long status)
{
    if (!summary.unreadable.empty()) {
        auto& last = summary.unreadable.back();
        if (last.status == status && last.start + last.length == start) {
            last.length += length;
            summary.unreadableBytes += length;
            return;
        }
    }
    summary.unreadable.push_back({start, length, status});
    summary.unreadableBytes += length;
}

bool Acquire(IDeviceSession& device,
             IRawWriter& writer,
             IHasher* hasher,
             AcquisitionSummary& summary,
             std::atomic_bool& cancelled,
             const AcquisitionConfig& config)
{
    (void)config;
    uint64_t highestEnd = 0;
    uint64_t totalBytes = 0;
    std::vector<MemoryRun> runs;

    if (!device.Query(highestEnd, totalBytes, runs)) {
        return false;
    }

    device.QueryHints(summary.hints);

    summary.logicalSize = highestEnd;
    summary.physicalBytes = totalBytes;
    summary.ranges = runs;

    uint64_t hashPosition = 0;
    auto hashZerosTo = [&](uint64_t next) -> bool {
        if (!hasher) {
            hashPosition = next;
            return true;
        }
        if (next < hashPosition) {
            return false;
        }
        if (next > hashPosition) {
            if (!hasher->UpdateZeros(next - hashPosition)) {
                return false;
            }
        }
        hashPosition = next;
        return true;
    };

    uint64_t processedPhysical = 0;

    for (const auto& run : runs) {
        if (cancelled.load()) {
            return false;
        }

        uint64_t offset = 0;
        while (offset < run.length) {
            if (cancelled.load()) {
                return false;
            }

            uint32_t wanted = static_cast<uint32_t>(std::min<uint64_t>(PHYLA_MAX_TRANSFER, run.length - offset));
            ReadResult rr;
            if (!device.Read(run.driverIndex, offset, wanted, rr)) {
                return false;
            }

            uint64_t expectedPhysical = run.base + offset;
            if (rr.physicalAddress != expectedPhysical || rr.requested != wanted) {
                return false;
            }

            if (rr.copied > 0) {
                if (!hashZerosTo(rr.physicalAddress)) {
                    return false;
                }
                if (!writer.WriteAt(rr.physicalAddress, rr.data.data(), rr.copied)) {
                    return false;
                }
                if (hasher && !hasher->Update(rr.data.data(), rr.copied)) {
                    return false;
                }
                hashPosition += rr.copied;
                offset += rr.copied;
                processedPhysical += rr.copied;
                summary.acquiredBytes += rr.copied;
            }

            if (rr.copied == wanted) {
                // Fast path: full chunk transfer
            } else {
                // Isolate unreadable boundary at 4 KiB page granularity
                uint64_t chunkEnd = std::min<uint64_t>(run.length, offset + (wanted - rr.copied));
                while (offset < chunkEnd) {
                    if (cancelled.load()) {
                        return false;
                    }

                    uint64_t physical = run.base + offset;
                    uint32_t pageRemainder = static_cast<uint32_t>(std::min<uint64_t>(
                        PHYLA_PAGE_SIZE - (physical & (PHYLA_PAGE_SIZE - 1)), chunkEnd - offset));

                    ReadResult page;
                    if (!device.Read(run.driverIndex, offset, pageRemainder, page)) {
                        return false;
                    }
                    if (page.physicalAddress != physical) {
                        return false;
                    }

                    if (page.copied > 0) {
                        if (!hashZerosTo(page.physicalAddress)) {
                            return false;
                        }
                        if (!writer.WriteAt(page.physicalAddress, page.data.data(), page.copied)) {
                            return false;
                        }
                        if (hasher && !hasher->Update(page.data.data(), page.copied)) {
                            return false;
                        }
                        hashPosition += page.copied;
                        offset += page.copied;
                        processedPhysical += page.copied;
                        summary.acquiredBytes += page.copied;

                        if (page.copied == pageRemainder) {
                            break;
                        }
                        continue;
                    }

                    AddUnreadable(summary, physical, pageRemainder, page.copyStatus);
                    processedPhysical += pageRemainder;
                    offset += pageRemainder;
                }
            }
        }
    }

    if (!hashZerosTo(summary.logicalSize)) {
        return false;
    }

    bool changed = false;
    if (!device.End(changed)) {
        return false;
    }

    (void)processedPhysical;
    summary.topologyChanged = changed;
    summary.completed = true;
    return true;
}

class MockDeviceSession : public IDeviceSession {
public:
    uint64_t mockHighestEnd = 0x40000000ULL; // 1 GiB
    uint64_t mockTotalBytes = 0;
    std::vector<MemoryRun> mockRuns;
    std::map<uint64_t, long> injectedPageErrors; // Physical page -> NTSTATUS
    bool mutateTopologyAtEnd = false;

    bool Open() override { return true; }
    void Close() override {}

    bool Query(uint64_t& highestEnd, uint64_t& totalBytes, std::vector<MemoryRun>& runs) override {
        highestEnd = mockHighestEnd;
        totalBytes = mockTotalBytes;
        runs = mockRuns;
        return true;
    }

    bool QueryHints(KernelHints& hints) override {
        hints.available = true;
        hints.directoryTableBase = 0x1AA000;
        hints.kernelBase = 0xFFFFF80100000000ULL;
        return true;
    }

    bool Read(uint32_t runIndex, uint64_t offset, uint32_t length, ReadResult& result) override {
        if (runIndex >= mockRuns.size()) return false;
        const auto& run = mockRuns[runIndex];
        if (offset + length > run.length) return false;

        uint64_t phys = run.base + offset;
        result.physicalAddress = phys;
        result.requested = length;
        result.data.resize(length);

        uint64_t firstErrorPage = 0;
        bool hasError = false;
        for (const auto& [errPhys, status] : injectedPageErrors) {
            if (errPhys >= phys && errPhys < phys + length) {
                if (!hasError || errPhys < firstErrorPage) {
                    firstErrorPage = errPhys;
                    result.copyStatus = status;
                    hasError = true;
                }
            }
        }

        if (!hasError) {
            result.copied = length;
            result.copyStatus = 0;
            std::memset(result.data.data(), 0x55, length);
        } else {
            result.copied = static_cast<uint32_t>(firstErrorPage - phys);
            result.data.resize(result.copied);
            std::memset(result.data.data(), 0x55, result.copied);
        }
        return true;
    }

    bool End(bool& topologyChanged) override {
        topologyChanged = mutateTopologyAtEnd;
        return true;
    }

    uint32_t LastError() const noexcept override { return 0; }
};

class MockRawWriter : public IRawWriter {
public:
    std::map<uint64_t, std::vector<uint8_t>> writes;
    uint64_t configuredLogicalSize = 0;

    bool PreflightAndOpen(const std::wstring&, uint64_t logicalSize, uint64_t) override {
        configuredLogicalSize = logicalSize;
        return true;
    }
    bool WriteAt(uint64_t offset, const uint8_t* data, size_t length) override {
        writes[offset].assign(data, data + length);
        return true;
    }
    bool FlushAndClose() override { return true; }
    void Close() override {}
    bool IsSparse() const noexcept override { return true; }
    uint32_t LastError() const noexcept override { return 0; }
};

class MockHasher : public IHasher {
public:
    uint64_t totalHashed = 0;
    bool Initialize() override { totalHashed = 0; return true; }
    bool Update(const uint8_t*, size_t length) override { totalHashed += length; return true; }
    bool UpdateZeros(uint64_t length) override { totalHashed += length; return true; }
    bool Finish(std::string& hex) override { hex = "MOCK_SHA256_PASSED"; return true; }
    void Reset() override { totalHashed = 0; }
};

int main() {
    AcquisitionConfig cfg{true, 0};

    // Scenario 1: Clean Acquisition with 16 MiB transfer
    {
        MockDeviceSession mockDev;
        MockRawWriter mockWriter;
        MockHasher mockHasher;
        AcquisitionSummary summary;
        std::atomic_bool cancelled{false};

        mockDev.mockRuns = {{0, 0x1000, 16 * 1024 * 1024}};
        mockDev.mockTotalBytes = 16 * 1024 * 1024;
        mockDev.mockHighestEnd = 0x1000 + 16 * 1024 * 1024;

        assert(Acquire(mockDev, mockWriter, &mockHasher, summary, cancelled, cfg));
        assert(summary.completed);
        assert(!summary.topologyChanged);
        assert(summary.hints.available);
        assert(summary.hints.directoryTableBase == 0x1AA000);
        assert(summary.acquiredBytes == 16 * 1024 * 1024);
        assert(summary.unreadableBytes == 0);
        assert(mockHasher.totalHashed == summary.logicalSize);
    }

    // Scenario 2: 16 MiB + 1 byte run boundary
    {
        MockDeviceSession mockDev;
        MockRawWriter mockWriter;
        MockHasher mockHasher;
        AcquisitionSummary summary;
        std::atomic_bool cancelled{false};

        mockDev.mockRuns = {{0, 0x1000, (16 * 1024 * 1024) + 1}};
        mockDev.mockTotalBytes = (16 * 1024 * 1024) + 1;
        mockDev.mockHighestEnd = 0x1000 + (16 * 1024 * 1024) + 1;

        assert(Acquire(mockDev, mockWriter, &mockHasher, summary, cancelled, cfg));
        assert(summary.acquiredBytes == (16 * 1024 * 1024) + 1);
    }

    // Scenario 3: Bad ECC page (4 KiB) isolation and resumption
    {
        MockDeviceSession mockDev;
        MockRawWriter mockWriter;
        MockHasher mockHasher;
        AcquisitionSummary summary;
        std::atomic_bool cancelled{false};

        mockDev.mockRuns = {{0, 0x100000, 32 * 1024 * 1024}}; // 32 MiB run
        mockDev.mockTotalBytes = 32 * 1024 * 1024;
        mockDev.mockHighestEnd = 0x100000 + 32 * 1024 * 1024;
        // Inject error at 0x100000 + 4096 (second page)
        mockDev.injectedPageErrors[0x100000 + 4096] = -1073741668; // 0xC000009C STATUS_DEVICE_DATA_ERROR

        assert(Acquire(mockDev, mockWriter, &mockHasher, summary, cancelled, cfg));
        assert(summary.completed);
        assert(summary.unreadableBytes == 4096);
        assert(summary.unreadable.size() == 1);
        assert(summary.unreadable[0].start == 0x100000 + 4096);
        assert(summary.unreadable[0].length == 4096);
        assert(summary.acquiredBytes == (32 * 1024 * 1024) - 4096);
        assert(mockHasher.totalHashed == summary.logicalSize);
    }

    // Scenario 4: Multiple contiguous bad pages coalesced
    {
        MockDeviceSession mockDev;
        MockRawWriter mockWriter;
        MockHasher mockHasher;
        AcquisitionSummary summary;
        std::atomic_bool cancelled{false};

        mockDev.mockRuns = {{0, 0x100000, 4 * 1024 * 1024}};
        mockDev.mockTotalBytes = 4 * 1024 * 1024;
        mockDev.mockHighestEnd = 0x100000 + 4 * 1024 * 1024;
        // Inject 3 consecutive page errors
        mockDev.injectedPageErrors[0x100000] = 0xC0000001;
        mockDev.injectedPageErrors[0x100000 + 4096] = 0xC0000001;
        mockDev.injectedPageErrors[0x100000 + 8192] = 0xC0000001;

        assert(Acquire(mockDev, mockWriter, &mockHasher, summary, cancelled, cfg));
        assert(summary.unreadableBytes == 12288);
        assert(summary.unreadable.size() == 1); // Coalesced into 1 span of length 12288
        assert(summary.unreadable[0].length == 12288);
    }

    // Scenario 5: Dynamic topology change detection
    {
        MockDeviceSession mockDev;
        MockRawWriter mockWriter;
        MockHasher mockHasher;
        AcquisitionSummary summary;
        std::atomic_bool cancelled{false};

        mockDev.mockRuns = {{0, 0x1000, 0x10000}};
        mockDev.mockTotalBytes = 0x10000;
        mockDev.mockHighestEnd = 0x11000;
        mockDev.mutateTopologyAtEnd = true;

        assert(Acquire(mockDev, mockWriter, &mockHasher, summary, cancelled, cfg));
        assert(summary.completed);
        assert(summary.topologyChanged);
    }

    // Scenario 6: Cancellation handling
    {
        MockDeviceSession mockDev;
        MockRawWriter mockWriter;
        MockHasher mockHasher;
        AcquisitionSummary summary;
        std::atomic_bool cancelled{true}; // Cancelled immediately

        mockDev.mockRuns = {{0, 0x1000, 0x10000}};
        mockDev.mockTotalBytes = 0x10000;
        mockDev.mockHighestEnd = 0x11000;

        assert(!Acquire(mockDev, mockWriter, &mockHasher, summary, cancelled, cfg));
        assert(!summary.completed);
    }

    std::cout << "[PASS] Mock acquisition fault injection and state machine tests passed successfully.\n";
    return 0;
}
