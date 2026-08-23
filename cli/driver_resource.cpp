#include "phylaram.hpp"
#include "resource.h"

#include <aclapi.h>
#include <array>
#include <iomanip>
#include <sddl.h>
#include <sstream>

namespace {

constexpr wchar_t kProtectedAdminDacl[] = L"D:P(A;;GA;;;SY)(A;;GA;;;BA)";

bool ApplyProtectedAdminDacl(const std::wstring& path)
{
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
            kProtectedAdminDacl,
            SDDL_REVISION_1,
            &descriptor,
            nullptr)) {
        return false;
    }

    BOOL daclPresent = FALSE;
    BOOL daclDefaulted = FALSE;
    PACL dacl = nullptr;
    const BOOL gotDacl = GetSecurityDescriptorDacl(
        descriptor,
        &daclPresent,
        &dacl,
        &daclDefaulted);

    DWORD result = ERROR_INVALID_SECURITY_DESCR;
    if (gotDacl && daclPresent && dacl != nullptr) {
        result = SetNamedSecurityInfoW(
            const_cast<LPWSTR>(path.c_str()),
            SE_FILE_OBJECT,
            DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
            nullptr,
            nullptr,
            dacl,
            nullptr);
    }

    LocalFree(descriptor);
    return result == ERROR_SUCCESS;
}

bool CreateSecureDirectory(const std::wstring& directoryPath)
{
    SECURITY_ATTRIBUTES attributes{};
    attributes.nLength = sizeof(attributes);
    attributes.bInheritHandle = FALSE;

    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
            kProtectedAdminDacl,
            SDDL_REVISION_1,
            &attributes.lpSecurityDescriptor,
            nullptr)) {
        return false;
    }

    const BOOL created = CreateDirectoryW(directoryPath.c_str(), &attributes);
    const DWORD createError = GetLastError();
    LocalFree(attributes.lpSecurityDescriptor);

    if (!created && createError != ERROR_ALREADY_EXISTS) {
        return false;
    }

    const DWORD fileAttributes = GetFileAttributesW(directoryPath.c_str());
    if (fileAttributes == INVALID_FILE_ATTRIBUTES ||
        (fileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        return false;
    }

    /*
     * SECURITY: The extraction directory is a privileged trust boundary.  An
     * existing directory is not trusted merely because its name matches ours;
     * reapply the protected SYSTEM/Administrators DACL on every use.
     */
    return ApplyProtectedAdminDacl(directoryPath);
}

std::wstring ResolveExtractionDirectory()
{
    wchar_t programData[MAX_PATH + 1]{};
    const DWORD programDataLength =
        GetEnvironmentVariableW(L"ProgramData", programData, MAX_PATH);

    std::wstring baseDirectory;
    if (programDataLength > 0 && programDataLength <= MAX_PATH) {
        baseDirectory = std::wstring(programData) + L"\\PhylaRAM";
    } else {
        wchar_t systemRoot[MAX_PATH + 1]{};
        const DWORD systemRootLength =
            GetEnvironmentVariableW(L"SystemRoot", systemRoot, MAX_PATH);
        if (systemRootLength == 0 || systemRootLength > MAX_PATH) {
            return {};
        }
        baseDirectory = std::wstring(systemRoot) + L"\\Temp\\PhylaRAM";
    }

    if (!CreateSecureDirectory(baseDirectory)) {
        return {};
    }

    std::wstring tempDirectory = baseDirectory + L"\\Temp";
    if (!CreateSecureDirectory(tempDirectory)) {
        return {};
    }

    return tempDirectory;
}

std::wstring GenerateUnpredictableDriverPath()
{
    const std::wstring directory = ResolveExtractionDirectory();
    if (directory.empty()) {
        return {};
    }

    std::array<unsigned char, 16> randomBytes{};
    const NTSTATUS randomStatus = BCryptGenRandom(
        nullptr,
        randomBytes.data(),
        static_cast<ULONG>(randomBytes.size()),
        BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (randomStatus < 0) {
        return {};
    }

    std::wostringstream path;
    path << directory << L"\\phylaram_drv_";
    for (const unsigned char byte : randomBytes) {
        path << std::hex << std::setw(2) << std::setfill(L'0')
             << static_cast<unsigned int>(byte);
    }
    path << L".sys";
    return path.str();
}

} // namespace

bool ExtractEmbeddedDriver(std::wstring& driverPathOut)
{
    driverPathOut.clear();

    /*
     * SECURITY: Production acquisition has exactly one implicit kernel-code
     * source: the driver embedded into this executable at build time.  An
     * adjacent phylaram.sys is intentionally ignored so replacing a sidecar
     * cannot substitute kernel code under an elevated process.
     */
    const HRSRC resource =
        FindResourceW(nullptr, MAKEINTRESOURCEW(IDR_PHYLA_DRIVER), RT_RCDATA);
    if (resource == nullptr) {
        return false;
    }

    const HGLOBAL loadedResource = LoadResource(nullptr, resource);
    if (loadedResource == nullptr) {
        return false;
    }

    const DWORD resourceSize = SizeofResource(nullptr, resource);
    const void* const resourceBytes = LockResource(loadedResource);
    if (resourceBytes == nullptr || resourceSize == 0) {
        return false;
    }

    const std::wstring driverPath = GenerateUnpredictableDriverPath();
    if (driverPath.empty()) {
        return false;
    }

    SECURITY_ATTRIBUTES attributes{};
    attributes.nLength = sizeof(attributes);
    attributes.bInheritHandle = FALSE;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
            kProtectedAdminDacl,
            SDDL_REVISION_1,
            &attributes.lpSecurityDescriptor,
            nullptr)) {
        return false;
    }

    ScopedHandle file(CreateFileW(
        driverPath.c_str(),
        GENERIC_WRITE,
        0,
        &attributes,
        CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL,
        nullptr));
    LocalFree(attributes.lpSecurityDescriptor);

    if (!file) {
        return false;
    }

    const auto* cursor = static_cast<const unsigned char*>(resourceBytes);
    DWORD remaining = resourceSize;
    while (remaining != 0) {
        DWORD written = 0;
        if (!WriteFile(file.Get(), cursor, remaining, &written, nullptr) ||
            written == 0) {
            file.Reset();
            DeleteFileW(driverPath.c_str());
            return false;
        }
        cursor += written;
        remaining -= written;
    }

    if (!FlushFileBuffers(file.Get())) {
        file.Reset();
        DeleteFileW(driverPath.c_str());
        return false;
    }

    file.Reset();
    driverPathOut = driverPath;
    return true;
}
