#pragma once

#include <windows.h>
#include <bcrypt.h>
#include <cstdint>
#include <string>
#include <vector>
#include <atomic>
#include <memory>
#include "../shared/phylaram.h"
#include "../shared/interfaces.hpp"

// Generic Win32 RAII handle wrapper adhering to the Rule of 5
template <typename HandleType, typename DeleterType, HandleType InvalidValue = nullptr>
class UniqueWin32Handle {
public:
    explicit UniqueWin32Handle(HandleType h = InvalidValue) noexcept : handle_(h) {}
    ~UniqueWin32Handle() noexcept { Reset(); }

    UniqueWin32Handle(const UniqueWin32Handle&) = delete;
    UniqueWin32Handle& operator=(const UniqueWin32Handle&) = delete;

    UniqueWin32Handle(UniqueWin32Handle&& other) noexcept : handle_(other.Release()) {}
    UniqueWin32Handle& operator=(UniqueWin32Handle&& other) noexcept {
        if (this != &other) {
            Reset(other.Release());
        }
        return *this;
    }

    HandleType Get() const noexcept { return handle_; }
    explicit operator bool() const noexcept { return handle_ != InvalidValue; }

    HandleType Release() noexcept {
        HandleType h = handle_;
        handle_ = InvalidValue;
        return h;
    }

    void Reset(HandleType h = InvalidValue) noexcept {
        if (handle_ != InvalidValue) {
            DeleterType{}(handle_);
            handle_ = InvalidValue;
        }
        handle_ = h;
    }

private:
    HandleType handle_;
};

struct FileHandleDeleter {
    void operator()(HANDLE h) const noexcept {
        if (h != INVALID_HANDLE_VALUE && h != nullptr) {
            CloseHandle(h);
        }
    }
};

struct ServiceHandleDeleter {
    void operator()(SC_HANDLE h) const noexcept {
        if (h != nullptr) {
            CloseServiceHandle(h);
        }
    }
};

struct SidDeleter {
    void operator()(PSID s) const noexcept {
        if (s != nullptr) {
            FreeSid(s);
        }
    }
};

using ScopedHandle = UniqueWin32Handle<HANDLE, FileHandleDeleter, INVALID_HANDLE_VALUE>;
using ScopedServiceHandle = UniqueWin32Handle<SC_HANDLE, ServiceHandleDeleter, nullptr>;
using ScopedSid = UniqueWin32Handle<PSID, SidDeleter, nullptr>;

class DeviceSession : public IDeviceSession {
public:
    DeviceSession();
    ~DeviceSession() override;
    DeviceSession(const DeviceSession&) = delete;
    DeviceSession& operator=(const DeviceSession&) = delete;
    DeviceSession(DeviceSession&&) noexcept = default;
    DeviceSession& operator=(DeviceSession&&) noexcept = default;

    bool Open() override;
    void Close() override;
    bool Query(uint64_t& highestEnd, uint64_t& totalBytes, std::vector<MemoryRun>& runs) override;
    bool QueryHints(KernelHints& hints) override;
    bool Read(uint32_t runIndex, uint64_t offset, uint32_t length, ReadResult& result) override;
    bool End(bool& topologyChanged) override;
    uint32_t LastError() const noexcept override { return lastError_; }

private:
    ScopedHandle handle_;
    DWORD lastError_ = ERROR_SUCCESS;
    std::vector<uint8_t> ioBuffer_; // Reusable 16 MiB transfer buffer
};

class RawWriter : public IRawWriter {
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
    bool WriteAt(uint64_t offset, const uint8_t* data, size_t length) override;
    bool FlushAndClose() override;
    void Close() override;
    bool IsSparse() const noexcept override { return sparse_; }
    bool IsStdout() const noexcept { return isStdout_; }
    uint32_t LastError() const noexcept override { return lastError_; }

private:
    ScopedHandle file_;
    bool sparse_ = false;
    bool isStdout_ = false;
    uint64_t currentStreamOffset_ = 0;
    DWORD lastError_ = ERROR_SUCCESS;
};

class Sha256 : public IHasher {
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
    BCRYPT_ALG_HANDLE alg_ = nullptr;
    BCRYPT_HASH_HANDLE hash_ = nullptr;
    bool initialized_ = false;
};

struct AcquisitionConfig {
    bool quiet = false;
    uint32_t rateLimitMBps = 0; // 0 = unlimited, >0 = throttle speed in MB/s
};

bool ExtractEmbeddedDriver(std::wstring& driverPathOut);
bool InstallAndStartDriver(const std::wstring& sysPath, std::wstring& errorText);
void StopAndDeleteDriver();
bool IsSupportedWindows(std::wstring& reason);
bool IsAdministrator();
bool WriteMapJson(const std::wstring& path, const AcquisitionSummary& summary);
bool WriteSha256Sidecar(const std::wstring& path, const std::wstring& rawFileName, const std::string& sha256);
bool PromoteStagingFile(const std::wstring& stagingPath, const std::wstring& finalPath);
bool CapturePagefiles(const std::wstring& outputBase, std::vector<std::wstring>& capturedFiles);
bool WriteCrashDumpHeader(HANDLE file, const AcquisitionSummary& summary);
bool Acquire(IDeviceSession& device,
             IRawWriter& writer,
             IHasher* hasher,
             AcquisitionSummary& summary,
             std::atomic_bool& cancelled,
             const AcquisitionConfig& config);
