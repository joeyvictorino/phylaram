#include "phylaram.hpp"
#include <iostream>
#include <filesystem>
#include <atomic>
#include <winternl.h>

static std::atomic_bool g_cancelled{false};

static bool PathExists(const std::wstring& path)
{
    return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

static BOOL WINAPI ConsoleHandler(DWORD type)
{
    if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT || type == CTRL_CLOSE_EVENT) {
        g_cancelled.store(true);
        return TRUE;
    }
    return FALSE;
}

bool IsAdministrator()
{
    BOOL isMember = FALSE;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    ScopedSid adminGroup;
    PSID rawSid = nullptr;

    if (AllocateAndInitializeSid(&ntAuthority, 2,
                                 SECURITY_BUILTIN_DOMAIN_RID,
                                 DOMAIN_ALIAS_RID_ADMINS,
                                 0, 0, 0, 0, 0, 0,
                                 &rawSid)) {
        adminGroup.Reset(rawSid);
        CheckTokenMembership(nullptr, adminGroup.Get(), &isMember);
    }
    return isMember != FALSE;
}

bool IsSupportedWindows(std::wstring& reason)
{
    using RtlGetVersionFn = LONG (WINAPI*)(PRTL_OSVERSIONINFOW);
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) {
        reason = L"Unable to locate ntdll.dll.";
        return false;
    }

    auto rtlGetVersion = reinterpret_cast<RtlGetVersionFn>(GetProcAddress(ntdll, "RtlGetVersion"));
    if (!rtlGetVersion) {
        reason = L"Unable to query Windows version.";
        return false;
    }

    RTL_OSVERSIONINFOW v{};
    v.dwOSVersionInfoSize = sizeof(v);
    if (rtlGetVersion(&v) != 0) {
        reason = L"Unable to determine Windows version.";
        return false;
    }

    if (v.dwMajorVersion < 10 || (v.dwMajorVersion == 10 && v.dwBuildNumber < 19041)) {
        reason = L"PhylaRAM requires Windows 10 version 2004 (build 19041) or later, or Windows 11.";
        return false;
    }
    return true;
}

static void Usage()
{
    std::wcout << L"PhylaRAM 0.1.0-alpha - Live physical-memory acquisition for Windows\n\n"
               << L"Usage:\n"
               << L"  phylaram.exe <output.raw | -> [options]\n"
               << L"  phylaram.exe --dry-run [--json]\n\n"
               << L"Arguments:\n"
               << L"  <output.raw>        Target raw image destination (supports UNC paths, e.g. \\\\server\\share\\mem.raw)\n"
               << L"  -                   Stream raw physical memory directly to standard output (stdout)\n\n"
               << L"Options:\n"
               << L"  --dry-run           Inspect physical memory topology and kernel hints without writing to disk\n"
               << L"  --json              Format dry-run or telemetry output as structured JSON\n"
               << L"  --rate-limit <MB>   Throttle maximum acquisition bandwidth in MB/s (e.g. --rate-limit 200)\n"
               << L"  --quiet             Suppress statistics and interactive progress output\n"
               << L"  --no-hash           Skip logical SHA-256 calculation and .sha256 sidecar\n"
               << L"  --help, -h          Display this help message\n";
}

