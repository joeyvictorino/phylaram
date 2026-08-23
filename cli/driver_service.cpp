#include "phylaram.hpp"

#include <chrono>
#include <sstream>
#include <thread>

namespace {

constexpr DWORD kServiceTransitionTimeoutMs = 5000;
constexpr auto kServicePollInterval = std::chrono::milliseconds(50);
constexpr int kDeleteFileAttempts = 50;
constexpr auto kDeleteFileRetryInterval = std::chrono::milliseconds(100);

std::wstring FormatDriverStartError(DWORD error)
{
    std::wostringstream message;
    message << L"Error " << error << L" (0x" << std::hex << error
            << std::dec << L"): ";

    switch (error) {
    case ERROR_INVALID_IMAGE_HASH:
        message << L"Windows rejected the kernel-driver signature. "
                << L"The alpha build is test-signed and belongs only in a dedicated test environment configured for test signing. "
                << L"Do not weaken Secure Boot or code-integrity policy on an evidence system.";
        break;
    case ERROR_DRIVER_BLOCKED:
        message << L"Windows blocked the driver under the active code-integrity policy. "
                << L"Use a production Microsoft-signed driver when testing with HVCI/Memory Integrity enabled.";
        break;
    case ERROR_ACCESS_DENIED:
        message << L"Administrator privileges are required, or endpoint policy denied driver loading.";
        break;
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
        message << L"The Service Control Manager could not open the extracted driver path.";
        break;
    default: {
        wchar_t* systemMessage = nullptr;
        const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER |
                            FORMAT_MESSAGE_FROM_SYSTEM |
                            FORMAT_MESSAGE_IGNORE_INSERTS;
        const DWORD length = FormatMessageW(
            flags,
            nullptr,
            error,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            reinterpret_cast<LPWSTR>(&systemMessage),
            0,
            nullptr);
        if (length != 0 && systemMessage != nullptr) {
            message << systemMessage;
            LocalFree(systemMessage);
        } else {
            message << L"Service Control Manager error.";
        }
        break;
    }
    }

    return message.str();
}

bool WaitForServiceState(SC_HANDLE service,
                         DWORD desiredState,
                         DWORD timeoutMilliseconds)
{
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(timeoutMilliseconds);

    while (std::chrono::steady_clock::now() < deadline) {
        SERVICE_STATUS_PROCESS status{};
        DWORD requiredBytes = 0;
        if (!QueryServiceStatusEx(
                service,
                SC_STATUS_PROCESS_INFO,
                reinterpret_cast<LPBYTE>(&status),
                sizeof(status),
                &requiredBytes)) {
            return false;
        }

        if (status.dwCurrentState == desiredState) {
            return true;
        }

        std::this_thread::sleep_for(kServicePollInterval);
    }

    return false;
}

bool StopServiceIfRunning(SC_HANDLE service)
{
    SERVICE_STATUS_PROCESS status{};
    DWORD requiredBytes = 0;
    if (!QueryServiceStatusEx(
            service,
            SC_STATUS_PROCESS_INFO,
            reinterpret_cast<LPBYTE>(&status),
            sizeof(status),
            &requiredBytes)) {
        return false;
    }

    if (status.dwCurrentState == SERVICE_STOPPED) {
        return true;
    }

    SERVICE_STATUS ignored{};
    if (!ControlService(service, SERVICE_CONTROL_STOP, &ignored)) {
        const DWORD error = GetLastError();
        if (error != ERROR_SERVICE_NOT_ACTIVE) {
            return false;
        }
    }

    return WaitForServiceState(
        service,
        SERVICE_STOPPED,
        kServiceTransitionTimeoutMs);
}

bool WaitUntilServiceIsDeleted(SC_HANDLE scm)
{
    for (int attempt = 0; attempt < 50; ++attempt) {
        ScopedServiceHandle probe(OpenServiceW(
            scm,
            PHYLA_SERVICE_NAME,
            SERVICE_QUERY_STATUS));
        if (!probe) {
            return GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST;
        }
        std::this_thread::sleep_for(kDeleteFileRetryInterval);
    }
    return false;
}

bool RemoveExistingService(SC_HANDLE scm)
{
    ScopedServiceHandle service(OpenServiceW(
        scm,
        PHYLA_SERVICE_NAME,
        SERVICE_STOP | DELETE | SERVICE_QUERY_STATUS));
    if (!service) {
        return GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST;
    }

    if (!StopServiceIfRunning(service.Get())) {
        return false;
    }
    if (!DeleteService(service.Get())) {
        const DWORD error = GetLastError();
        if (error != ERROR_SERVICE_MARKED_FOR_DELETE) {
            return false;
        }
    }

    service.Reset();
    return WaitUntilServiceIsDeleted(scm);
}

void BestEffortDeleteService(SC_HANDLE service) noexcept
{
    StopServiceIfRunning(service);
    DeleteService(service);
}

DWORD DeleteExtractedDriver(const std::wstring& path) noexcept
{
    if (path.empty()) {
        return ERROR_SUCCESS;
    }

    for (int attempt = 0; attempt < kDeleteFileAttempts; ++attempt) {
        if (DeleteFileW(path.c_str())) {
            return ERROR_SUCCESS;
        }

        const DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND) {
            return ERROR_SUCCESS;
        }
        if (error != ERROR_SHARING_VIOLATION &&
            error != ERROR_ACCESS_DENIED) {
            return error;
        }

        std::this_thread::sleep_for(kDeleteFileRetryInterval);
    }

