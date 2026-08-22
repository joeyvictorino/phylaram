#include "phylaram.hpp"
#include <algorithm>
#include <iostream>
#include <chrono>
#include <thread>

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
    uint64_t highestEnd = 0;
    uint64_t totalBytes = 0;
    std::vector<MemoryRun> runs;

    if (!device.Query(highestEnd, totalBytes, runs)) {
        return false;
    }

    // Query live Ring 0 forensic telemetry (CR3, KPCR, NT Base, VBS)
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
    uint64_t lastPercent = UINT64_MAX;
    auto transferStartTime = std::chrono::steady_clock::now();
    uint64_t bytesTransferredSinceStart = 0;

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
                bytesTransferredSinceStart += rr.copied;

                // Apply bandwidth throttling / rate limiting if configured
                if (config.rateLimitMBps > 0) {
                    double targetBytesPerSec = static_cast<double>(config.rateLimitMBps) * 1024.0 * 1024.0;
                    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - transferStartTime).count();
                    double expectedMs = (static_cast<double>(bytesTransferredSinceStart) / targetBytesPerSec) * 1000.0;
                    if (expectedMs > static_cast<double>(elapsedMs)) {
                        auto sleepDurationMs = static_cast<int64_t>(expectedMs - static_cast<double>(elapsedMs));
                        if (sleepDurationMs > 0 && sleepDurationMs < 5000) {
                            std::this_thread::sleep_for(std::chrono::milliseconds(sleepDurationMs));
                        }
                    }
                }
            }

            if (rr.copied == wanted) {
                // Fast path: full transfer completed.
            } else {
                // Isolate the unreadable span at 4 KiB page granularity across the failed window.
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
                        bytesTransferredSinceStart += page.copied;

                        if (page.copied == pageRemainder) {
                            // Page read succeeded; break out of isolation loop to resume 16 MiB fast path.
                            break;
                        }
                        continue;
                    }

                    // Page read failed (0 bytes copied)
                    AddUnreadable(summary, physical, pageRemainder, page.copyStatus);
                    processedPhysical += pageRemainder;
                    offset += pageRemainder;
                    // Do NOT break; continue scanning the remainder of the failed window at 4 KiB granularity.
                }
            }

            if (!config.quiet && summary.physicalBytes != 0) {
                uint64_t percent = (processedPhysical * 100) / summary.physicalBytes;
                if (percent != lastPercent) {
                    std::wcout << L"\rAcquiring... " << percent << L"%" << std::flush;
                    lastPercent = percent;
                }
            }
        }
    }

    if (!config.quiet) {
        std::wcout << L"\rAcquiring... 100%\n";
    }

    if (!hashZerosTo(summary.logicalSize)) {
        return false;
    }

    bool changed = false;
    if (!device.End(changed)) {
        return false;
    }

    summary.topologyChanged = changed;
    summary.completed = true;
    return true;
}
