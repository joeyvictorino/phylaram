#pragma once

#include <windows.h>
#include <bcrypt.h>
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

#include "../shared/interfaces.hpp"
#include "../shared/phylaram.h"

template <typename HandleType, typename DeleterType, HandleType InvalidValue = nullptr>
class UniqueWin32Handle {
public:
    explicit UniqueWin32Handle(HandleType handle = InvalidValue) noexcept
        : handle_(handle) {}

    ~UniqueWin32Handle() noexcept { Reset(); }

    UniqueWin32Handle(const UniqueWin32Handle&) = delete;
    UniqueWin32Handle& operator=(const UniqueWin32Handle&) = delete;

    UniqueWin32Handle(UniqueWin32Handle&& other) noexcept
        : handle_(other.Release()) {}

    UniqueWin32Handle& operator=(UniqueWin32Handle&& other) noexcept
    {
        if (this != &other) {
            Reset(other.Release());
        }
        return *this;
    }

    [[nodiscard]] HandleType Get() const noexcept { return handle_; }
    [[nodiscard]] explicit operator bool() const noexcept
    {
        return handle_ != InvalidValue;
    }

    [[nodiscard]] HandleType Release() noexcept
    {
        const HandleType handle = handle_;
        handle_ = InvalidValue;
        return handle;
    }

    void Reset(HandleType replacement = InvalidValue) noexcept
    {
        if (handle_ != InvalidValue) {
            DeleterType{}(handle_);
        }
        handle_ = replacement;
    }

private:
    HandleType handle_;
};

struct FileHandleDeleter {
    void operator()(HANDLE handle) const noexcept
    {
        if (handle != INVALID_HANDLE_VALUE && handle != nullptr) {
            CloseHandle(handle);
        }
    }
};

struct ServiceHandleDeleter {
    void operator()(SC_HANDLE handle) const noexcept
    {
        if (handle != nullptr) {
            CloseServiceHandle(handle);
        }
    }
};

struct SidDeleter {
    void operator()(PSID sid) const noexcept
    {
        if (sid != nullptr) {
            FreeSid(sid);
        }
    }
};

using ScopedHandle =
    UniqueWin32Handle<HANDLE, FileHandleDeleter, INVALID_HANDLE_VALUE>;
using ScopedServiceHandle =
    UniqueWin32Handle<SC_HANDLE, ServiceHandleDeleter, nullptr>;
using ScopedSid = UniqueWin32Handle<PSID, SidDeleter, nullptr>;

class DeviceSession final : public IDeviceSession {
public:
    DeviceSession();
    ~DeviceSession() override;

    DeviceSession(const DeviceSession&) = delete;
    DeviceSession& operator=(const DeviceSession&) = delete;
    DeviceSession(DeviceSession&&) noexcept = default;
    DeviceSession& operator=(DeviceSession&&) noexcept = default;

    bool Open() override;
    void Close() override;
    bool Query(uint64_t& highestEnd,
               uint64_t& totalBytes,
               std::vector<MemoryRun>& runs) override;
    bool QueryHints(KernelHints& hints) override;
    bool Read(uint32_t runIndex,
              uint64_t offset,
              uint32_t length,
              ReadResult& result) override;
    bool End(bool& topologyChanged) override;

    [[nodiscard]] uint32_t LastError() const noexcept override
    {
        return lastError_;
    }

private:
    ScopedHandle handle_;
    DWORD lastError_ = ERROR_SUCCESS;
    std::vector<uint8_t> ioBuffer_;
};

class RawWriter final : public IRawWriter {
public:
    RawWriter() = default;
    ~RawWriter() override { Close(); }

    RawWriter(const RawWriter&) = delete;
    RawWriter& operator=(const RawWriter&) = delete;
    RawWriter(RawWriter&&) noexcept = default;
    RawWriter& operator=(RawWriter&&) noexcept = default;

