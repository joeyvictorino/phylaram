#include "phylaram.hpp"

#include <atomic>
#include <cerrno>
#include <cwctype>
#include <iostream>
#include <limits>
#include <string>
#include <vector>
#include <winternl.h>

namespace {

std::atomic_bool gCancelled{false};

enum class Command {
    Capture,
    DryRun,
    Gui,
    Help,
};

struct CliOptions {
    Command command = Command::Capture;
    std::wstring outputPath;
    bool quiet = false;
    bool json = false;
    uint32_t rateLimitMBps = 0;
    OutputFormat format = OutputFormat::Raw;
    EvidenceMetadata metadata;
};

BOOL WINAPI ConsoleHandler(DWORD eventType)
{
    if (eventType == CTRL_C_EVENT ||
        eventType == CTRL_BREAK_EVENT ||
        eventType == CTRL_CLOSE_EVENT) {
        gCancelled.store(true);
        return TRUE;
    }
    return FALSE;
}

void PrintUsage()
{
    std::wcout
        << L"PhylaRAM 0.1.0-alpha - Live physical-memory acquisition for Windows\n\n"
        << L"Usage:\n"
        << L"  phylaram.exe <output_path> [options]\n"
        << L"  phylaram.exe --dry-run [--json]\n"
        << L"  phylaram.exe --gui\n\n"
        << L"Supported Output Formats (auto-detected by extension or set via --format):\n"
        << L"  .raw                  Flat physical-address-preserving sparse raw image\n"
        << L"  .zdmp, .dmp           Microsoft 64-bit Complete Memory Crash Dump\n"
        << L"  .e01                  Expert Witness Compression Format (EnCase EWF)\n\n"
        << L"Options:\n"
        << L"  --format <fmt>        Explicit format: raw, zdmp, dmp, e01.\n"
        << L"  --case-number <str>   Case identifier (for E01 metadata).\n"
        << L"  --evidence-number <s> Evidence identifier (for E01 metadata).\n"
        << L"  --examiner <str>      Examiner name (for E01 metadata).\n"
        << L"  --description <str>   Evidence description.\n"
        << L"  --notes <str>         Acquisition notes.\n"
        << L"  --rate-limit <MiB/s>  Limit acquisition throughput; 0 means unlimited.\n"
        << L"  --quiet               Suppress interactive acquisition progress.\n"
        << L"  --dry-run             Inspect topology and kernel hints without creating evidence.\n"
        << L"  --json                Emit dry-run output as JSON.\n"
        << L"  --gui                 Launch the native graphical interface.\n"
        << L"  --help, -h            Display this help.\n\n"
        << L"A successful capture always creates evidence, map.json, and SHA-256 sidecars.\n";
}

bool ParseUint32(const std::wstring& text, uint32_t& value)
{
    if (text.empty()) {
        return false;
    }
    for (const wchar_t character : text) {
        if (std::iswdigit(character) == 0) {
            return false;
        }
    }

    errno = 0;
    wchar_t* end = nullptr;
    const unsigned long long parsed = wcstoull(text.c_str(), &end, 10);
    if (errno == ERANGE ||
        end == text.c_str() ||
        *end != L'\0' ||
        parsed > std::numeric_limits<uint32_t>::max()) {
        return false;
    }

    value = static_cast<uint32_t>(parsed);
    return true;
}

bool ParseCommandLine(int argc,
                      wchar_t** argv,
                      CliOptions& options,
                      std::wstring& error)
{
    bool sawGui = false;
    bool sawHelp = false;
    bool sawDryRun = false;

    for (int index = 1; index < argc; ++index) {
        const std::wstring argument = argv[index];

        if (argument == L"--help" || argument == L"-h") {
            sawHelp = true;
            continue;
        }
        if (argument == L"--gui") {
            sawGui = true;
            continue;
        }
        if (argument == L"--dry-run") {
            sawDryRun = true;
            continue;
        }
        if (argument == L"--json") {
            options.json = true;
            continue;
        }
        if (argument == L"--quiet") {
            options.quiet = true;
            continue;
        }
        if (argument == L"--format") {
            if (index + 1 >= argc) {
                error = L"--format requires a format name (raw, zdmp, dmp, e01).";
                return false;
            }
            std::wstring fmt = argv[++index];
            for (auto& c : fmt) {
                c = static_cast<wchar_t>(towlower(c));
            }
            if (fmt == L"raw") {
                options.format = OutputFormat::Raw;
            } else if (fmt == L"zdmp" || fmt == L"dmp") {
                options.format = OutputFormat::Zdmp;
            } else if (fmt == L"e01") {
                options.format = OutputFormat::E01;
            } else {
                error = L"Unknown --format value: " + fmt + L". Supported formats: raw, zdmp, dmp, e01.";
                return false;
            }
            continue;
        }
        if (argument == L"--case-number") {
            if (index + 1 >= argc) {
                error = L"--case-number requires a string argument.";
                return false;
            }
            options.metadata.caseNumber = argv[++index];
            continue;
        }
        if (argument == L"--evidence-number") {
            if (index + 1 >= argc) {
                error = L"--evidence-number requires a string argument.";
                return false;
            }
            options.metadata.evidenceNumber = argv[++index];
            continue;
        }
        if (argument == L"--examiner") {
            if (index + 1 >= argc) {
                error = L"--examiner requires a string argument.";
                return false;
            }
            options.metadata.examiner = argv[++index];
            continue;
        }
        if (argument == L"--description") {
            if (index + 1 >= argc) {
                error = L"--description requires a string argument.";
                return false;
            }
            options.metadata.description = argv[++index];
            continue;
        }
        if (argument == L"--notes") {
            if (index + 1 >= argc) {
                error = L"--notes requires a string argument.";
                return false;
            }
            options.metadata.notes = argv[++index];
            continue;
        }
        if (argument == L"--rate-limit") {
            if (index + 1 >= argc) {
                error = L"--rate-limit requires a non-negative integer MiB/s value.";
                return false;
            }
            uint32_t parsedRate = 0;
            if (!ParseUint32(argv[++index], parsedRate)) {
                error = L"Invalid --rate-limit value.";
                return false;
            }
            options.rateLimitMBps = parsedRate;
            continue;
        }
        if (!argument.empty() && argument.front() == L'-') {
            error = L"Unknown option: " + argument;
            return false;
        }
        if (!options.outputPath.empty()) {
            error = L"Only one evidence output path may be specified.";
            return false;
        }
        options.outputPath = argument;
    }

    const unsigned modeCount = static_cast<unsigned>(sawGui) +
                               static_cast<unsigned>(sawHelp) +
                               static_cast<unsigned>(sawDryRun);
    if (modeCount > 1) {
        error = L"--gui, --help, and --dry-run are mutually exclusive modes.";
        return false;
    }

    if (sawHelp) {
        options.command = Command::Help;
        return true;
    }
    if (sawGui) {
        if (!options.outputPath.empty() ||
            options.json ||
            options.rateLimitMBps != 0 ||
            options.quiet) {
            error = L"--gui does not accept capture-mode options.";
            return false;
        }
        options.command = Command::Gui;
        return true;
    }
    if (sawDryRun) {
        if (!options.outputPath.empty() || options.rateLimitMBps != 0) {
            error = L"--dry-run does not accept an output path or rate limit.";
            return false;
        }
        options.command = Command::DryRun;
        return true;
    }

    if (options.json) {
        error = L"--json is supported only with --dry-run.";
        return false;
    }
    if (options.outputPath.empty()) {
        error = L"An evidence output path is required.";
        return false;
    }
    if (options.outputPath == L"-") {
        error = L"Raw stdout acquisition is not supported because unreadable-byte provenance requires a companion map.";
        return false;
    }

    options.command = Command::Capture;
    return true;
}

void PrintDryRunJson(uint64_t highestPhysicalEnd,
                     uint64_t totalPhysicalBytes,
                     const std::vector<MemoryRun>& runs,
                     const KernelHints& hints,
                     bool topologyChanged)
{
    std::wcout << L"{\n"
               << L"  \"dry_run\": true,\n"
               << L"  \"logical_size\": " << highestPhysicalEnd << L",\n"
               << L"  \"physical_bytes\": " << totalPhysicalBytes << L",\n"
               << L"  \"range_count\": " << runs.size() << L",\n"
               << L"  \"topology_changed\": "
               << (topologyChanged ? L"true" : L"false") << L",\n"
               << L"  \"kernel_hints_available\": "
               << (hints.available ? L"true" : L"false");

    if (hints.available) {
        std::wcout << L",\n  \"kernel_hints\": {\n"
                   << L"    \"hypervisor_present\": "
                   << (hints.hypervisorPresent ? L"true" : L"false") << L",\n"
                   << L"    \"directory_table_base\": \"0x" << std::hex
                   << std::uppercase << hints.directoryTableBase << std::dec << L"\",\n"
                   << L"    \"kpcr_address\": \"0x" << std::hex << std::uppercase
                   << hints.kpcrAddress << std::dec << L"\",\n"
                   << L"    \"kernel_base\": \"0x" << std::hex << std::uppercase
                   << hints.kernelBase << std::dec << L"\",\n"
                   << L"    \"kernel_size\": " << hints.kernelSize << L",\n"
                   << L"    \"build_number\": " << hints.buildNumber << L"\n"
                   << L"  }";
    }

    std::wcout << L"\n}\n";
}

void PrintDryRunText(uint64_t highestPhysicalEnd,
                     uint64_t totalPhysicalBytes,
                     const std::vector<MemoryRun>& runs,
                     const KernelHints& hints,
                     bool topologyChanged)
{
    std::wcout << L"PhylaRAM 0.1.0-alpha - Dry-run topology inspection\n\n"
               << L"Physical RAM    : " << (totalPhysicalBytes / (1024ull * 1024ull))
               << L" MiB (" << totalPhysicalBytes << L" bytes)\n"
               << L"Highest address : 0x" << std::hex << std::uppercase
               << highestPhysicalEnd << std::dec << L"\n"
               << L"Memory ranges   : " << runs.size() << L"\n"
               << L"Topology changed: " << (topologyChanged ? L"Yes" : L"No") << L"\n";

    if (hints.available) {
        std::wcout << L"System DTB      : 0x" << std::hex << std::uppercase
                   << hints.directoryTableBase << std::dec << L"\n"
                   << L"Executing KPCR  : 0x" << std::hex << std::uppercase
                   << hints.kpcrAddress << std::dec << L"\n"
                   << L"Kernel base     : 0x" << std::hex << std::uppercase
                   << hints.kernelBase << std::dec << L"\n"
                   << L"Windows build   : " << hints.buildNumber << L"\n";
    }
}

int RunDryRun(bool json)
{
    std::wstring runtimeError;
    DriverRuntime runtime;
    if (!runtime.Start(runtimeError)) {
        std::wcerr << runtimeError << L"\n";
        return 1;
    }

    DeviceSession device;
    if (!device.Open()) {
        std::wcerr << L"Unable to open \\\\.\\PhylaRAM. Error "
                   << device.LastError() << L"\n";
        (void)runtime.Stop();
        return 1;
    }

    uint64_t highestPhysicalEnd = 0;
    uint64_t totalPhysicalBytes = 0;
    std::vector<MemoryRun> runs;
    KernelHints hints;
    bool topologyChanged = false;

    const bool queryOk = device.Query(
        highestPhysicalEnd,
        totalPhysicalBytes,
        runs);
    const bool hintsOk = device.QueryHints(hints);
    const bool endOk = device.End(topologyChanged);

    device.Close();
    const DWORD cleanupError = runtime.Stop();

    if (!queryOk || !endOk) {
        std::wcerr << L"Dry-run topology inspection failed.\n";
        return 1;
    }
    if (!hintsOk) {
        hints = KernelHints{};
    }
    if (cleanupError != ERROR_SUCCESS) {
        std::wcerr << L"Dry-run completed, but driver cleanup failed with error "
                   << cleanupError << L".\n";
        return 1;
    }

    if (json) {
        PrintDryRunJson(
            highestPhysicalEnd,
            totalPhysicalBytes,
            runs,
            hints,
            topologyChanged);
    } else {
        PrintDryRunText(
            highestPhysicalEnd,
            totalPhysicalBytes,
            runs,
            hints,
            topologyChanged);
    }

    return topologyChanged ? 2 : 0;
}

int RunCapture(const CliOptions& options)
{
    std::wstring runtimeError;
    DriverRuntime runtime;
    if (!runtime.Start(runtimeError)) {
        std::wcerr << runtimeError << L"\n";
        return 1;
    }

    DeviceSession device;
    if (!device.Open()) {
        std::wcerr << L"Unable to open \\\\.\\PhylaRAM. Error "
                   << device.LastError() << L"\n";
        (void)runtime.Stop();
        return 1;
    }

    AcquisitionConfig config;
    config.quiet = options.quiet;
    config.rateLimitMBps = options.rateLimitMBps;
    config.format = options.format;
    config.metadata = options.metadata;

    gCancelled.store(false);
    EvidenceCaptureResult result = CaptureEvidenceToFile(
        device,
        options.outputPath,
        gCancelled,
        config);

    device.Close();
    const DWORD cleanupError = runtime.Stop();

    if (result.HasFinalizedBundle()) {
        if (!options.quiet) {
            std::wcout << L"Acquired bytes  : " << result.summary.acquiredBytes << L"\n"
                       << L"Unreadable bytes: " << result.summary.unreadableBytes << L"\n"
                       << L"Topology changed: "
                       << (result.summary.topologyChanged ? L"Yes" : L"No") << L"\n"
                       << L"SHA-256         : "
                       << std::wstring(
                              result.summary.sha256.begin(),
                              result.summary.sha256.end())
                       << L"\n";
        }

        if (cleanupError != ERROR_SUCCESS) {
            std::wcerr << L"Evidence bundle was finalized, but driver cleanup failed with error "
                       << cleanupError << L".\n";
            return 1;
        }

        if (result.status == EvidenceCaptureStatus::Incomplete) {
            std::wcout << L"INCOMPLETE: the evidence bundle is finalized, but unreadable memory or a topology change was recorded.\n";
            return 2;
        }

        std::wcout << L"Complete. Run phylaram-verify for independent offline verification.\n";
        return 0;
    }

    if (!result.error.empty()) {
        std::wcerr << result.error;
        if (result.systemError != ERROR_SUCCESS) {
            std::wcerr << L" Error " << result.systemError << L".";
        }
        std::wcerr << L"\n";
    }
    if (cleanupError != ERROR_SUCCESS) {
        std::wcerr << L"Driver cleanup also failed with error "
                   << cleanupError << L".\n";
    }
    return 1;
}

} // namespace

