#include "phylaram.hpp"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <thread>

namespace {

constexpr uint64_t kMebibyte = 1024ull * 1024ull;
constexpr auto kCancellationPollingInterval = std::chrono::milliseconds(100);

bool CheckedAdd(uint64_t& value, uint64_t increment)
{
    if (increment > UINT64_MAX - value) {
        return false;
    }
    value += increment;
    return true;
}

bool AddUnreadable(AcquisitionSummary& summary,
                   uint64_t start,
                   uint64_t length,
                   long status)
{
    if (length == 0 || !CheckedAdd(summary.unreadableBytes, length)) {
        return false;
    }

    if (!summary.unreadable.empty()) {
        UnreadableSpan& previous = summary.unreadable.back();
        if (previous.start <= UINT64_MAX - previous.length &&
            previous.start + previous.length == start &&
            previous.status == status &&
            previous.length <= UINT64_MAX - length) {
            previous.length += length;
            return true;
        }
    }

    summary.unreadable.push_back({start, length, status});
    return true;
}

bool ValidateReadResult(const ReadResult& result,
                        uint64_t expectedPhysicalAddress,
                        uint32_t requestedBytes)
{
    return result.physicalAddress == expectedPhysicalAddress &&
           result.requested == requestedBytes &&
           result.copied <= requestedBytes &&
           result.data.size() == result.copied;
}

bool PaceTransfer(uint64_t bytesTransferred,
                  uint32_t rateLimitMBps,
                  std::chrono::steady_clock::time_point startedAt,
                  std::atomic_bool& cancelled)
{
    if (rateLimitMBps == 0) {
        return true;
    }

    const long double bytesPerSecond =
        static_cast<long double>(rateLimitMBps) *
        static_cast<long double>(kMebibyte);
    const std::chrono::duration<long double> targetElapsed(
        static_cast<long double>(bytesTransferred) / bytesPerSecond);
    const auto targetTime = startedAt +
        std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            targetElapsed);

    while (std::chrono::steady_clock::now() < targetTime) {
        if (cancelled.load()) {
            return false;
        }

        const auto remaining = targetTime - std::chrono::steady_clock::now();
        std::this_thread::sleep_for(
            std::min(remaining,
                     std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                         kCancellationPollingInterval)));
    }

    return !cancelled.load();
}

uint32_t ClampEtaSeconds(long double seconds)
{
    if (seconds <= 0.0L) {
        return 0;
    }
    const long double maximum =
        static_cast<long double>(std::numeric_limits<uint32_t>::max());
    return static_cast<uint32_t>(std::min(seconds, maximum));
}

} // namespace

