#include "gui.hpp"
#include "phylaram.hpp"

#include <commctrl.h>
#include <commdlg.h>
#include <chrono>
#include <ctime>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <windows.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")

namespace {

constexpr wchar_t kWindowClassName[] = L"PhylaRAMMainWindow";
constexpr UINT kMessageProgress = WM_APP + 1;
constexpr UINT kMessageWorkerDone = WM_APP + 2;

constexpr int kControlDestination = 1001;
constexpr int kControlBrowse = 1002;
constexpr int kControlStart = 1003;
constexpr int kControlDryRun = 1004;
constexpr int kControlCancel = 1005;
constexpr int kControlRate = 1006;
constexpr int kControlProgress = 1007;
constexpr int kControlStatus = 1008;

enum class WorkerOperation {
    Capture,
    DryRun,
};

struct ProgressPayload {
    uint64_t completedBytes = 0;
    uint64_t totalBytes = 0;
    double speedMBps = 0.0;
    uint32_t etaSeconds = 0;
};

struct WorkerResult {
    WorkerOperation operation = WorkerOperation::Capture;
    EvidenceCaptureResult capture;
    bool dryRunSucceeded = false;
    uint64_t highestPhysicalEnd = 0;
    uint64_t totalPhysicalBytes = 0;
    std::vector<MemoryRun> ranges;
    KernelHints hints;
    bool topologyChanged = false;
    std::wstring error;
    DWORD cleanupError = ERROR_SUCCESS;
};

std::wstring ReadWindowText(HWND control)
{
    const int length = GetWindowTextLengthW(control);
    if (length <= 0) {
        return {};
    }

    std::wstring text(static_cast<size_t>(length) + 1, L'\0');
    const int copied = GetWindowTextW(
        control,
        text.data(),
        static_cast<int>(text.size()));
    if (copied <= 0) {
        return {};
    }

    text.resize(static_cast<size_t>(copied));
    return text;
}

std::wstring DefaultEvidencePath()
{
    wchar_t computerName[MAX_COMPUTERNAME_LENGTH + 1]{};
    DWORD computerNameLength = MAX_COMPUTERNAME_LENGTH + 1;
    if (!GetComputerNameW(computerName, &computerNameLength)) {
        wcscpy_s(computerName, L"HOST");
    }

    const auto now = std::chrono::system_clock::now();
    const std::time_t rawTime = std::chrono::system_clock::to_time_t(now);
    std::tm localTime{};
    localtime_s(&localTime, &rawTime);

    wchar_t timestamp[32]{};
    wcsftime(timestamp, std::size(timestamp), L"%Y%m%d_%H%M%S", &localTime);

    CreateDirectoryW(L"C:\\Evidence", nullptr);

    std::wostringstream path;
    path << L"C:\\Evidence\\mem_" << computerName << L"_"
         << timestamp << L".raw";
    return path.str();
}

std::wstring ToWide(const std::string& text)
{
    return std::wstring(text.begin(), text.end());
}

void PostProgress(uint64_t completedBytes,
                  uint64_t totalBytes,
                  double speedMBps,
                  uint32_t etaSeconds,
                  void* callbackData)
{
    HWND window = static_cast<HWND>(callbackData);
    auto* payload = new ProgressPayload{
        completedBytes,
        totalBytes,
        speedMBps,
        etaSeconds,
    };

    if (!PostMessageW(
            window,
            kMessageProgress,
            0,
            reinterpret_cast<LPARAM>(payload))) {
        delete payload;
    }
}

class MainWindow final {
public:
    explicit MainWindow(HINSTANCE instance) : instance_(instance) {}

    ~MainWindow()
    {
        cancelled_.store(true);
        if (worker_.joinable()) {
            worker_.join();
        }
    }

    bool Create();
    int Run();

private:
    static LRESULT CALLBACK WindowProc(
        HWND window,
        UINT message,
        WPARAM wParam,
        LPARAM lParam);

    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    void CreateControls();
    void SetWorking(bool working);
    void SetStatus(const std::wstring& text);
    void BrowseForDestination();
    uint32_t SelectedRateLimit() const;
    void StartWorker(WorkerOperation operation);
    void HandleWorkerDone(WorkerResult* result);
    void ShowCaptureResult(const WorkerResult& result);
    void ShowDryRunResult(const WorkerResult& result);

    HINSTANCE instance_ = nullptr;
    HWND window_ = nullptr;
    HWND destination_ = nullptr;
    HWND browse_ = nullptr;
    HWND start_ = nullptr;
    HWND dryRun_ = nullptr;
    HWND cancel_ = nullptr;
    HWND rate_ = nullptr;
    HWND progress_ = nullptr;
    HWND status_ = nullptr;