    bool PreflightAndOpen(const std::wstring& partialPath,
                          uint64_t logicalSize,
                          uint64_t expectedPhysicalBytes) override;
    bool WriteAt(uint64_t offset,
                 const uint8_t* data,
                 size_t length) override;
    bool FlushAndClose() override;
    void Close() override;

    [[nodiscard]] bool IsSparse() const noexcept override { return sparse_; }
    [[nodiscard]] uint32_t LastError() const noexcept override
    {
        return lastError_;
    }

private:
    ScopedHandle file_;
    uint64_t logicalSize_ = 0;
    bool sparse_ = false;
    DWORD lastError_ = ERROR_SUCCESS;
};

class Sha256 final : public IHasher {
public:
    Sha256() = default;
    ~Sha256() override { Reset(); }

    Sha256(const Sha256&) = delete;
    Sha256& operator=(const Sha256&) = delete;
    Sha256(Sha256&& other) noexcept;
    Sha256& operator=(Sha256&& other) noexcept;

    bool Initialize() override;
    bool Update(const uint8_t* data, size_t length) override;
    bool UpdateZeros(uint64_t length) override;
    bool Finish(std::string& hex) override;
    void Reset() override;

private:
    BCRYPT_ALG_HANDLE algorithm_ = nullptr;
    BCRYPT_HASH_HANDLE hash_ = nullptr;
    bool initialized_ = false;
};

using ProgressCallback = void (*)(uint64_t acquiredBytes,
                                  uint64_t totalBytes,
                                  double speedMBs,
                                  uint32_t etaSeconds,
                                  void* userData);

struct AcquisitionConfig {
    bool quiet = false;
    uint32_t rateLimitMBps = 0;
    ProgressCallback onProgress = nullptr;
    void* callbackData = nullptr;
};

enum class EvidenceCaptureStatus {
    Complete,
    Incomplete,
    Cancelled,
    Failed,
};

struct EvidenceCaptureResult {
    EvidenceCaptureStatus status = EvidenceCaptureStatus::Failed;
    AcquisitionSummary summary;
    std::wstring error;
    DWORD systemError = ERROR_SUCCESS;

    [[nodiscard]] bool HasFinalizedBundle() const noexcept
    {
        return status == EvidenceCaptureStatus::Complete ||
               status == EvidenceCaptureStatus::Incomplete;
    }
};

class DriverRuntime final {
public:
    DriverRuntime() = default;
    ~DriverRuntime() { (void)Stop(); }

    DriverRuntime(const DriverRuntime&) = delete;
    DriverRuntime& operator=(const DriverRuntime&) = delete;
    DriverRuntime(DriverRuntime&&) = delete;
    DriverRuntime& operator=(DriverRuntime&&) = delete;

    bool Start(std::wstring& errorText);
    [[nodiscard]] DWORD Stop() noexcept;

private:
    std::wstring extractedDriverPath_;
    bool serviceStarted_ = false;
};

bool ExtractEmbeddedDriver(std::wstring& driverPathOut);
bool InstallAndStartDriver(const std::wstring& sysPath, std::wstring& errorText);
[[nodiscard]] DWORD StopAndDeleteDriver() noexcept;

bool IsSupportedWindows(std::wstring& reason);
bool IsAdministrator();

bool WriteMapJson(const std::wstring& path,
                  const AcquisitionSummary& summary);
bool WriteSha256Sidecar(const std::wstring& path,
                        const std::wstring& rawFileName,
                        const std::string& sha256);
bool PromoteStagingFile(const std::wstring& stagingPath,
                        const std::wstring& finalPath);

bool Acquire(IDeviceSession& device,
             IRawWriter& writer,
             IHasher& hasher,
             AcquisitionSummary& summary,
             std::atomic_bool& cancelled,
             const AcquisitionConfig& config);

EvidenceCaptureResult CaptureEvidenceToFile(
    IDeviceSession& device,
    const std::wstring& outputPath,
    std::atomic_bool& cancelled,
    const AcquisitionConfig& config);

int LaunchGui(HINSTANCE instance);