int wmain(int argc, wchar_t** argv)
{
    if (argc < 2) {
        Usage();
        return 1;
    }

    bool quiet = false;
    bool hashEnabled = true;
    bool dryRun = false;
    bool jsonOutput = false;
    uint32_t rateLimitMBps = 0;
    std::wstring output;

    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (arg == L"--quiet") {
            quiet = true;
        } else if (arg == L"--no-hash") {
            hashEnabled = false;
        } else if (arg == L"--dry-run") {
            dryRun = true;
        } else if (arg == L"--json") {
            jsonOutput = true;
        } else if (arg == L"--rate-limit" || arg == L"--throttle") {
            if (i + 1 < argc) {
                rateLimitMBps = static_cast<uint32_t>(_wtoi(argv[++i]));
            } else {
                Usage();
                return 1;
            }
        } else if (arg == L"--help" || arg == L"-h") {
            Usage();
            return 0;
        } else if (!arg.empty() && arg[0] == L'-' && arg != L"-") {
            Usage();
            return 1;
        } else if (output.empty()) {
            output = arg;
        } else {
            Usage();
            return 1;
        }
    }

    if (output.empty() && !dryRun) {
        Usage();
        return 1;
    }

    bool isStdout = (output == L"-");
    if (isStdout || jsonOutput) {
        quiet = true; // Automatically suppress interactive text progress
    }

    // Preflight collision reservation for all 6 target and staging file paths
    const std::wstring rawFinal = output;
    const std::wstring rawPartial = isStdout ? L"-" : output + L".partial";
    const std::wstring mapFinal = output + L".map.json";
    const std::wstring mapPartial = output + L".map.json.partial";
    const std::wstring hashFinal = output + L".sha256";
    const std::wstring hashPartial = output + L".sha256.partial";

    if (!isStdout && !dryRun) {
        if (PathExists(rawFinal) || PathExists(rawPartial) ||
            PathExists(mapFinal) || PathExists(mapPartial) ||
            (hashEnabled && (PathExists(hashFinal) || PathExists(hashPartial)))) {
            std::wcerr << L"Refusing to overwrite an existing output image or sidecar.\n";
            return 1;
        }
    }

    if (!IsAdministrator()) {
        std::wcerr << L"PhylaRAM must run elevated (Administrator privileges required).\n";
        return 1;
    }

    std::wstring reason;
    if (!IsSupportedWindows(reason)) {
        std::wcerr << reason << L"\n";
        return 1;
    }

    SetConsoleCtrlHandler(ConsoleHandler, TRUE);

    std::wstring driverPath;
    if (!ExtractEmbeddedDriver(driverPath)) {
        std::wcerr << L"Failed to extract embedded driver.\n";
        return 1;
    }

    std::wstring serviceError;
    if (!InstallAndStartDriver(driverPath, serviceError)) {
        std::wcerr << L"Failed to load driver: " << serviceError << L"\n";
        DeleteFileW(driverPath.c_str());
        return 1;
    }

    int exitCode = 1;
    DeviceSession device;
    RawWriter writer;
    AcquisitionSummary summary;
    AcquisitionConfig config{quiet, rateLimitMBps};

    do {
        if (!device.Open()) {
            std::wcerr << L"Unable to open \\\\.\\PhylaRAM. Error " << device.LastError() << L"\n";
            break;
        }

        uint64_t highestEnd = 0;
        uint64_t totalBytes = 0;
        std::vector<MemoryRun> runs;
        if (!device.Query(highestEnd, totalBytes, runs)) {
            std::wcerr << L"Unable to query memory layout. Error " << device.LastError() << L"\n";
            break;
        }

        if (dryRun) {
            device.QueryHints(summary.hints);
            if (jsonOutput) {
                std::wcout << L"{\n"
                           << L"  \"dry_run\": true,\n"
                           << L"  \"logical_size\": " << highestEnd << L",\n"
                           << L"  \"physical_bytes\": " << totalBytes << L",\n"
                           << L"  \"range_count\": " << runs.size() << L",\n"
                           << L"  \"kernel_hints\": {\n"
                           << L"    \"hypervisor_present\": " << (summary.hints.hypervisorPresent ? L"true" : L"false") << L",\n"
                           << L"    \"directory_table_base\": \"0x" << std::hex << std::uppercase << summary.hints.directoryTableBase << std::dec << L"\",\n"
                           << L"    \"kpcr_address\": \"0x" << std::hex << std::uppercase << summary.hints.kpcrAddress << std::dec << L"\",\n"
                           << L"    \"kernel_base\": \"0x" << std::hex << std::uppercase << summary.hints.kernelBase << std::dec << L"\",\n"
                           << L"    \"kernel_size\": " << summary.hints.kernelSize << L",\n"
                           << L"    \"build_number\": " << summary.hints.buildNumber << L"\n"
                           << L"  }\n"
                           << L"}\n";
            } else {
                std::wcout << L"PhylaRAM 0.1.0-alpha — Dry-Run Topology & Kernel Hints\n\n"
                           << L"Physical RAM    : " << (totalBytes / (1024ull * 1024ull)) << L" MiB (" << totalBytes << L" bytes)\n"
                           << L"Highest Address : 0x" << std::hex << std::uppercase << highestEnd << std::dec << L"\n"
                           << L"Memory Ranges   : " << runs.size() << L"\n";
                if (summary.hints.available) {
                    std::wcout << L"System DTB (CR3): 0x" << std::hex << std::uppercase << summary.hints.directoryTableBase << std::dec << L"\n"
                               << L"Executing KPCR  : 0x" << std::hex << std::uppercase << summary.hints.kpcrAddress << std::dec << L"\n"
                               << L"Kernel Base     : 0x" << std::hex << std::uppercase << summary.hints.kernelBase << std::dec << L"\n"
                               << L"Kernel Size     : " << summary.hints.kernelSize << L" bytes\n"
                               << L"Windows Build   : " << summary.hints.majorVersion << L"." << summary.hints.minorVersion << L"." << summary.hints.buildNumber << L"\n"
                               << L"Hypervisor      : " << (summary.hints.hypervisorPresent ? L"Present" : L"None") << L"\n";
                }
                std::wcout << L"\n[PASS] Dry-run completed successfully. Zero bytes written to disk.\n";
            }
            exitCode = 0;
            break;
        }

        if (!quiet && !isStdout) {
            std::wcout << L"PhylaRAM 0.1.0-alpha — Live RAM Capture for Windows\n"
                       << L"Physical memory : " << (totalBytes / (1024ull * 1024ull)) << L" MiB\n"
                       << L"Ranges          : " << runs.size() << L"\n"
                       << L"Output          : " << output << L"\n";
            if (rateLimitMBps > 0) {
                std::wcout << L"Bandwidth Limit : " << rateLimitMBps << L" MB/s\n";
            }
            std::wcout << L"\n";
        }

        if (!writer.PreflightAndOpen(rawPartial, highestEnd, totalBytes)) {
            std::wcerr << L"Unable to create output image. Error " << writer.LastError() << L"\n";
            break;
        }

        Sha256 sha;
        Sha256* shaPtr = nullptr;
        if (hashEnabled) {
            if (!sha.Initialize()) {
                std::wcerr << L"Unable to initialize SHA-256 engine.\n";
                break;
            }
            shaPtr = &sha;
        }

        if (!Acquire(device, writer, shaPtr, summary, g_cancelled, config)) {
            std::wcerr << (g_cancelled.load() ? L"Acquisition cancelled.\n" : L"Acquisition failed.\n");
            break;
        }

        if (hashEnabled && !sha.Finish(summary.sha256)) {
            std::wcerr << L"Failed to finalize SHA-256.\n";
            break;
        }

        if (!writer.FlushAndClose()) {
            std::wcerr << L"Failed to flush output image. Error " << writer.LastError() << L"\n";
            break;
        }

        if (!isStdout) {
            // Write and promote provenance map sidecar
            if (!WriteMapJson(mapPartial, summary)) {
                std::wcerr << L"Failed to write provenance map sidecar.\n";
                break;
            }
            if (!PromoteStagingFile(mapPartial, mapFinal)) {
                std::wcerr << L"Failed to promote provenance map sidecar.\n";
                DeleteFileW(mapPartial.c_str());
                break;
            }

            // Write and promote SHA-256 sidecar
            if (hashEnabled) {
                std::filesystem::path p(output);
                if (!WriteSha256Sidecar(hashPartial, p.filename().wstring(), summary.sha256)) {
                    std::wcerr << L"Failed to write SHA-256 sidecar.\n";
                    DeleteFileW(mapFinal.c_str());
                    break;
                }
                if (!PromoteStagingFile(hashPartial, hashFinal)) {
                    std::wcerr << L"Failed to promote SHA-256 sidecar.\n";
                    DeleteFileW(hashPartial.c_str());
                    DeleteFileW(mapFinal.c_str());
                    break;
                }
            }

            // Promote RAW image only after valid completion and sidecar finalization
            if (!PromoteStagingFile(rawPartial, rawFinal)) {
                std::wcerr << L"Failed to rename completed image. Error " << GetLastError() << L"\n";
                DeleteFileW(mapFinal.c_str());
                if (hashEnabled) {
                    DeleteFileW(hashFinal.c_str());
                }
                break;
            }

        }

        if (!quiet && !isStdout) {
            std::wcout << L"Acquired        : " << summary.acquiredBytes << L" bytes\n"
                       << L"Unreadable      : " << summary.unreadableBytes << L" bytes\n"
                       << L"Topology changed: " << (summary.topologyChanged ? L"Yes" : L"No") << L"\n";
            if (summary.hints.available) {
                std::wcout << L"Kernel Base     : 0x" << std::hex << summary.hints.kernelBase << std::dec << L"\n"
                           << L"Directory Base  : 0x" << std::hex << summary.hints.directoryTableBase << std::dec << L"\n";
            }
            if (hashEnabled) {
                std::wcout << L"SHA-256         : "
                           << std::wstring(summary.sha256.begin(), summary.sha256.end()) << L"\n";
            }
        }

        if (summary.topologyChanged || summary.unreadableBytes != 0) {
            if (!isStdout) {
                std::wcout << L"INCOMPLETE: acquisition reached the end but one or more integrity conditions were not perfect.\n";
            }
            exitCode = 2;
        } else {
            if (!isStdout) {
                std::wcout << L"Complete.\n";
            }
            exitCode = 0;
        }
    } while (false);

    device.Close();
    writer.Close();
    StopAndDeleteDriver();

    for (int i = 0; i < 20; ++i) {
        if (DeleteFileW(driverPath.c_str()) || GetLastError() == ERROR_FILE_NOT_FOUND) {
            break;
        }
        Sleep(100);
    }

    return exitCode;
}