    std::thread worker_;
    std::atomic_bool cancelled_{false};
    bool working_ = false;
    bool closing_ = false;
};

bool MainWindow::Create()
{
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.hInstance = instance_;
    windowClass.lpfnWndProc = &MainWindow::WindowProc;
    windowClass.lpszClassName = kWindowClassName;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    windowClass.hbrBackground =
        reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

    if (RegisterClassExW(&windowClass) == 0 &&
        GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    window_ = CreateWindowExW(
        0,
        kWindowClassName,
        L"PhylaRAM - Live RAM Capture",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        720,
        360,
        nullptr,
        nullptr,
        instance_,
        this);

    return window_ != nullptr;
}

int MainWindow::Run()
{
    ShowWindow(window_, SW_SHOWNORMAL);
    UpdateWindow(window_);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}

LRESULT CALLBACK MainWindow::WindowProc(
    HWND window,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    MainWindow* self = reinterpret_cast<MainWindow*>(
        GetWindowLongPtrW(window, GWLP_USERDATA));

    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        self = static_cast<MainWindow*>(create->lpCreateParams);
        self->window_ = window;
        SetWindowLongPtrW(
            window,
            GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(self));
    }

    return self != nullptr
               ? self->HandleMessage(message, wParam, lParam)
               : DefWindowProcW(window, message, wParam, lParam);
}

void MainWindow::CreateControls()
{
    HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

    auto createControl = [&](const wchar_t* className,
                             const wchar_t* text,
                             DWORD style,
                             int x,
                             int y,
                             int width,
                             int height,
                             int id) -> HWND {
        HWND control = CreateWindowExW(
            0,
            className,
            text,
            WS_CHILD | WS_VISIBLE | style,
            x,
            y,
            width,
            height,
            window_,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
            instance_,
            nullptr);
        if (control != nullptr) {
            SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        }
        return control;
    };

    createControl(
        L"STATIC",
        L"Evidence destination",
        0,
        24,
        24,
        180,
        22,
        0);

    destination_ = createControl(
        L"EDIT",
        DefaultEvidencePath().c_str(),
        WS_BORDER | ES_AUTOHSCROLL,
        24,
        48,
        540,
        26,
        kControlDestination);

    browse_ = createControl(
        L"BUTTON",
        L"Browse...",
        BS_PUSHBUTTON,
        574,
        47,
        110,
        28,
        kControlBrowse);

    createControl(
        L"STATIC",
        L"Rate limit",
        0,
        24,
        90,
        100,
        22,
        0);

    rate_ = createControl(
        WC_COMBOBOXW,
        L"",
        CBS_DROPDOWNLIST | WS_VSCROLL,
        24,
        112,
        170,
        120,
        kControlRate);

    struct RateChoice {
        const wchar_t* label;
        uint32_t value;
    };
    constexpr RateChoice rateChoices[] = {
        {L"Unlimited", 0},
        {L"500 MiB/s", 500},
        {L"250 MiB/s", 250},
        {L"100 MiB/s", 100},
        {L"25 MiB/s", 25},
        {L"1 MiB/s", 1},
    };

    for (const RateChoice& choice : rateChoices) {
        const LRESULT index = SendMessageW(
            rate_,
            CB_ADDSTRING,
            0,
            reinterpret_cast<LPARAM>(choice.label));
        if (index != CB_ERR && index != CB_ERRSPACE) {
            SendMessageW(
                rate_,
                CB_SETITEMDATA,
                static_cast<WPARAM>(index),
                static_cast<LPARAM>(choice.value));
        }
    }
    SendMessageW(rate_, CB_SETCURSEL, 0, 0);

    createControl(
        L"STATIC",
        L"SHA-256 and provenance sidecars are mandatory for every finalized capture.",
        0,
        220,
        112,
        464,
        22,
        0);

    start_ = createControl(
        L"BUTTON",
        L"Capture Memory",
        BS_DEFPUSHBUTTON,
        24,
        154,
        160,
        34,
        kControlStart);

    dryRun_ = createControl(
        L"BUTTON",
        L"Inspect Only",
        BS_PUSHBUTTON,
        194,
        154,
        130,
        34,
        kControlDryRun);

    cancel_ = createControl(
        L"BUTTON",
        L"Cancel",
        BS_PUSHBUTTON,
        334,
        154,
        100,
        34,
        kControlCancel);

    progress_ = createControl(
        PROGRESS_CLASSW,
        L"",
        0,
        24,
        208,
        660,
        24,
        kControlProgress);
    SendMessageW(progress_, PBM_SETRANGE32, 0, 1000);

    status_ = createControl(
        L"STATIC",
        L"Ready. The GUI uses the same evidence transaction as the CLI.",
        SS_LEFT,
        24,
        246,
        660,
        56,
        kControlStatus);

    SetWorking(false);
}

void MainWindow::SetWorking(bool working)
{
    working_ = working;
    EnableWindow(destination_, !working);
    EnableWindow(browse_, !working);
    EnableWindow(start_, !working);
    EnableWindow(dryRun_, !working);
    EnableWindow(rate_, !working);
    EnableWindow(cancel_, working);
}

void MainWindow::SetStatus(const std::wstring& text)
{
    if (status_ != nullptr) {
        SetWindowTextW(status_, text.c_str());
    }
}

void MainWindow::BrowseForDestination()
{
    std::wstring path = ReadWindowText(destination_);
    std::vector<wchar_t> buffer(
        std::max<size_t>(path.size() + 1024, 32768),
        L'\0');
    if (!path.empty()) {
        wcsncpy_s(buffer.data(), buffer.size(), path.c_str(), _TRUNCATE);
    }

    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = window_;
    dialog.lpstrFilter =
        L"RAW physical memory image (*.raw)\0*.raw\0All files (*.*)\0*.*\0";
    dialog.lpstrFile = buffer.data();
    dialog.nMaxFile = static_cast<DWORD>(buffer.size());
    dialog.lpstrDefExt = L"raw";
    dialog.Flags = OFN_PATHMUSTEXIST | OFN_NOREADONLYRETURN;

    if (GetSaveFileNameW(&dialog)) {
        SetWindowTextW(destination_, buffer.data());
    }
}

uint32_t MainWindow::SelectedRateLimit() const
{
    const LRESULT selected = SendMessageW(rate_, CB_GETCURSEL, 0, 0);
    if (selected == CB_ERR) {
        return 0;
    }

    const LRESULT value = SendMessageW(
        rate_,
        CB_GETITEMDATA,
        static_cast<WPARAM>(selected),
        0);
    if (value == CB_ERR || value < 0) {
        return 0;
    }
    return static_cast<uint32_t>(value);
}

void MainWindow::StartWorker(WorkerOperation operation)
{
    if (working_) {
        return;
    }
    if (worker_.joinable()) {
        worker_.join();
    }

    const std::wstring outputPath = ReadWindowText(destination_);
    if (operation == WorkerOperation::Capture && outputPath.empty()) {
        MessageBoxW(
            window_,
            L"Choose an evidence destination first.",
            L"PhylaRAM",
            MB_OK | MB_ICONWARNING);
        return;
    }

    const uint32_t rateLimit = SelectedRateLimit();
    cancelled_.store(false);
    SendMessageW(progress_, PBM_SETPOS, 0, 0);
    SetWorking(true);
    SetStatus(operation == WorkerOperation::Capture
                  ? L"Acquiring physical memory..."
                  : L"Inspecting physical-memory topology...");

    worker_ = std::thread([
        this,
        operation,
        outputPath,
        rateLimit]() {
        auto* result = new WorkerResult{};
        result->operation = operation;

        if (!IsAdministrator()) {
            result->error = L"PhylaRAM requires Administrator privileges.";
        } else {
            std::wstring supportError;
            if (!IsSupportedWindows(supportError)) {
                result->error = supportError;
            }
        }

        DriverRuntime runtime;
        DeviceSession device;

        if (result->error.empty()) {
            std::wstring runtimeError;
            if (!runtime.Start(runtimeError)) {
                result->error = runtimeError;
            } else if (!device.Open()) {
                result->error = L"Unable to open \\\\.\\PhylaRAM. Error " +
                                std::to_wstring(device.LastError()) + L".";
            }
        }

        if (result->error.empty()) {
            if (operation == WorkerOperation::DryRun) {
                const bool queryOk = device.Query(
                    result->highestPhysicalEnd,
                    result->totalPhysicalBytes,
                    result->ranges);
                const bool hintsOk = device.QueryHints(result->hints);
                const bool endOk = device.End(result->topologyChanged);

                if (!hintsOk) {
                    result->hints = KernelHints{};
                }
                if (!queryOk || !endOk) {
                    result->error = L"Dry-run topology inspection failed. Error " +
                                    std::to_wstring(device.LastError()) + L".";
                } else {
                    result->dryRunSucceeded = true;
                }
            } else {
                AcquisitionConfig config;
                config.quiet = true;
                config.rateLimitMBps = rateLimit;
                config.onProgress = &PostProgress;
                config.callbackData = window_;

                result->capture = CaptureEvidenceToFile(
                    device,
                    outputPath,
                    cancelled_,
                    config);
            }
        }

        device.Close();
        result->cleanupError = runtime.Stop();

        if (!PostMessageW(
                window_,
                kMessageWorkerDone,
                0,
                reinterpret_cast<LPARAM>(result))) {
            delete result;
        }
    });
}

void MainWindow::ShowCaptureResult(const WorkerResult& result)
{
    const EvidenceCaptureResult& capture = result.capture;

    if (capture.status == EvidenceCaptureStatus::Complete) {
        std::wostringstream message;
        message << L"Evidence bundle finalized successfully.\n\n"
                << L"Acquired: " << capture.summary.acquiredBytes << L" bytes\n"
                << L"SHA-256: " << ToWide(capture.summary.sha256) << L"\n\n"
                << L"Run phylaram-verify for independent offline verification.";
        if (result.cleanupError != ERROR_SUCCESS) {
            message << L"\n\nWarning: driver cleanup returned error "
                    << result.cleanupError << L".";
        }
        SetStatus(L"Capture finalized. Independent verification is still required.");
        MessageBoxW(
            window_,
            message.str().c_str(),
            L"PhylaRAM",
            MB_OK | (result.cleanupError == ERROR_SUCCESS
                         ? MB_ICONINFORMATION
                         : MB_ICONWARNING));
        return;
    }

    if (capture.status == EvidenceCaptureStatus::Incomplete) {
        std::wostringstream message;
        message << L"Evidence bundle finalized as INCOMPLETE.\n\n"
                << L"Unreadable: " << capture.summary.unreadableBytes << L" bytes\n"
                << L"Topology changed: "
                << (capture.summary.topologyChanged ? L"Yes" : L"No") << L"\n\n"
                << L"The map preserves these conditions. Run phylaram-verify before analysis.";
        if (result.cleanupError != ERROR_SUCCESS) {
            message << L"\n\nDriver cleanup returned error "
                    << result.cleanupError << L".";
        }
        SetStatus(L"Capture finalized as incomplete; review the provenance map.");
        MessageBoxW(
            window_,
            message.str().c_str(),
            L"PhylaRAM",
            MB_OK | MB_ICONWARNING);
        return;
    }

    std::wstring message = capture.error.empty()
                               ? L"The capture did not produce a finalized evidence bundle."
                               : capture.error;
    if (capture.systemError != ERROR_SUCCESS) {
        message += L"\nSystem error: " +
                   std::to_wstring(capture.systemError) + L".";
    }
    if (result.cleanupError != ERROR_SUCCESS) {
        message += L"\nDriver cleanup error: " +
                   std::to_wstring(result.cleanupError) + L".";
    }

    SetStatus(capture.status == EvidenceCaptureStatus::Cancelled
                  ? L"Capture cancelled; no final evidence bundle was published."
                  : L"Capture failed; no final evidence bundle was published.");
    MessageBoxW(
        window_,
        message.c_str(),
        L"PhylaRAM",
        MB_OK | (capture.status == EvidenceCaptureStatus::Cancelled
                     ? MB_ICONWARNING
                     : MB_ICONERROR));
}

void MainWindow::ShowDryRunResult(const WorkerResult& result)
{
    if (!result.dryRunSucceeded || !result.error.empty()) {
        std::wstring message = result.error.empty()
                                   ? L"Dry-run inspection failed."
                                   : result.error;
        if (result.cleanupError != ERROR_SUCCESS) {
            message += L"\nDriver cleanup error: " +
                       std::to_wstring(result.cleanupError) + L".";
        }
        SetStatus(L"Dry-run inspection failed.");
        MessageBoxW(
            window_,
            message.c_str(),
            L"PhylaRAM",
            MB_OK | MB_ICONERROR);
        return;
    }

    std::wostringstream message;
    message << L"Physical bytes: " << result.totalPhysicalBytes << L"\n"
            << L"Logical end: 0x" << std::hex << std::uppercase
            << result.highestPhysicalEnd << std::dec << L"\n"
            << L"Memory ranges: " << result.ranges.size() << L"\n"
            << L"Topology changed: "
            << (result.topologyChanged ? L"Yes" : L"No") << L"\n";

    if (result.hints.available) {
        message << L"System DTB: 0x" << std::hex << std::uppercase
                << result.hints.directoryTableBase << std::dec << L"\n"
                << L"Kernel base: 0x" << std::hex << std::uppercase
                << result.hints.kernelBase << std::dec << L"\n"
                << L"Windows build: " << result.hints.buildNumber << L"\n";
    }
    if (result.cleanupError != ERROR_SUCCESS) {
        message << L"Driver cleanup error: " << result.cleanupError << L"\n";
    }

    SetStatus(result.topologyChanged
                  ? L"Inspection completed; topology changed during the session."
                  : L"Inspection completed without creating evidence files.");
    MessageBoxW(
        window_,
        message.str().c_str(),
        L"PhylaRAM Dry Run",
        MB_OK | (result.topologyChanged || result.cleanupError != ERROR_SUCCESS
                     ? MB_ICONWARNING
                     : MB_ICONINFORMATION));
}

void MainWindow::HandleWorkerDone(WorkerResult* result)
{
    if (worker_.joinable()) {
        worker_.join();
    }
    SetWorking(false);

    if (closing_) {
        delete result;
        DestroyWindow(window_);
        return;
    }

    if (!result->error.empty() &&
        result->operation == WorkerOperation::Capture &&
        result->capture.error.empty()) {
        result->capture.status = EvidenceCaptureStatus::Failed;
        result->capture.error = result->error;
    }

    if (result->operation == WorkerOperation::Capture) {
        ShowCaptureResult(*result);
    } else {
        ShowDryRunResult(*result);
    }

    delete result;
}

LRESULT MainWindow::HandleMessage(
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (message) {
    case WM_CREATE:
        CreateControls();
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case kControlBrowse:
            if (HIWORD(wParam) == BN_CLICKED) {
                BrowseForDestination();
            }
            return 0;
        case kControlStart:
            if (HIWORD(wParam) == BN_CLICKED) {
                StartWorker(WorkerOperation::Capture);
            }
            return 0;
        case kControlDryRun:
            if (HIWORD(wParam) == BN_CLICKED) {
                StartWorker(WorkerOperation::DryRun);
            }
            return 0;
        case kControlCancel:
            if (HIWORD(wParam) == BN_CLICKED && working_) {
                cancelled_.store(true);
                EnableWindow(cancel_, FALSE);
                SetStatus(L"Cancellation requested. Finishing the current bounded operation...");
            }
            return 0;
        default:
            break;
        }
        break;

    case kMessageProgress: {
        auto* payload = reinterpret_cast<ProgressPayload*>(lParam);
        if (payload != nullptr) {
            const uint64_t scaled = payload->totalBytes == 0
                                        ? 0
                                        : static_cast<uint64_t>(
                                              (static_cast<long double>(
                                                   payload->completedBytes) *
                                               1000.0L) /
                                              static_cast<long double>(
                                                   payload->totalBytes));
            SendMessageW(
                progress_,
                PBM_SETPOS,
                static_cast<WPARAM>(std::min<uint64_t>(scaled, 1000)),
                0);

            std::wostringstream status;
            status << L"Acquiring: "
                   << (payload->completedBytes / (1024ull * 1024ull))
                   << L" / "
                   << (payload->totalBytes / (1024ull * 1024ull))
                   << L" MiB, " << std::fixed << std::setprecision(1)
                   << payload->speedMBps << L" MiB/s, ETA "
                   << (payload->etaSeconds / 60) << L":"
                   << std::setw(2) << std::setfill(L'0')
                   << (payload->etaSeconds % 60);
            SetStatus(status.str());
            delete payload;
        }
        return 0;
    }

    case kMessageWorkerDone:
        HandleWorkerDone(reinterpret_cast<WorkerResult*>(lParam));
        return 0;

    case WM_CLOSE:
        if (working_) {
            closing_ = true;
            cancelled_.store(true);
            EnableWindow(cancel_, FALSE);
            SetStatus(L"Cancelling before close...");
            return 0;
        }
        DestroyWindow(window_);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }

    return DefWindowProcW(window_, message, wParam, lParam);
}

} // namespace

int LaunchGui(HINSTANCE instance)
{
    INITCOMMONCONTROLSEX controls{};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_STANDARD_CLASSES | ICC_PROGRESS_CLASS;
    if (!InitCommonControlsEx(&controls)) {
        return 1;
    }

    MainWindow window(instance);
    if (!window.Create()) {
        return 1;
    }
    return window.Run();
}
