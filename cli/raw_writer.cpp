#include "phylaram.hpp"

#include <algorithm>
#include <limits>

namespace {

constexpr uint64_t kDiskHeadroomBytes = 64ull * 1024ull * 1024ull;
constexpr size_t kWriteChunkBytes = 16u * 1024u * 1024u;
constexpr uint64_t kMaxFatFileBytes = 0xFFFFFFFFull;

bool ResolveVolumeRoot(const std::wstring& path,
                       wchar_t (&volumeRoot)[MAX_PATH + 1])
{
    const bool isUnc = path.size() >= 2 && path[0] == L'\\' && path[1] == L'\\';
    if (!isUnc) {
        return GetVolumePathNameW(path.c_str(), volumeRoot, MAX_PATH) != FALSE;
    }

    const size_t serverEnd = path.find(L'\\', 2);
    if (serverEnd == std::wstring::npos) {
        return false;
    }

    const size_t shareEnd = path.find(L'\\', serverEnd + 1);
    const std::wstring root =
        shareEnd == std::wstring::npos
            ? path + L"\\"
            : path.substr(0, shareEnd + 1);

    return wcsncpy_s(volumeRoot, MAX_PATH + 1, root.c_str(), _TRUNCATE) == 0;
}

} // namespace

bool RawWriter::PreflightAndOpen(const std::wstring& partialPath,
                                 uint64_t logicalSize,
                                 uint64_t expectedPhysicalBytes)
{
    Close();
    lastError_ = ERROR_SUCCESS;
    sparse_ = false;
    logicalSize_ = 0;

    if (partialPath.empty() ||
        logicalSize == 0 ||
        logicalSize > static_cast<uint64_t>(std::numeric_limits<LONGLONG>::max()) ||
        expectedPhysicalBytes > logicalSize) {
        lastError_ = ERROR_INVALID_PARAMETER;
        return false;
    }

    wchar_t volumeRoot[MAX_PATH + 1]{};
    if (!ResolveVolumeRoot(partialPath, volumeRoot)) {
        lastError_ = GetLastError();
        if (lastError_ == ERROR_SUCCESS) {
            lastError_ = ERROR_INVALID_NAME;
        }
        return false;
    }

    DWORD fileSystemFlags = 0;
    DWORD maxComponentLength = 0;
    wchar_t fileSystemName[MAX_PATH + 1]{};
    if (!GetVolumeInformationW(
            volumeRoot,
            nullptr,
            0,
            nullptr,
            &maxComponentLength,
            &fileSystemFlags,
            fileSystemName,
            MAX_PATH)) {
        lastError_ = GetLastError();
        return false;
    }

    const bool supportsSparse =
        (fileSystemFlags & FILE_SUPPORTS_SPARSE_FILES) != 0;
    const std::wstring fileSystem(fileSystemName);
    if ((fileSystem == L"FAT32" || fileSystem == L"FAT") &&
        logicalSize > kMaxFatFileBytes) {
        lastError_ = ERROR_FILE_SYSTEM_LIMITATION;
        return false;
    }

    const uint64_t payloadBytes = supportsSparse
                                      ? expectedPhysicalBytes
                                      : logicalSize;
    if (payloadBytes > UINT64_MAX - kDiskHeadroomBytes) {
        lastError_ = ERROR_ARITHMETIC_OVERFLOW;
        return false;
    }

    ULARGE_INTEGER freeBytesAvailable{};
    if (!GetDiskFreeSpaceExW(
            volumeRoot,
            &freeBytesAvailable,
            nullptr,
            nullptr)) {
        lastError_ = GetLastError();
        return false;
    }

    const uint64_t requiredBytes = payloadBytes + kDiskHeadroomBytes;
    if (freeBytesAvailable.QuadPart < requiredBytes) {
        lastError_ = ERROR_DISK_FULL;
        return false;
    }

    file_.Reset(CreateFileW(
        partialPath.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL,
        nullptr));
    if (!file_) {
        lastError_ = GetLastError();
        return false;
    }

    if (supportsSparse) {
        DWORD bytesReturned = 0;
        if (!DeviceIoControl(
                file_.Get(),
                FSCTL_SET_SPARSE,
                nullptr,
                0,
                nullptr,
                0,
                &bytesReturned,
                nullptr)) {
            lastError_ = GetLastError();
            Close();
            DeleteFileW(partialPath.c_str());
            return false;
        }
        sparse_ = true;
    }

    LARGE_INTEGER endPosition{};
    endPosition.QuadPart = static_cast<LONGLONG>(logicalSize);
    if (!SetFilePointerEx(file_.Get(), endPosition, nullptr, FILE_BEGIN) ||
        !SetEndOfFile(file_.Get())) {
        lastError_ = GetLastError();
        Close();
        DeleteFileW(partialPath.c_str());
        return false;
    }

    logicalSize_ = logicalSize;
    return true;
}

bool RawWriter::WriteAt(uint64_t offset, const uint8_t* data, size_t length)
{
    if (!file_) {
        lastError_ = ERROR_INVALID_HANDLE;
        return false;
    }
    if (data == nullptr && length != 0) {
        lastError_ = ERROR_INVALID_PARAMETER;
        return false;
    }

    const uint64_t lengthBytes = static_cast<uint64_t>(length);
    if (offset > logicalSize_ || lengthBytes > logicalSize_ - offset) {
        lastError_ = ERROR_INVALID_PARAMETER;
        return false;
    }

    LARGE_INTEGER position{};
    position.QuadPart = static_cast<LONGLONG>(offset);
    if (!SetFilePointerEx(file_.Get(), position, nullptr, FILE_BEGIN)) {
        lastError_ = GetLastError();
        return false;
    }

    size_t writtenTotal = 0;
    while (writtenTotal < length) {
        const DWORD requested = static_cast<DWORD>(
            std::min(length - writtenTotal, kWriteChunkBytes));
        DWORD written = 0;
        if (!WriteFile(
                file_.Get(),
                data + writtenTotal,
                requested,
                &written,
                nullptr) ||
            written == 0) {
            lastError_ = GetLastError();
            return false;
        }
        writtenTotal += written;
    }

    return true;
}

bool RawWriter::FlushAndClose()
{
    if (!file_) {
        lastError_ = ERROR_INVALID_HANDLE;
        return false;
    }

    if (!FlushFileBuffers(file_.Get())) {
        lastError_ = GetLastError();
        file_.Reset();
        logicalSize_ = 0;
        return false;
    }

    file_.Reset();
    logicalSize_ = 0;
    return true;
}

void RawWriter::Close()
{
    file_.Reset();
    logicalSize_ = 0;
    sparse_ = false;
}
