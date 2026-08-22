#include "phylaram.hpp"
#include <algorithm>
#include <io.h>
#include <fcntl.h>

bool RawWriter::PreflightAndOpen(const std::wstring& partialPath,
                                 uint64_t logicalSize,
                                 uint64_t expectedPhysicalBytes)
{
    Close();
    lastError_ = ERROR_SUCCESS;
    isStdout_ = false;
    logicalSize_ = logicalSize;
    currentStreamOffset_ = 0;

    // Support stdout streaming when "-" is specified
    if (partialPath == L"-") {
        isStdout_ = true;
        sparse_ = false;
        HANDLE stdOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (stdOut == INVALID_HANDLE_VALUE || stdOut == nullptr) {
            lastError_ = ERROR_INVALID_HANDLE;
            return false;
        }
        // Set stdout to binary mode
        _setmode(_fileno(stdout), _O_BINARY);
        file_.Reset(stdOut);
        return true;
    }

    wchar_t volumeRoot[MAX_PATH + 1]{};
    bool isUnc = (partialPath.size() >= 2 && partialPath[0] == L'\\' && partialPath[1] == L'\\');

    if (isUnc) {
        // Extract \\server\share\ for UNC paths
        size_t firstSlash = partialPath.find(L'\\', 2);
        if (firstSlash != std::wstring::npos) {
            size_t secondSlash = partialPath.find(L'\\', firstSlash + 1);
            if (secondSlash != std::wstring::npos) {
                wcsncpy_s(volumeRoot, MAX_PATH, partialPath.substr(0, secondSlash + 1).c_str(), _TRUNCATE);
            } else {
                wcsncpy_s(volumeRoot, MAX_PATH, (partialPath + L"\\").c_str(), _TRUNCATE);
            }
        }
    } else {
        if (!GetVolumePathNameW(partialPath.c_str(), volumeRoot, MAX_PATH)) {
            lastError_ = GetLastError();
            return false;
        }
    }

    DWORD fileSystemFlags = 0;
    DWORD maxComponentLength = 0;
    wchar_t fsName[MAX_PATH + 1]{};
    bool supportsSparse = false;

    if (volumeRoot[0] != L'\0' && GetVolumeInformationW(volumeRoot, nullptr, 0, nullptr,
                                                        &maxComponentLength, &fileSystemFlags,
                                                        fsName, MAX_PATH)) {
        supportsSparse = (fileSystemFlags & FILE_SUPPORTS_SPARSE_FILES) != 0;
        std::wstring fsNameStr(fsName);

        // Enforce FAT32 4 GiB - 1 single-file limit
        if (fsNameStr == L"FAT32" || fsNameStr == L"FAT") {
            const uint64_t kMaxFat32FileSize = 0xFFFFFFFFULL;
            if (logicalSize > kMaxFat32FileSize) {
                lastError_ = ERROR_FILE_SYSTEM_LIMITATION;
                return false;
            }
        }

        const uint64_t kHeadroom = 64ull * 1024ull * 1024ull; // 64 MiB headroom
        uint64_t requiredSpace = supportsSparse ? expectedPhysicalBytes : logicalSize;
        if (requiredSpace <= UINT64_MAX - kHeadroom) {
            requiredSpace += kHeadroom;
        }

        ULARGE_INTEGER freeBytesCaller{};
        if (GetDiskFreeSpaceExW(volumeRoot, &freeBytesCaller, nullptr, nullptr)) {
            if (freeBytesCaller.QuadPart < requiredSpace) {
                lastError_ = ERROR_DISK_FULL;
                return false;
            }
        }
    }

    file_.Reset(CreateFileW(partialPath.c_str(),
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
        sparse_ = DeviceIoControl(file_.Get(), FSCTL_SET_SPARSE, nullptr, 0,
                                  nullptr, 0, &bytesReturned, nullptr) != FALSE;
    } else {
        sparse_ = false;
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

    return true;
}

bool RawWriter::WriteAt(uint64_t offset, const uint8_t* data, size_t length)
{
    if (!file_ || (!data && length != 0)) {
        lastError_ = ERROR_INVALID_HANDLE;
        return false;
    }

    if (isStdout_) {
        // For stdout streaming, stream zero-fill gaps if offset is ahead of current stream position
        if (offset > currentStreamOffset_) {
            uint64_t gap = offset - currentStreamOffset_;
            std::vector<uint8_t> zeroChunk(static_cast<size_t>(std::min<uint64_t>(gap, 1024u * 1024u)), 0);
            while (gap > 0) {
                DWORD toWrite = static_cast<DWORD>(std::min<uint64_t>(gap, zeroChunk.size()));
                DWORD written = 0;
                if (!WriteFile(file_.Get(), zeroChunk.data(), toWrite, &written, nullptr) || written == 0) {
                    lastError_ = GetLastError();
                    return false;
                }
                gap -= written;
                currentStreamOffset_ += written;
            }
        }

        size_t done = 0;
        while (done < length) {
            DWORD chunk = static_cast<DWORD>(std::min<size_t>(length - done, 16u * 1024u * 1024u));
            DWORD written = 0;
            if (!WriteFile(file_.Get(), data + done, chunk, &written, nullptr) || written == 0) {
                lastError_ = GetLastError();
                return false;
            }
            done += written;
            currentStreamOffset_ += written;
        }
        return true;
    }

    LARGE_INTEGER pos{};
    pos.QuadPart = static_cast<LONGLONG>(offset);
    if (!SetFilePointerEx(file_.Get(), pos, nullptr, FILE_BEGIN)) {
        lastError_ = GetLastError();
        return false;
    }

    size_t done = 0;
    while (done < length) {
        DWORD chunk = static_cast<DWORD>(std::min<size_t>(length - done, 16u * 1024u * 1024u));
        DWORD written = 0;
        if (!WriteFile(file_.Get(), data + done, chunk, &written, nullptr) || written == 0) {
            lastError_ = GetLastError();
            return false;
        }
        done += written;
    }

    return true;
}

bool RawWriter::FlushAndClose()
{
    if (!file_) {
        return false;
    }

    if (isStdout_) {
        // Stream trailing zero bytes up to logicalSize if the stream hasn't reached it
        if (currentStreamOffset_ < logicalSize_) {
            uint64_t gap = logicalSize_ - currentStreamOffset_;
            std::vector<uint8_t> zeroChunk(static_cast<size_t>(std::min<uint64_t>(gap, 1024u * 1024u)), 0);
            while (gap > 0) {
                DWORD toWrite = static_cast<DWORD>(std::min<uint64_t>(gap, zeroChunk.size()));
                DWORD written = 0;
                if (!WriteFile(file_.Get(), zeroChunk.data(), toWrite, &written, nullptr) || written == 0) {
                    break;
                }
                gap -= written;
                currentStreamOffset_ += written;
            }
        }
        file_.Release(); // Do not close standard output handle
        return true;
    }

    bool ok = FlushFileBuffers(file_.Get()) != FALSE;
    if (!ok) {
        lastError_ = GetLastError();
    }

    file_.Reset();
    return ok;
}

void RawWriter::Close()
{
    if (isStdout_) {
        file_.Release();
    } else {
        file_.Reset();
    }
}
