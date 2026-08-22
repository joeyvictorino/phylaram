#include "phylaram.hpp"
#include <sstream>
#include <chrono>
#include <thread>

static std::wstring FormatDriverStartError(DWORD err)
{
    std::wostringstream ss;
    ss << L"Error " << err << L" (0x" << std::hex << err << std::dec << L"): ";
    switch (err) {
    case 577: // ERROR_INVALID_IMAGE_HASH
        ss << L"ERROR_INVALID_IMAGE_HASH\n"
           << L"  -> Windows kernel signature check rejected the driver.\n"
           << L"  -> Resolution for Pre-Release:\n"
           << L"     1. Ensure Secure Boot is DISABLED in UEFI/BIOS/VM settings.\n"
           << L"     2. Run 'install_test_cert.bat' as Administrator.\n"
           << L"     3. Run 'bcdedit /set testsigning on' as Administrator and reboot.";
        break;
    case 1275: // ERROR_DRIVER_BLOCKED
        ss << L"ERROR_DRIVER_BLOCKED\n"
           << L"  -> Driver blocked by Windows Memory Integrity (HVCI) or Microsoft Vulnerable Driver Blocklist.\n"
           << L"  -> Temporarily disable Memory Integrity in Windows Security > Core Isolation for testing.";
        break;
    case 5: // ERROR_ACCESS_DENIED
        ss << L"ERROR_ACCESS_DENIED\n"
           << L"  -> Administrator privileges required or an active security agent blocked driver loading.";
        break;
    case 2: // ERROR_FILE_NOT_FOUND
    case 3: // ERROR_PATH_NOT_FOUND
        ss << L"ERROR_FILE_NOT_FOUND\n"
           << L"  -> The driver binary path could not be found or opened by the Service Control Manager.";
        break;
    default: {
        wchar_t* msgBuf = nullptr;
        DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
        DWORD len = FormatMessageW(flags, nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                   reinterpret_cast<LPWSTR>(&msgBuf), 0, nullptr);
        if (len > 0 && msgBuf != nullptr) {
            ss << msgBuf;
            LocalFree(msgBuf);
        } else {
            ss << L"Service Control Manager error.";
        }
        break;
    }
    }
    return ss.str();
}

static bool WaitForServiceState(SC_HANDLE service, DWORD desiredState, DWORD timeoutMs)
{
    auto start = std::chrono::steady_clock::now();
    SERVICE_STATUS_PROCESS ssp{};
    DWORD needed = 0;

    while (true) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed >= timeoutMs) {
            break;
        }

        if (!QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO,
                                  reinterpret_cast<LPBYTE>(&ssp), sizeof(ssp), &needed)) {
            return false;
        }

        if (ssp.dwCurrentState == desiredState) {
            return true;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    return false;
}

static void RemoveExistingService(SC_HANDLE scm)
{
    ScopedServiceHandle service(OpenServiceW(scm, PHYLA_SERVICE_NAME,
                                            SERVICE_STOP | DELETE | SERVICE_QUERY_STATUS));
    if (!service) {
        return;
    }

    SERVICE_STATUS status{};
    ControlService(service.Get(), SERVICE_CONTROL_STOP, &status);
    WaitForServiceState(service.Get(), SERVICE_STOPPED, 5000);
    DeleteService(service.Get());
    service.Reset();

    for (int i = 0; i < 50; ++i) {
        ScopedServiceHandle probe(OpenServiceW(scm, PHYLA_SERVICE_NAME, SERVICE_QUERY_STATUS));
        if (!probe && GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

bool InstallAndStartDriver(const std::wstring& sysPath, std::wstring& errorText)
{
    errorText.clear();

    ScopedServiceHandle scm(OpenSCManagerW(nullptr, nullptr,
                                          SC_MANAGER_CONNECT | SC_MANAGER_CREATE_SERVICE));
    if (!scm) {
        errorText = L"OpenSCManagerW failed (Error: " + std::to_wstring(GetLastError()) + L")";
        return false;
    }

    RemoveExistingService(scm.Get());

    // NOTE: For SERVICE_KERNEL_DRIVER, MSDN specifies lpBinaryPathName must NOT be enclosed in quotes.
    ScopedServiceHandle service(CreateServiceW(
        scm.Get(),
        PHYLA_SERVICE_NAME,
        PHYLA_SERVICE_NAME,
        SERVICE_START | SERVICE_STOP | DELETE | SERVICE_QUERY_STATUS,
        SERVICE_KERNEL_DRIVER,
        SERVICE_DEMAND_START,
        SERVICE_ERROR_NORMAL,
        sysPath.c_str(),
        nullptr, nullptr, nullptr, nullptr, nullptr));

    if (!service) {
        DWORD err = GetLastError();
        errorText = L"CreateServiceW failed: " + FormatDriverStartError(err);
        return false;
    }

    if (!StartServiceW(service.Get(), 0, nullptr)) {
        DWORD err = GetLastError();
        if (err != ERROR_SERVICE_ALREADY_RUNNING) {
            errorText = L"StartServiceW failed: " + FormatDriverStartError(err);
            return false;
        }
    }

    if (!WaitForServiceState(service.Get(), SERVICE_RUNNING, 5000)) {
        errorText = L"Driver failed to reach SERVICE_RUNNING state";
        return false;
    }

    return true;
}

void StopAndDeleteDriver()
{
    ScopedServiceHandle scm(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
    if (!scm) {
        return;
    }

    ScopedServiceHandle service(OpenServiceW(scm.Get(), PHYLA_SERVICE_NAME,
                                            SERVICE_STOP | DELETE | SERVICE_QUERY_STATUS));
    if (service) {
        SERVICE_STATUS status{};
        ControlService(service.Get(), SERVICE_CONTROL_STOP, &status);
        WaitForServiceState(service.Get(), SERVICE_STOPPED, 5000);
        DeleteService(service.Get());
    }
}