    return GetLastError();
}

} // namespace

bool InstallAndStartDriver(const std::wstring& sysPath,
                           std::wstring& errorText)
{
    errorText.clear();

    ScopedServiceHandle scm(OpenSCManagerW(
        nullptr,
        nullptr,
        SC_MANAGER_CONNECT | SC_MANAGER_CREATE_SERVICE));
    if (!scm) {
        const DWORD error = GetLastError();
        errorText = L"OpenSCManagerW failed: " + FormatDriverStartError(error);
        return false;
    }

    if (!RemoveExistingService(scm.Get())) {
        errorText = L"A previous PhylaRAM service instance could not be stopped and deleted safely.";
        return false;
    }

    ScopedServiceHandle service(CreateServiceW(
        scm.Get(),
        PHYLA_SERVICE_NAME,
        PHYLA_SERVICE_NAME,
        SERVICE_START | SERVICE_STOP | DELETE | SERVICE_QUERY_STATUS,
        SERVICE_KERNEL_DRIVER,
        SERVICE_DEMAND_START,
        SERVICE_ERROR_NORMAL,
        sysPath.c_str(),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr));
    if (!service) {
        const DWORD error = GetLastError();
        errorText = L"CreateServiceW failed: " + FormatDriverStartError(error);
        return false;
    }

    if (!StartServiceW(service.Get(), 0, nullptr)) {
        const DWORD error = GetLastError();
        if (error != ERROR_SERVICE_ALREADY_RUNNING) {
            BestEffortDeleteService(service.Get());
            errorText = L"StartServiceW failed: " + FormatDriverStartError(error);
            return false;
        }
    }

    if (!WaitForServiceState(
            service.Get(),
            SERVICE_RUNNING,
            kServiceTransitionTimeoutMs)) {
        BestEffortDeleteService(service.Get());
        errorText = L"The driver service did not reach SERVICE_RUNNING before the timeout.";
        return false;
    }

    return true;
}

DWORD StopAndDeleteDriver() noexcept
{
    ScopedServiceHandle scm(OpenSCManagerW(
        nullptr,
        nullptr,
        SC_MANAGER_CONNECT));
    if (!scm) {
        return GetLastError();
    }

    ScopedServiceHandle service(OpenServiceW(
        scm.Get(),
        PHYLA_SERVICE_NAME,
        SERVICE_STOP | DELETE | SERVICE_QUERY_STATUS));
    if (!service) {
        const DWORD error = GetLastError();
        return error == ERROR_SERVICE_DOES_NOT_EXIST
                   ? ERROR_SUCCESS
                   : error;
    }

    if (!StopServiceIfRunning(service.Get())) {
        return GetLastError();
    }

    if (!DeleteService(service.Get())) {
        const DWORD error = GetLastError();
        if (error != ERROR_SERVICE_MARKED_FOR_DELETE) {
            return error;
        }
    }

    return ERROR_SUCCESS;
}

bool DriverRuntime::Start(std::wstring& errorText)
{
    errorText.clear();
    if (serviceStarted_ || !extractedDriverPath_.empty()) {
        errorText = L"Driver runtime is already active.";
        return false;
    }

    std::wstring extractedPath;
    if (!ExtractEmbeddedDriver(extractedPath)) {
        errorText = L"Failed to extract the embedded PhylaRAM driver into the protected staging directory.";
        return false;
    }

    if (!InstallAndStartDriver(extractedPath, errorText)) {
        DeleteExtractedDriver(extractedPath);
        return false;
    }

    extractedDriverPath_ = std::move(extractedPath);
    serviceStarted_ = true;
    return true;
}

DWORD DriverRuntime::Stop() noexcept
{
    DWORD firstError = ERROR_SUCCESS;

    if (serviceStarted_) {
        firstError = StopAndDeleteDriver();
        serviceStarted_ = false;
    }

    const DWORD fileError = DeleteExtractedDriver(extractedDriverPath_);
    extractedDriverPath_.clear();

    if (firstError != ERROR_SUCCESS) {
        return firstError;
    }
    return fileError;
}
