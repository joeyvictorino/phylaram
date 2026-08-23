#include "phylaram.hpp"

#include <filesystem>

namespace {

struct EvidencePaths {
    std::wstring rawFinal;
    std::wstring rawPartial;
    std::wstring mapFinal;
    std::wstring mapPartial;
    std::wstring hashFinal;
    std::wstring hashPartial;
};

EvidencePaths BuildEvidencePaths(const std::wstring& outputPath)
{
    return {
        outputPath,
        outputPath + L".partial",
        outputPath + L".map.json",
        outputPath + L".map.json.partial",
        outputPath + L".sha256",
        outputPath + L".sha256.partial",
    };
}

bool PathExists(const std::wstring& path)
{
    return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

void DeleteIfPresent(const std::wstring& path) noexcept
{
    if (!path.empty()) {
        DeleteFileW(path.c_str());
    }
}

void RemoveStagingFiles(const EvidencePaths& paths) noexcept
{
    DeleteIfPresent(paths.rawPartial);
    DeleteIfPresent(paths.mapPartial);
    DeleteIfPresent(paths.hashPartial);
}

void RemoveFinalizedSidecars(const EvidencePaths& paths) noexcept
{
    DeleteIfPresent(paths.mapFinal);
    DeleteIfPresent(paths.hashFinal);
}

EvidenceCaptureResult Failure(EvidenceCaptureStatus status,
                              const std::wstring& message,
                              DWORD systemError = ERROR_SUCCESS)
{
    EvidenceCaptureResult result;
    result.status = status;
    result.error = message;
    result.systemError = systemError;
    return result;
}

bool HasAnyCollision(const EvidencePaths& paths)
{
    return PathExists(paths.rawFinal) ||
           PathExists(paths.rawPartial) ||
           PathExists(paths.mapFinal) ||
           PathExists(paths.mapPartial) ||
           PathExists(paths.hashFinal) ||
           PathExists(paths.hashPartial);
}

} // namespace

EvidenceCaptureResult CaptureEvidenceToFile(
    IDeviceSession& device,
    const std::wstring& outputPath,
    std::atomic_bool& cancelled,
    const AcquisitionConfig& config)
{
    if (outputPath.empty() || outputPath == L"-") {
        return Failure(
            EvidenceCaptureStatus::Failed,
            L"A filesystem evidence path is required. Raw stdout acquisition is not supported because it cannot preserve unreadable-byte provenance.",
            ERROR_INVALID_PARAMETER);
    }

    const EvidencePaths paths = BuildEvidencePaths(outputPath);
    if (HasAnyCollision(paths)) {
        return Failure(
            EvidenceCaptureStatus::Failed,
            L"Refusing to overwrite an existing evidence file or staging file.",
            ERROR_FILE_EXISTS);
    }

    uint64_t highestPhysicalEnd = 0;
    uint64_t totalPhysicalBytes = 0;
    std::vector<MemoryRun> runs;
    if (!device.Query(highestPhysicalEnd, totalPhysicalBytes, runs)) {
        return Failure(
            EvidenceCaptureStatus::Failed,
            L"Unable to query the frozen physical-memory topology.",
            device.LastError());
    }

    Sha256 hasher;
    if (!hasher.Initialize()) {
        return Failure(
            EvidenceCaptureStatus::Failed,
            L"Unable to initialize SHA-256.");
    }

    RawWriter writer;
    if (!writer.PreflightAndOpen(
            paths.rawPartial,
            highestPhysicalEnd,
            totalPhysicalBytes)) {
        return Failure(
            EvidenceCaptureStatus::Failed,
            L"Unable to create and preflight the staged RAW image.",
            writer.LastError());
    }

    EvidenceCaptureResult result;
    if (!Acquire(device, writer, hasher, result.summary, cancelled, config)) {
        writer.Close();
        RemoveStagingFiles(paths);
        result.status = cancelled.load()
                            ? EvidenceCaptureStatus::Cancelled
                            : EvidenceCaptureStatus::Failed;
        result.error = cancelled.load()
                           ? L"Memory acquisition was cancelled."
                           : L"Memory acquisition failed before a complete evidence transaction could be finalized.";
        result.systemError = device.LastError();
        return result;
    }

    if (!hasher.Finish(result.summary.sha256)) {
        writer.Close();
        RemoveStagingFiles(paths);
        return Failure(
            EvidenceCaptureStatus::Failed,
            L"SHA-256 finalization failed; no evidence bundle was published.");
    }

    if (!writer.FlushAndClose()) {
        const DWORD writeError = writer.LastError();
        RemoveStagingFiles(paths);
        return Failure(
            EvidenceCaptureStatus::Failed,
            L"The staged RAW image could not be flushed durably; no evidence bundle was published.",
            writeError);
    }

    if (result.summary.acquiredBytes > UINT64_MAX - result.summary.unreadableBytes ||
        result.summary.acquiredBytes + result.summary.unreadableBytes !=
            result.summary.physicalBytes) {
        RemoveStagingFiles(paths);
        return Failure(
            EvidenceCaptureStatus::Failed,
            L"Acquisition byte accounting violated an internal forensic invariant; no evidence bundle was published.",
            ERROR_INVALID_DATA);
    }

    if (!WriteMapJson(paths.mapPartial, result.summary)) {
        RemoveStagingFiles(paths);
        return Failure(
            EvidenceCaptureStatus::Failed,
            L"The provenance map could not be written durably; no evidence bundle was published.");
    }

    const std::filesystem::path rawPath(outputPath);
    if (!WriteSha256Sidecar(
            paths.hashPartial,
            rawPath.filename().wstring(),
            result.summary.sha256)) {
        RemoveStagingFiles(paths);
        return Failure(
            EvidenceCaptureStatus::Failed,
            L"The SHA-256 sidecar could not be written durably; no evidence bundle was published.");
    }

    /*
     * FORENSIC: Publish sidecars first and the RAW image last.  The canonical
     * RAW filename therefore appears only after every staged component has
     * been flushed.  Known promotion failures remove already-published
     * sidecars so the caller never reports a finalized bundle on partial
     * publication.
     */
    if (!PromoteStagingFile(paths.mapPartial, paths.mapFinal)) {
        const DWORD promotionError = GetLastError();
        RemoveStagingFiles(paths);
        return Failure(
            EvidenceCaptureStatus::Failed,
            L"The provenance map could not be promoted to its final name.",
            promotionError);
    }

    if (!PromoteStagingFile(paths.hashPartial, paths.hashFinal)) {
        const DWORD promotionError = GetLastError();
        RemoveStagingFiles(paths);
        RemoveFinalizedSidecars(paths);
        return Failure(
            EvidenceCaptureStatus::Failed,
            L"The SHA-256 sidecar could not be promoted to its final name.",
            promotionError);
    }

    if (!PromoteStagingFile(paths.rawPartial, paths.rawFinal)) {
        const DWORD promotionError = GetLastError();
        RemoveStagingFiles(paths);
        RemoveFinalizedSidecars(paths);
        return Failure(
            EvidenceCaptureStatus::Failed,
            L"The RAW image could not be promoted to its final name.",
            promotionError);
    }

    result.status =
        result.summary.topologyChanged || result.summary.unreadableBytes != 0
            ? EvidenceCaptureStatus::Incomplete
            : EvidenceCaptureStatus::Complete;
    result.error.clear();
    result.systemError = ERROR_SUCCESS;
    return result;
}
