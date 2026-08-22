#include "phylaram.hpp"
#include <sstream>
#include <chrono>
#include <thread>

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

    // Enclose binary path in quotes to eliminate Unquoted Service Path Vulnerability (CWE-428)
    std::wstring quotedPath = L"\"" + sysPath + L"\"";

    ScopedServiceHandle service(CreateServiceW(
        scm.Get(),
        PHYLA_SERVICE_NAME,
        PHYLA_SERVICE_NAME,
        SERVICE_START | SERVICE_STOP | DELETE | SERVICE_QUERY_STATUS,
        SERVICE_KERNEL_DRIVER,
        SERVICE_DEMAND_START,
        SERVICE_ERROR_NORMAL,
        quotedPath.c_str(),
        nullptr, nullptr, nullptr, nullptr, nullptr));

    if (!service) {
        DWORD err = GetLastError();
        errorText = L"CreateServiceW failed (Error: " + std::to_wstring(err) + L")";
        return false;
    }

    if (!StartServiceW(service.Get(), 0, nullptr)) {
        DWORD err = GetLastError();
        if (err != ERROR_SERVICE_ALREADY_RUNNING) {
            errorText = L"StartServiceW failed (Error: " + std::to_wstring(err) + L")";
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
