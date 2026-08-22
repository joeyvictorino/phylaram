#include "phylaram.hpp"
#include <iostream>
#include <vector>
#include <set>
#include <sstream>

static void DiscoverRegistryPagingFiles(std::set<std::wstring>& outPaths)
{
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                      L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Memory Management",
                      0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD type = 0;
        DWORD bytesNeeded = 0;
        if (RegQueryValueExW(hKey, L"PagingFiles", nullptr, &type, nullptr, &bytesNeeded) == ERROR_SUCCESS && bytesNeeded > 0) {
            std::vector<wchar_t> buffer(bytesNeeded / sizeof(wchar_t) + 2, L'\0');
            if (RegQueryValueExW(hKey, L"PagingFiles", nullptr, &type,
                                 reinterpret_cast<LPBYTE>(buffer.data()), &bytesNeeded) == ERROR_SUCCESS) {
                const wchar_t* p = buffer.data();
                while (*p) {
                    std::wstring entry(p);
                    // PagingFiles entry format: "<path> <min_size> <max_size>"
                    size_t space = entry.find(L' ');
                    std::wstring filePath = (space != std::wstring::npos) ? entry.substr(0, space) : entry;
                    if (!filePath.empty() && filePath.size() >= 3 && filePath[1] == L':') {
                        outPaths.insert(filePath);
                    }
                    p += wcslen(p) + 1;
                }
            }
        }
        RegCloseKey(hKey);
    }
}

static void DiscoverVolumeFiles(std::set<std::wstring>& outPaths)
{
    // Scan all active drive letters from C: to Z:
    for (wchar_t letter = L'C'; letter <= L'Z'; ++letter) {
        std::wstring drive = std::wstring(1, letter) + L":\\";
        UINT driveType = GetDriveTypeW(drive.c_str());
        if (driveType == DRIVE_FIXED || driveType == DRIVE_REMOVABLE) {
            outPaths.insert(std::wstring(1, letter) + L":\\pagefile.sys");
            outPaths.insert(std::wstring(1, letter) + L":\\swapfile.sys");
            outPaths.insert(std::wstring(1, letter) + L":\\hiberfil.sys");
        }
    }
}

bool CapturePagefiles(const std::wstring& outputBase, std::vector<std::wstring>& capturedFiles)
{
    capturedFiles.clear();
    std::set<std::wstring> candidates;

    DiscoverRegistryPagingFiles(candidates);
    DiscoverVolumeFiles(candidates);

    for (const auto& candidate : candidates) {
        DWORD attrs = GetFileAttributesW(candidate.c_str());
        if (attrs == INVALID_FILE_ATTRIBUTES) {
            continue;
        }

        // Determine unique destination file name (e.g. memory.raw.C_pagefile.sys)
        wchar_t driveLetter = (candidate.size() >= 2 && candidate[1] == L':') ? candidate[0] : L'_';
        std::wstring baseName = candidate.substr(candidate.find_last_of(L'\\') + 1);
        std::wstring destPath = outputBase + L"." + driveLetter + L"_" + baseName;

        ScopedHandle src(CreateFileW(candidate.c_str(),
                                     GENERIC_READ,
                                     FILE_SHARE_READ | FILE_SHARE_WRITE,
                                     nullptr,
                                     OPEN_EXISTING,
                                     FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS,
                                     nullptr));

        if (!src) {
            continue;
        }

        ScopedHandle dst(CreateFileW(destPath.c_str(),
                                     GENERIC_WRITE,
                                     0,
                                     nullptr,
                                     CREATE_ALWAYS,
                                     FILE_ATTRIBUTE_NORMAL,
                                     nullptr));
        if (!dst) {
            continue;
        }

        std::vector<uint8_t> buffer(4 * 1024 * 1024); // 4 MiB transfer buffer
        DWORD bytesRead = 0;
        DWORD bytesWritten = 0;
        bool copyOk = true;

        while (ReadFile(src.Get(), buffer.data(), static_cast<DWORD>(buffer.size()), &bytesRead, nullptr) && bytesRead > 0) {
            if (!WriteFile(dst.Get(), buffer.data(), bytesRead, &bytesWritten, nullptr) || bytesWritten != bytesRead) {
                copyOk = false;
                break;
            }
        }

        if (copyOk) {
            FlushFileBuffers(dst.Get());
            capturedFiles.push_back(destPath);
        } else {
            dst.Reset();
            DeleteFileW(destPath.c_str());
        }
    }

    return !capturedFiles.empty();
}