bool IsAdministrator()
{
    SID_IDENTIFIER_AUTHORITY authority = SECURITY_NT_AUTHORITY;
    PSID rawSid = nullptr;
    if (!AllocateAndInitializeSid(
            &authority,
            2,
            SECURITY_BUILTIN_DOMAIN_RID,
            DOMAIN_ALIAS_RID_ADMINS,
            0,
            0,
            0,
            0,
            0,
            0,
            &rawSid)) {
        return false;
    }

    ScopedSid administrators(rawSid);
    BOOL isMember = FALSE;
    if (!CheckTokenMembership(nullptr, administrators.Get(), &isMember)) {
        return false;
    }
    return isMember != FALSE;
}

bool IsSupportedWindows(std::wstring& reason)
{
    using RtlGetVersionFunction = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);

    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (ntdll == nullptr) {
        reason = L"Unable to locate ntdll.dll.";
        return false;
    }

    const auto rtlGetVersion = reinterpret_cast<RtlGetVersionFunction>(
        GetProcAddress(ntdll, "RtlGetVersion"));
    if (rtlGetVersion == nullptr) {
        reason = L"Unable to resolve RtlGetVersion.";
        return false;
    }

    RTL_OSVERSIONINFOW version{};
    version.dwOSVersionInfoSize = sizeof(version);
    if (rtlGetVersion(&version) != 0) {
        reason = L"Unable to determine the Windows version.";
        return false;
    }

    if (version.dwMajorVersion < 10 ||
        (version.dwMajorVersion == 10 && version.dwBuildNumber < 19041)) {
        reason = L"PhylaRAM requires Windows 10 version 2004 (build 19041) or later, or Windows 11.";
        return false;
    }

    return true;
}

int wmain(int argc, wchar_t** argv)
{
    if (argc == 1) {
        return LaunchGui(GetModuleHandleW(nullptr));
    }

    CliOptions options;
    std::wstring parseError;
    if (!ParseCommandLine(argc, argv, options, parseError)) {
        std::wcerr << parseError << L"\n\n";
        PrintUsage();
        return 1;
    }

    if (options.command == Command::Help) {
        PrintUsage();
        return 0;
    }
    if (options.command == Command::Gui) {
        return LaunchGui(GetModuleHandleW(nullptr));
    }

    if (!IsAdministrator()) {
        std::wcerr << L"PhylaRAM must run elevated as Administrator.\n";
        return 1;
    }

    std::wstring unsupportedReason;
    if (!IsSupportedWindows(unsupportedReason)) {
        std::wcerr << unsupportedReason << L"\n";
        return 1;
    }

    if (!SetConsoleCtrlHandler(ConsoleHandler, TRUE)) {
        std::wcerr << L"Unable to install the cancellation handler.\n";
        return 1;
    }

    if (options.command == Command::DryRun) {
        return RunDryRun(options.json);
    }
    return RunCapture(options);
}