bool Acquire(IDeviceSession& device,
             IRawWriter& writer,
             IHasher& hasher,
             AcquisitionSummary& summary,
             std::atomic_bool& cancelled,
             const AcquisitionConfig& config)
{
    summary = AcquisitionSummary{};

    uint64_t highestPhysicalEnd = 0;
    uint64_t totalPhysicalBytes = 0;
    std::vector<MemoryRun> runs;
    if (!device.Query(highestPhysicalEnd, totalPhysicalBytes, runs) ||
        highestPhysicalEnd == 0 ||
        totalPhysicalBytes == 0 ||
        runs.empty()) {
        return false;
    }

    KernelHints hints;
    if (device.QueryHints(hints)) {
        summary.hints = hints;
    }

    summary.logicalSize = highestPhysicalEnd;
    summary.physicalBytes = totalPhysicalBytes;
    summary.ranges = runs;

    uint64_t hashPosition = 0;
    auto hashZerosTo = [&](uint64_t nextPhysicalAddress) -> bool {
        if (nextPhysicalAddress < hashPosition) {
            return false;
        }
        if (nextPhysicalAddress > hashPosition &&
            !hasher.UpdateZeros(nextPhysicalAddress - hashPosition)) {
            return false;
        }
        hashPosition = nextPhysicalAddress;
        return true;
    };

    uint64_t processedPhysicalBytes = 0;
    uint64_t transferredBytes = 0;
    uint64_t lastPercent = UINT64_MAX;
    int64_t lastProgressMilliseconds = -1;
    const auto transferStartedAt = std::chrono::steady_clock::now();

    for (const MemoryRun& run : runs) {
        if (run.length == 0 || run.base > UINT64_MAX - run.length) {
            return false;
        }

        uint64_t offsetWithinRun = 0;
        while (offsetWithinRun < run.length) {
            if (cancelled.load()) {
                return false;
            }

            const uint32_t requestedBytes = static_cast<uint32_t>(
                std::min<uint64_t>(
                    PHYLA_MAX_TRANSFER,
                    run.length - offsetWithinRun));
            const uint64_t expectedPhysicalAddress =
                run.base + offsetWithinRun;

            ReadResult result;
            if (!device.Read(
                    run.driverIndex,
                    offsetWithinRun,
                    requestedBytes,
                    result) ||
                !ValidateReadResult(
                    result,
                    expectedPhysicalAddress,
                    requestedBytes)) {
                return false;
            }

            if (result.copied != 0) {
                if (!hashZerosTo(result.physicalAddress) ||
                    !writer.WriteAt(
                        result.physicalAddress,
                        result.data.data(),
                        result.copied) ||
                    !hasher.Update(result.data.data(), result.copied) ||
                    !CheckedAdd(hashPosition, result.copied) ||
                    !CheckedAdd(offsetWithinRun, result.copied) ||
                    !CheckedAdd(processedPhysicalBytes, result.copied) ||
                    !CheckedAdd(summary.acquiredBytes, result.copied) ||
                    !CheckedAdd(transferredBytes, result.copied) ||
                    !PaceTransfer(
                        transferredBytes,
                        config.rateLimitMBps,
                        transferStartedAt,
                        cancelled)) {
                    return false;
                }
            }

            if (result.copied != requestedBytes) {
                const uint64_t unreadableWindowBytes =
                    static_cast<uint64_t>(requestedBytes - result.copied);
                if (offsetWithinRun > UINT64_MAX - unreadableWindowBytes) {
                    return false;
                }

                const uint64_t chunkEnd = std::min(
                    run.length,
                    offsetWithinRun + unreadableWindowBytes);

                while (offsetWithinRun < chunkEnd) {
                    if (cancelled.load()) {
                        return false;
                    }

                    const uint64_t physicalAddress =
                        run.base + offsetWithinRun;
                    const uint64_t bytesToPageBoundary =
                        PHYLA_PAGE_SIZE -
                        (physicalAddress & (PHYLA_PAGE_SIZE - 1));
                    const uint32_t pageRequestBytes = static_cast<uint32_t>(
                        std::min<uint64_t>(
                            bytesToPageBoundary,
                            chunkEnd - offsetWithinRun));

                    ReadResult pageResult;
                    if (!device.Read(
                            run.driverIndex,
                            offsetWithinRun,
                            pageRequestBytes,
                            pageResult) ||
                        !ValidateReadResult(
                            pageResult,
                            physicalAddress,
                            pageRequestBytes)) {
                        return false;
                    }

                    if (pageResult.copied != 0) {
                        if (!hashZerosTo(pageResult.physicalAddress) ||
                            !writer.WriteAt(
                                pageResult.physicalAddress,
                                pageResult.data.data(),
                                pageResult.copied) ||
                            !hasher.Update(
                                pageResult.data.data(),
                                pageResult.copied) ||
                            !CheckedAdd(hashPosition, pageResult.copied) ||
                            !CheckedAdd(offsetWithinRun, pageResult.copied) ||
                            !CheckedAdd(
                                processedPhysicalBytes,
                                pageResult.copied) ||
                            !CheckedAdd(
                                summary.acquiredBytes,
                                pageResult.copied) ||
                            !CheckedAdd(transferredBytes, pageResult.copied) ||
                            !PaceTransfer(
                                transferredBytes,
                                config.rateLimitMBps,
                                transferStartedAt,
                                cancelled)) {
                            return false;
                        }

                        if (pageResult.copied == pageRequestBytes) {
                            break;
                        }
                        continue;
                    }

                    if (!AddUnreadable(
                            summary,
                            physicalAddress,
                            pageRequestBytes,
                            pageResult.copyStatus) ||
                        !CheckedAdd(
                            processedPhysicalBytes,
                            pageRequestBytes) ||
                        !CheckedAdd(offsetWithinRun, pageRequestBytes)) {
                        return false;
                    }
                }
            }

            if (summary.physicalBytes != 0) {
                const uint64_t percent = static_cast<uint64_t>(
                    (static_cast<long double>(processedPhysicalBytes) * 100.0L) /
                    static_cast<long double>(summary.physicalBytes));
                const auto now = std::chrono::steady_clock::now();
                const int64_t elapsedMilliseconds =
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        now - transferStartedAt)
                        .count();

                if (percent != lastPercent ||
                    elapsedMilliseconds - lastProgressMilliseconds >= 250) {
                    lastPercent = percent;
                    lastProgressMilliseconds = elapsedMilliseconds;

                    const long double elapsedSeconds =
                        elapsedMilliseconds > 0
                            ? static_cast<long double>(elapsedMilliseconds) /
                                  1000.0L
                            : 0.0L;
                    const long double speedMBps =
                        elapsedSeconds > 0.0L
                            ? (static_cast<long double>(transferredBytes) /
                               static_cast<long double>(kMebibyte)) /
                                  elapsedSeconds
                            : 0.0L;
                    const uint64_t remainingBytes =
                        summary.physicalBytes > processedPhysicalBytes
                            ? summary.physicalBytes - processedPhysicalBytes
                            : 0;
                    const long double etaSeconds =
                        speedMBps > 0.0L
                            ? (static_cast<long double>(remainingBytes) /
                               static_cast<long double>(kMebibyte)) /
                                  speedMBps
                            : 0.0L;
                    const uint32_t eta = ClampEtaSeconds(etaSeconds);

                    if (config.onProgress != nullptr) {
                        config.onProgress(
                            processedPhysicalBytes,
                            summary.physicalBytes,
                            static_cast<double>(speedMBps),
                            eta,
                            config.callbackData);
                    }

                    if (!config.quiet) {
                        const uint32_t etaMinutes = eta / 60;
                        const uint32_t etaRemainderSeconds = eta % 60;
                        std::wostringstream progress;
                        progress << L"\rAcquiring: " << std::setw(3) << percent
                                 << L"% ["
                                 << (processedPhysicalBytes / kMebibyte)
                                 << L" / "
                                 << (summary.physicalBytes / kMebibyte)
                                 << L" MiB] [" << std::fixed
                                 << std::setprecision(1)
                                 << static_cast<double>(speedMBps)
                                 << L" MiB/s] [ETA: " << std::setfill(L'0')
                                 << std::setw(2) << etaMinutes << L":"
                                 << std::setw(2) << etaRemainderSeconds
                                 << L"]   ";
                        std::wcout << progress.str() << std::flush;
                    }
                }
            }
        }
    }

    if (!hashZerosTo(summary.logicalSize)) {
        return false;
    }

    bool topologyChanged = false;
    if (!device.End(topologyChanged)) {
        return false;
    }

    summary.topologyChanged = topologyChanged;
    summary.completed = true;

    if (!config.quiet) {
        std::wcout << L"\n";
    }
    return true;
}
