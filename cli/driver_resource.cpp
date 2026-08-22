#include "phylaram.hpp"
#include "resource.h"
#include <sddl.h>
#include <sstream>
#include <iomanip>
#include <array>

static bool CreateSecureDirectory(const std::wstring& dirPath)
{
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = FALSE;

    // Grant GENERIC_ALL exclusively to SYSTEM (SY) and BUILTIN\Administrators (BA) with protected DACL (D:P)
    const wchar_t* sddl = L"D:P(A;;GA;;;SY)(A;;GA;;;BA)";
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
            sddl, SDDL_REVISION_1, &sa.lpSecurityDescriptor, nullptr)) {
        return false;
    }

    BOOL created = CreateDirectoryW(dirPath.c_str(), &sa);
    DWORD err = GetLastError();
    LocalFree(sa.lpSecurityDescriptor);

    return created || (err == ERROR_ALREADY_EXISTS);
}

static std::wstring GenerateUnpredictableDriverPath()
{
    wchar_t programData[MAX_PATH + 1]{};
    DWORD n = GetEnvironmentVariableW(L"ProgramData", programData, MAX_PATH);
    std::wstring baseDir;

    if (n > 0 && n <= MAX_PATH) {
        baseDir = std::wstring(programData) + L"\\PhylaRAM";
    } else {
        wchar_t systemRoot[MAX_PATH + 1]{};
        DWORD srN = GetEnvironmentVariableW(L"SystemRoot", systemRoot, MAX_PATH);
        if (srN > 0 && srN <= MAX_PATH) {
            baseDir = std::wstring(systemRoot) + L"\\Temp\\PhylaRAM";
        } else {
            return {};
        }
    }

    CreateSecureDirectory(baseDir);
    std::wstring tempDir = baseDir + L"\\Temp";
    if (!CreateSecureDirectory(tempDir)) {
        return {};
    }

    std::array<uint8_t, 16> randomBytes{};
    BCryptGenRandom(nullptr, randomBytes.data(), static_cast<ULONG>(randomBytes.size()),
                    BCRYPT_USE_SYSTEM_PREFERRED_RNG);

    std::wostringstream ss;
    ss << tempDir << L"\\phylaram_drv_";
    for (uint8_t b : randomBytes) {
        ss << std::hex << std::setw(2) << std::setfill(L'0') << static_cast<unsigned int>(b);
    }
    ss << L".sys";

    return ss.str();
}

bool ExtractEmbeddedDriver(std::wstring& driverPathOut)
{
    driverPathOut.clear();

    HRSRC res = FindResourceW(nullptr, MAKEINTRESOURCEW(IDR_PHYLA_DRIVER), RT_RCDATA);
    if (!res) {
        return false;
    }

    HGLOBAL loaded = LoadResource(nullptr, res);
    if (!loaded) {
        return false;
    }

    DWORD size = SizeofResource(nullptr, res);
    const void* bytes = LockResource(loaded);
    if (!bytes || size == 0) {
        return false;
    }

    std::wstring path = GenerateUnpredictableDriverPath();
    if (path.empty()) {
        return false;
    }

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = FALSE;
    const wchar_t* sddl = L"D:P(A;;GA;;;SY)(A;;GA;;;BA)";
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
            sddl, SDDL_REVISION_1, &sa.lpSecurityDescriptor, nullptr)) {
        return false;
    }

    ScopedHandle file(CreateFileW(path.c_str(), GENERIC_WRITE, 0, &sa,
                                  CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
    LocalFree(sa.lpSecurityDescriptor);

    if (!file) {
        return false;
    }

    const uint8_t* p = static_cast<const uint8_t*>(bytes);
    DWORD remaining = size;
    bool writeOk = true;

    while (remaining != 0) {
        DWORD written = 0;
        if (!WriteFile(file.Get(), p, remaining, &written, nullptr) || written == 0) {
            writeOk = false;
            break;
        }
        p += written;
        remaining -= written;
    }

    if (writeOk) {
        writeOk = FlushFileBuffers(file.Get()) != FALSE;
    }

    file.Reset();

    if (!writeOk) {
        DeleteFileW(path.c_str());
        return false;
    }

    driverPathOut = path;
    return true;
}
