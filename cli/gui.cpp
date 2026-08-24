#include "gui.hpp"
#include "phylaram.hpp"

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <dwmapi.h>
#include <uxtheme.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "user32.lib")

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif

namespace {

constexpr wchar_t kWindowClassName[] = L"PhylaRAM_MainWindow";
constexpr UINT kMessageProgress = WM_APP + 1;
constexpr UINT kMessageWorkerDone = WM_APP + 2;

constexpr int kControlDestination = 1001;
constexpr int kControlBrowse      = 1002;
constexpr int kControlStart       = 1003;
constexpr int kControlDryRun      = 1004;
constexpr int kControlCancel      = 1005;
constexpr int kControlRate        = 1006;

// Minimalist, Understated Pro Dark Palette (Apple / Microsoft Official System Tool)
namespace Theme {
    constexpr COLORREF kCanvasBg        = RGB(30, 30, 30);      // #1E1E1E System Dark
    constexpr COLORREF kCardBg          = RGB(37, 37, 38);      // #252526 Elevated Container
    constexpr COLORREF kCardInsetBg     = RGB(45, 45, 48);      // #2D2D30 Input / Inset Surface
    constexpr COLORREF kBorder          = RGB(60, 60, 60);      // #3C3C3C 1px Crisp Divider
    constexpr COLORREF kBorderFocus     = RGB(0, 120, 212);     // #0078D4 Accent Border

    constexpr COLORREF kTextPrimary     = RGB(255, 255, 255);   // #FFFFFF Crisp White
    constexpr COLORREF kTextSecondary   = RGB(204, 204, 204);   // #CCCCCC Neutral Body
    constexpr COLORREF kTextMuted       = RGB(140, 140, 140);   // #8C8C8C Subdued Label
    constexpr COLORREF kTextMono        = RGB(220, 220, 220);   // #DCDCDC Code/Hex Values

    constexpr COLORREF kAccentPrimary   = RGB(0, 120, 212);     // #0078D4 Microsoft / Apple Blue
    constexpr COLORREF kAccentHover     = RGB(16, 132, 217);    // #1084D9
    constexpr COLORREF kAccentPress     = RGB(0, 103, 184);     // #0067B8

    constexpr COLORREF kBtnNeutralBg    = RGB(51, 51, 51);      // #333333 Neutral Button
    constexpr COLORREF kBtnNeutralHover = RGB(62, 62, 66);      // #3E3E42
    constexpr COLORREF kBtnNeutralPress = RGB(40, 40, 40);      // #282828

    constexpr COLORREF kProgressTrack   = RGB(45, 45, 48);      // #2D2D30
    constexpr COLORREF kProgressFill    = RGB(0, 120, 212);     // #0078D4 Solid Accent Fill
}

enum class WorkerOperation {
    Capture,
    DryRun,
};

struct ProgressPayload {
    uint64_t completedBytes = 0;
    uint64_t totalBytes     = 0;
    double speedMBps        = 0.0;
    uint32_t etaSeconds     = 0;
};

struct WorkerResult {
    WorkerOperation operation = WorkerOperation::Capture;
    EvidenceCaptureResult capture;
    bool dryRunSucceeded       = false;
    uint64_t highestPhysicalEnd = 0;
    uint64_t totalPhysicalBytes = 0;
    std::vector<MemoryRun> ranges;
    KernelHints hints;
    bool topologyChanged       = false;
    std::wstring error;
    DWORD cleanupError         = ERROR_SUCCESS;
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

std::wstring FormatBytes(uint64_t bytes)
{
    constexpr double kKiB = 1024.0;
    constexpr double kMiB = 1024.0 * 1024.0;
    constexpr double kGiB = 1024.0 * 1024.0 * 1024.0;

    std::wostringstream oss;
    oss << std::fixed << std::setprecision(2);
    if (bytes >= static_cast<uint64_t>(kGiB)) {
        oss << (static_cast<double>(bytes) / kGiB) << L" GiB";
    } else if (bytes >= static_cast<uint64_t>(kMiB)) {
        oss << (static_cast<double>(bytes) / kMiB) << L" MiB";
    } else if (bytes >= static_cast<uint64_t>(kKiB)) {
        oss << (static_cast<double>(bytes) / kKiB) << L" KiB";
    } else {
        oss << bytes << L" B";
    }
    return oss.str();
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

void FillSolidRect(HDC hdc, const RECT* prc, COLORREF color)
{
    COLORREF oldBk = SetBkColor(hdc, color);
    ExtTextOutW(hdc, 0, 0, ETO_OPAQUE, prc, L"", 0, nullptr);
    SetBkColor(hdc, oldBk);
}

void DrawCleanBox(HDC hdc, const RECT& rc, int radius, COLORREF bgColor, COLORREF borderColor)
{
    HBRUSH bgBrush = CreateSolidBrush(bgColor);
    HPEN borderPen = CreatePen(PS_SOLID, 1, borderColor);
    HGDIOBJ oldBrush = SelectObject(hdc, bgBrush);
    HGDIOBJ oldPen = SelectObject(hdc, borderPen);

    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, radius * 2, radius * 2);

    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(borderPen);
    DeleteObject(bgBrush);
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
        CleanupGdi();
    }

    bool Create();
    int Run();

private:
    static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK CustomButtonProc(HWND button, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
    static LRESULT CALLBACK CustomEditProc(HWND edit, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    void CreateFonts();
    void CleanupGdi();
    void CreateControls();
    void SetWorking(bool working);
    void SetStatus(const std::wstring& text);
    void BrowseForDestination();
    uint32_t SelectedRateLimit() const;
    void StartWorker(WorkerOperation operation);
    void HandleWorkerDone(WorkerResult* result);
    void ShowCaptureResult(const WorkerResult& result);
    void ShowDryRunResult(const WorkerResult& result);
    void Paint(HDC hdc);

    int Scale(int val) const { return MulDiv(val, dpi_, 96); }

    HINSTANCE instance_ = nullptr;
    HWND window_        = nullptr;
    HWND destination_   = nullptr;
    HWND browse_        = nullptr;
    HWND start_         = nullptr;
    HWND dryRun_        = nullptr;
    HWND cancel_        = nullptr;
    HWND rate_          = nullptr;

    // GDI Resources
    int dpi_ = 96;
    HFONT fontTitle_      = nullptr;
    HFONT fontSubtitle_   = nullptr;
    HFONT fontSection_    = nullptr;
    HFONT fontBody_       = nullptr;
    HFONT fontBodyBold_   = nullptr;
    HFONT fontMono_       = nullptr;
    HFONT fontSmall_      = nullptr;
    HBRUSH brushCanvas_   = nullptr;
    HBRUSH brushCard_     = nullptr;
    HBRUSH brushInset_    = nullptr;

    // Telemetry State
    std::thread worker_;
    std::atomic_bool cancelled_{false};
    bool working_ = false;
    bool closing_ = false;

    double progressPercent_   = 0.0;
    uint64_t completedBytes_  = 0;
    uint64_t totalBytes_      = 0;
    double currentSpeedMBps_  = 0.0;
    uint32_t currentEtaSec_   = 0;
    std::wstring statusText_  = L"Ready. Physical memory topology verified.";
    std::wstring dtbText_     = L"--";
    std::wstring kbaseText_   = L"--";
    std::wstring rangesText_  = L"--";
};

void MainWindow::CreateFonts()
{
    CleanupGdi();

    dpi_ = GetDpiForWindow(window_);
    if (dpi_ <= 0) dpi_ = 96;

    auto makeFont = [this](int ptSize, int weight, const wchar_t* face) -> HFONT {
        return CreateFontW(
            -MulDiv(ptSize, dpi_, 72),
            0, 0, 0,
            weight,
            FALSE, FALSE, FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE,
            face);
    };

    fontTitle_    = makeFont(14, FW_SEMIBOLD, L"Segoe UI Variable Display");
    fontSubtitle_ = makeFont(9,  FW_NORMAL,   L"Segoe UI Variable Text");
    fontSection_  = makeFont(9,  FW_SEMIBOLD, L"Segoe UI Variable Display");
    fontBody_     = makeFont(9,  FW_NORMAL,   L"Segoe UI Variable Text");
    fontBodyBold_ = makeFont(9,  FW_SEMIBOLD, L"Segoe UI Variable Text");
    fontMono_     = makeFont(9,  FW_NORMAL,   L"Cascadia Code");
    fontSmall_    = makeFont(8,  FW_NORMAL,   L"Segoe UI Variable Text");

    if (!fontTitle_) fontTitle_ = makeFont(14, FW_SEMIBOLD, L"Segoe UI");
    if (!fontMono_)  fontMono_  = makeFont(9,  FW_NORMAL,   L"Consolas");

    brushCanvas_ = CreateSolidBrush(Theme::kCanvasBg);
    brushCard_   = CreateSolidBrush(Theme::kCardBg);
    brushInset_  = CreateSolidBrush(Theme::kCardInsetBg);
}

void MainWindow::CleanupGdi()
{
    if (fontTitle_)    { DeleteObject(fontTitle_); fontTitle_ = nullptr; }
    if (fontSubtitle_) { DeleteObject(fontSubtitle_); fontSubtitle_ = nullptr; }
    if (fontSection_)  { DeleteObject(fontSection_); fontSection_ = nullptr; }
    if (fontBody_)     { DeleteObject(fontBody_); fontBody_ = nullptr; }
    if (fontBodyBold_) { DeleteObject(fontBodyBold_); fontBodyBold_ = nullptr; }
    if (fontMono_)     { DeleteObject(fontMono_); fontMono_ = nullptr; }
    if (fontSmall_)    { DeleteObject(fontSmall_); fontSmall_ = nullptr; }

    if (brushCanvas_)  { DeleteObject(brushCanvas_); brushCanvas_ = nullptr; }
    if (brushCard_)    { DeleteObject(brushCard_); brushCard_ = nullptr; }
    if (brushInset_)   { DeleteObject(brushInset_); brushInset_ = nullptr; }
}

bool MainWindow::Create()
{
    WNDCLASSEXW windowClass{};
    windowClass.cbSize        = sizeof(windowClass);
    windowClass.hInstance     = instance_;
    windowClass.lpfnWndProc   = &MainWindow::WindowProc;
    windowClass.lpszClassName = kWindowClassName;
    windowClass.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hIcon         = LoadIconW(nullptr, IDI_APPLICATION);
    windowClass.hbrBackground = nullptr;

    if (RegisterClassExW(&windowClass) == 0 &&
        GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    const int initialWidth  = 740;
    const int initialHeight = 510;

    window_ = CreateWindowExW(
        WS_EX_APPWINDOW,
        kWindowClassName,
        L"PhylaRAM — Physical Memory Acquisition",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        initialWidth,
        initialHeight,
        nullptr,
        nullptr,
        instance_,
        this);

    if (!window_) {
        return false;
    }

    BOOL darkMode = TRUE;
    DwmSetWindowAttribute(window_, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));
    DwmSetWindowAttribute(window_, 19, &darkMode, sizeof(darkMode));
    DWORD cornerPref = 2; // DWMWCP_ROUND
    DwmSetWindowAttribute(window_, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPref, sizeof(cornerPref));

    CreateFonts();
    CreateControls();

    return true;
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
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }

    return self != nullptr
               ? self->HandleMessage(message, wParam, lParam)
               : DefWindowProcW(window, message, wParam, lParam);
}

LRESULT CALLBACK MainWindow::CustomButtonProc(
    HWND button,
    UINT message,
    WPARAM wParam,
    LPARAM lParam,
    UINT_PTR uIdSubclass,
    DWORD_PTR dwRefData)
{
    UNREFERENCED_PARAMETER(uIdSubclass);
    UNREFERENCED_PARAMETER(dwRefData);

    switch (message) {
    case WM_MOUSEMOVE: {
        TRACKMOUSEEVENT tme{};
        tme.cbSize    = sizeof(tme);
        tme.dwFlags   = TME_LEAVE;
        tme.hwndTrack = button;
        TrackMouseEvent(&tme);
        InvalidateRect(button, nullptr, FALSE);
        break;
    }
    case WM_MOUSELEAVE:
        InvalidateRect(button, nullptr, FALSE);
        break;
    default:
        break;
    }
    return DefSubclassProc(button, message, wParam, lParam);
}

LRESULT CALLBACK MainWindow::CustomEditProc(
    HWND edit,
    UINT message,
    WPARAM wParam,
    LPARAM lParam,
    UINT_PTR uIdSubclass,
    DWORD_PTR dwRefData)
{
    UNREFERENCED_PARAMETER(uIdSubclass);
    UNREFERENCED_PARAMETER(dwRefData);

    if (message == WM_NCPAINT) {
        return 0;
    }
    return DefSubclassProc(edit, message, wParam, lParam);
}

void MainWindow::CreateControls()
{
    auto createButton = [&](const wchar_t* text, int x, int y, int w, int h, int id) -> HWND {
        HWND btn = CreateWindowExW(
            0,
            L"BUTTON",
            text,
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
            Scale(x), Scale(y), Scale(w), Scale(h),
            window_,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
            instance_,
            nullptr);
        SetWindowSubclass(btn, &MainWindow::CustomButtonProc, static_cast<UINT_PTR>(id), 0);
        return btn;
    };

    // Destination Path Edit
    destination_ = CreateWindowExW(
        0,
        L"EDIT",
        DefaultEvidencePath().c_str(),
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | WS_TABSTOP,
        Scale(40), Scale(94), Scale(524), Scale(28),
        window_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kControlDestination)),
        instance_,
        nullptr);
    SendMessageW(destination_, WM_SETFONT, reinterpret_cast<WPARAM>(fontBody_), TRUE);
    SetWindowSubclass(destination_, &MainWindow::CustomEditProc, kControlDestination, 0);

    // Browse Button
    browse_ = createButton(L"Browse...", 572, 92, 128, 32, kControlBrowse);

    // Rate Limit Combobox
    rate_ = CreateWindowExW(
        0,
        WC_COMBOBOXW,
        L"",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP,
        Scale(40), Scale(134), Scale(180), Scale(180),
        window_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kControlRate)),
        instance_,
        nullptr);
    SendMessageW(rate_, WM_SETFONT, reinterpret_cast<WPARAM>(fontBody_), TRUE);

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

    // Action Buttons
    start_  = createButton(L"Start Acquisition", 40, 426, 170, 36, kControlStart);
    dryRun_ = createButton(L"Inspect Topology", 218, 426, 150, 36, kControlDryRun);
    cancel_ = createButton(L"Cancel", 376, 426, 100, 36, kControlCancel);

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
    InvalidateRect(window_, nullptr, FALSE);
}

void MainWindow::SetStatus(const std::wstring& text)
{
    statusText_ = text;
    InvalidateRect(window_, nullptr, FALSE);
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
    dialog.hwndOwner   = window_;
    dialog.lpstrFilter =
        L"RAW physical memory image (*.raw)\0*.raw\0"
        L"Expert Witness Format (*.e01)\0*.e01\0"
        L"Complete Crash Dump (*.zdmp;*.dmp)\0*.zdmp;*.dmp\0"
        L"All files (*.*)\0*.*\0";
    dialog.lpstrFile   = buffer.data();
    dialog.nMaxFile    = static_cast<DWORD>(buffer.size());
    dialog.lpstrDefExt = L"raw";
    dialog.Flags       = OFN_PATHMUSTEXIST | OFN_NOREADONLYRETURN | OFN_OVERWRITEPROMPT;

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
            L"Please select an evidence destination file path.",
            L"PhylaRAM",
            MB_OK | MB_ICONWARNING);
        return;
    }

    const uint32_t rateLimit = SelectedRateLimit();
    cancelled_.store(false);
    progressPercent_  = 0.0;
    completedBytes_   = 0;
    totalBytes_       = 0;
    currentSpeedMBps_ = 0.0;
    currentEtaSec_    = 0;

    SetWorking(true);
    SetStatus(operation == WorkerOperation::Capture
                  ? L"Acquiring physical memory via Ring 0 driver..."
                  : L"Inspecting physical-memory topology...");

    worker_ = std::thread([this, operation, outputPath, rateLimit]() {
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
                result->error = L"Unable to open \\\\.\\PhylaRAM (Error " +
                                std::to_wstring(device.LastError()) + L").";
            }
        }

        if (result->error.empty()) {
            if (operation == WorkerOperation::DryRun) {
                const bool queryOk = device.Query(
                    result->highestPhysicalEnd,
                    result->totalPhysicalBytes,
                    result->ranges);
                const bool hintsOk = device.QueryHints(result->hints);
                const bool endOk   = device.End(result->topologyChanged);

                if (!hintsOk) {
                    result->hints = KernelHints{};
                }
                if (!queryOk || !endOk) {
                    result->error = L"Dry-run topology inspection failed. (Error " +
                                    std::to_wstring(device.LastError()) + L").";
                } else {
                    result->dryRunSucceeded = true;
                }
            } else {
                AcquisitionConfig config;
                config.quiet         = true;
                config.rateLimitMBps = rateLimit;
                config.onProgress    = &PostProgress;
                config.callbackData  = window_;

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
        const std::wstring outputPath = ReadWindowText(destination_);
        std::wostringstream message;
        message << L"Physical memory evidence bundle finalized.\n\n"
                << L"Acquired: " << FormatBytes(capture.summary.acquiredBytes) << L" (" << capture.summary.acquiredBytes << L" bytes)\n"
                << L"Logical RAW Size: " << FormatBytes(capture.summary.logicalSize) << L"\n"
                << L"SHA-256: " << ToWide(capture.summary.sha256) << L"\n\n"
                << L"Files generated:\n"
                << L"• " << outputPath << L"\n"
                << L"• " << outputPath << L".map.json\n"
                << L"• " << outputPath << L".sha256\n\n"
                << L"Independent verification with phylaram-verify is recommended.";

        if (result.cleanupError != ERROR_SUCCESS) {
            message << L"\n\nDriver cleanup returned code " << result.cleanupError << L".";
        }
        SetStatus(L"Capture complete and cryptographically finalized.");
        MessageBoxW(
            window_,
            message.str().c_str(),
            L"PhylaRAM — Capture Finalized",
            MB_OK | (result.cleanupError == ERROR_SUCCESS ? MB_ICONINFORMATION : MB_ICONWARNING));
        return;
    }

    if (capture.status == EvidenceCaptureStatus::Incomplete) {
        std::wostringstream message;
        message << L"Evidence bundle finalized as INCOMPLETE.\n\n"
                << L"Unreadable: " << capture.summary.unreadableBytes << L" bytes\n"
                << L"Topology changed: " << (capture.summary.topologyChanged ? L"Yes" : L"No") << L"\n\n"
                << L"The map preserves these conditions. Review map before analysis.";
        if (result.cleanupError != ERROR_SUCCESS) {
            message << L"\n\nDriver cleanup returned code " << result.cleanupError << L".";
        }
        SetStatus(L"Capture finalized as incomplete; review provenance map.");
        MessageBoxW(
            window_,
            message.str().c_str(),
            L"PhylaRAM — Incomplete Capture",
            MB_OK | MB_ICONWARNING);
        return;
    }

    std::wstring message = capture.error.empty()
                               ? L"The capture did not produce a finalized evidence bundle."
                               : capture.error;
    if (capture.systemError != ERROR_SUCCESS) {
        message += L"\nSystem error: " + std::to_wstring(capture.systemError) + L".";
    }
    if (result.cleanupError != ERROR_SUCCESS) {
        message += L"\nDriver cleanup error: " + std::to_wstring(result.cleanupError) + L".";
    }

    SetStatus(capture.status == EvidenceCaptureStatus::Cancelled
                  ? L"Acquisition cancelled by user. Partial files cleaned."
                  : L"Acquisition failed. No evidence files produced.");
    MessageBoxW(
        window_,
        message.c_str(),
        L"PhylaRAM — Status",
        MB_OK | (capture.status == EvidenceCaptureStatus::Cancelled ? MB_ICONWARNING : MB_ICONERROR));
}

void MainWindow::ShowDryRunResult(const WorkerResult& result)
{
    if (!result.dryRunSucceeded || !result.error.empty()) {
        std::wstring message = result.error.empty()
                                   ? L"Dry-run inspection failed."
                                   : result.error;
        if (result.cleanupError != ERROR_SUCCESS) {
            message += L"\nDriver cleanup error: " + std::to_wstring(result.cleanupError) + L".";
        }
        SetStatus(L"Topology inspection failed.");
        MessageBoxW(
            window_,
            message.c_str(),
            L"PhylaRAM — Inspection Failed",
            MB_OK | MB_ICONERROR);
        return;
    }

    std::wostringstream dtbHex, kbaseHex;
    dtbHex << L"0x" << std::hex << std::uppercase << result.hints.directoryTableBase;
    kbaseHex << L"0x" << std::hex << std::uppercase << result.hints.kernelBase;

    dtbText_    = dtbHex.str();
    kbaseText_  = kbaseHex.str();
    rangesText_ = std::to_wstring(result.ranges.size()) + L" ranges (" +
                  FormatBytes(result.totalPhysicalBytes) + L")";

    std::wostringstream message;
    message << L"Physical Memory Topology:\n"
            << L"• Total RAM: " << FormatBytes(result.totalPhysicalBytes) << L" (" << result.totalPhysicalBytes << L" bytes)\n"
            << L"• Highest Physical End: 0x" << std::hex << std::uppercase << result.highestPhysicalEnd << std::dec << L"\n"
            << L"• Memory Ranges: " << result.ranges.size() << L"\n"
            << L"• Topology Stability: " << (result.topologyChanged ? L"Modified during session" : L"Stable") << L"\n\n";

    if (result.hints.available) {
        message << L"Kernel Hints:\n"
                << L"• System DTB (CR3): " << dtbText_ << L"\n"
                << L"• Kernel Base: " << kbaseText_ << L"\n"
                << L"• Build Number: " << result.hints.buildNumber << L"\n";
    }

    SetStatus(result.topologyChanged
                  ? L"Inspection complete; topology changed during session."
                  : L"Inspection complete. Physical topology and hints verified.");

    InvalidateRect(window_, nullptr, FALSE);

    MessageBoxW(
        window_,
        message.str().c_str(),
        L"PhylaRAM — Inspection Complete",
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
        result->capture.error  = result->error;
    }

    if (result->operation == WorkerOperation::Capture) {
        if (result->capture.status == EvidenceCaptureStatus::Complete) {
            progressPercent_ = 100.0;
        }
        ShowCaptureResult(*result);
    } else {
        ShowDryRunResult(*result);
    }

    delete result;
}

void MainWindow::Paint(HDC hdc)
{
    RECT clientRect{};
    GetClientRect(window_, &clientRect);

    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBitmap = CreateCompatibleBitmap(hdc, clientRect.right, clientRect.bottom);
    HGDIOBJ oldBmp = SelectObject(memDC, memBitmap);

    SetBkMode(memDC, TRANSPARENT);

    // 1. Canvas
    FillSolidRect(memDC, &clientRect, Theme::kCanvasBg);

    // 2. Header
    {
        SelectObject(memDC, fontTitle_);
        SetTextColor(memDC, Theme::kTextPrimary);
        RECT titleRect{Scale(24), Scale(18), Scale(300), Scale(40)};
        DrawTextW(memDC, L"PhylaRAM", -1, &titleRect, DT_LEFT | DT_TOP | DT_SINGLELINE);

        SelectObject(memDC, fontSubtitle_);
        SetTextColor(memDC, Theme::kTextMuted);
        RECT subRect{Scale(24), Scale(38), Scale(400), Scale(54)};
        DrawTextW(memDC, L"Physical Memory Acquisition Engine", -1, &subRect, DT_LEFT | DT_TOP | DT_SINGLELINE);

        // Version & Role Badge (Single subtle badge)
        RECT badgeRect{clientRect.right - Scale(160), Scale(22), clientRect.right - Scale(24), Scale(44)};
        DrawCleanBox(memDC, badgeRect, Scale(4), Theme::kCardInsetBg, Theme::kBorder);
        SelectObject(memDC, fontSmall_);
        SetTextColor(memDC, Theme::kTextMuted);
        DrawTextW(memDC, L"v0.1.0-alpha · Admin", -1, &badgeRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // 3. Section 1: Target & Acquisition Pacing
    {
        RECT cardRect{Scale(24), Scale(62), clientRect.right - Scale(24), Scale(182)};
        DrawCleanBox(memDC, cardRect, Scale(6), Theme::kCardBg, Theme::kBorder);

        SelectObject(memDC, fontSection_);
        SetTextColor(memDC, Theme::kTextMuted);
        RECT headRect{cardRect.left + Scale(16), cardRect.top + Scale(10), cardRect.right - Scale(16), cardRect.top + Scale(26)};
        DrawTextW(memDC, L"TARGET & PACING", -1, &headRect, DT_LEFT | DT_SINGLELINE);

        // Inset border around path input
        RECT inputBorder{Scale(38), Scale(92), Scale(566), Scale(124)};
        DrawCleanBox(memDC, inputBorder, Scale(3), Theme::kCardInsetBg, Theme::kBorder);

        // Subdued note
        SelectObject(memDC, fontSmall_);
        SetTextColor(memDC, Theme::kTextMuted);
        RECT noteRect{Scale(230), Scale(138), cardRect.right - Scale(16), Scale(172)};
        DrawTextW(memDC, L"Generates verified flat RAW image, SHA-256 digest, and JSON provenance map sidecars.", -1, &noteRect, DT_LEFT | DT_SINGLELINE);
    }

    // 4. Section 2: Acquisition Status & Telemetry Inspector
    {
        RECT cardRect{Scale(24), Scale(192), clientRect.right - Scale(24), Scale(410)};
        DrawCleanBox(memDC, cardRect, Scale(6), Theme::kCardBg, Theme::kBorder);

        SelectObject(memDC, fontSection_);
        SetTextColor(memDC, Theme::kTextMuted);
        RECT headRect{cardRect.left + Scale(16), cardRect.top + Scale(10), cardRect.right - Scale(16), cardRect.top + Scale(26)};
        DrawTextW(memDC, L"ACQUISITION STATUS & TELEMETRY", -1, &headRect, DT_LEFT | DT_SINGLELINE);

        // Progress Bar
        RECT progTrack{Scale(40), Scale(224), clientRect.right - Scale(40), Scale(230)};
        FillSolidRect(memDC, &progTrack, Theme::kProgressTrack);

        if (progressPercent_ > 0.0) {
            int trackWidth = progTrack.right - progTrack.left;
            int fillWidth = static_cast<int>(trackWidth * std::min(progressPercent_ / 100.0, 1.0));
            RECT fillRect{progTrack.left, progTrack.top, progTrack.left + fillWidth, progTrack.bottom};
            FillSolidRect(memDC, &fillRect, Theme::kProgressFill);
        }

        // Clean Data Table (Key-Value inspector style)
        int col1X = Scale(40);
        int col2X = Scale(380);
        int row1Y = Scale(246);
        int row2Y = Scale(276);
        int row3Y = Scale(306);
        int row4Y = Scale(336);
        int row5Y = Scale(366);

        auto drawRow = [&](int x, int y, const wchar_t* key, const std::wstring& val) {
            SelectObject(memDC, fontBody_);
            SetTextColor(memDC, Theme::kTextMuted);
            RECT keyRect{x, y, x + Scale(110), y + Scale(20)};
            DrawTextW(memDC, key, -1, &keyRect, DT_LEFT | DT_SINGLELINE);

            SelectObject(memDC, fontMono_);
            SetTextColor(memDC, Theme::kTextPrimary);
            RECT valRect{x + Scale(115), y, x + Scale(320), y + Scale(20)};
            DrawTextW(memDC, val.c_str(), -1, &valRect, DT_LEFT | DT_SINGLELINE);
        };

        std::wostringstream speedStr, progStr, etaStr;
        speedStr << std::fixed << std::setprecision(1) << currentSpeedMBps_ << L" MiB/s";
        progStr << std::fixed << std::setprecision(1) << progressPercent_ << L"% ("
                << FormatBytes(completedBytes_) << L" / " << FormatBytes(totalBytes_) << L")";
        if (currentEtaSec_ > 0) {
            etaStr << (currentEtaSec_ / 60) << L":" << std::setw(2) << std::setfill(L'0') << (currentEtaSec_ % 60);
        } else {
            etaStr << L"--:--";
        }

        drawRow(col1X, row1Y, L"Status:", working_ ? L"Acquiring" : L"Ready");
        drawRow(col1X, row2Y, L"Throughput:", speedStr.str());
        drawRow(col1X, row3Y, L"Progress:", progStr.str());
        drawRow(col1X, row4Y, L"ETA:", etaStr.str());

        drawRow(col2X, row1Y, L"System DTB:", dtbText_);
        drawRow(col2X, row2Y, L"Kernel Base:", kbaseText_);
        drawRow(col2X, row3Y, L"Topology:", rangesText_);

        // Status Line
        SelectObject(memDC, fontSmall_);
        SetTextColor(memDC, Theme::kTextMuted);
        RECT statusRect{Scale(40), row5Y, clientRect.right - Scale(40), row5Y + Scale(18)};
        DrawTextW(memDC, statusText_.c_str(), -1, &statusRect, DT_LEFT | DT_SINGLELINE);
    }

    // 5. Action Bar
    // Buttons are rendered by WM_DRAWITEM

    BitBlt(hdc, 0, 0, clientRect.right, clientRect.bottom, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBmp);
    DeleteObject(memBitmap);
    DeleteDC(memDC);
}

LRESULT MainWindow::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(window_, &ps);
        Paint(hdc);
        EndPaint(window_, &ps);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC: {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        HWND ctl = reinterpret_cast<HWND>(lParam);
        SetBkMode(hdc, TRANSPARENT);

        if (ctl == destination_) {
            SetTextColor(hdc, Theme::kTextPrimary);
            SetBkColor(hdc, Theme::kCardInsetBg);
            return reinterpret_cast<LRESULT>(brushInset_);
        }
        SetTextColor(hdc, Theme::kTextPrimary);
        SetBkColor(hdc, Theme::kCardBg);
        return reinterpret_cast<LRESULT>(brushCard_);
    }

    case WM_DRAWITEM: {
        auto* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
        if (dis->CtlType != ODT_BUTTON) {
            break;
        }

        const bool isHover = (dis->itemState & ODS_HOTLIGHT) != 0;
        const bool isPress = (dis->itemState & ODS_SELECTED) != 0;
        const bool isDisable = (dis->itemState & ODS_DISABLED) != 0;

        COLORREF bg = Theme::kBtnNeutralBg;
        COLORREF border = Theme::kBorder;
        COLORREF textCol = Theme::kTextPrimary;

        if (dis->CtlID == kControlStart) {
            bg = isDisable ? Theme::kCardInsetBg
                           : (isPress ? Theme::kAccentPress : (isHover ? Theme::kAccentHover : Theme::kAccentPrimary));
            border = isDisable ? Theme::kBorder : Theme::kAccentPrimary;
            textCol = isDisable ? Theme::kTextMuted : RGB(255, 255, 255);
        } else {
            bg = isDisable ? Theme::kCardBg
                           : (isPress ? Theme::kBtnNeutralPress : (isHover ? Theme::kBtnNeutralHover : Theme::kBtnNeutralBg));
            border = Theme::kBorder;
            textCol = isDisable ? Theme::kTextMuted : Theme::kTextPrimary;
        }

        DrawCleanBox(dis->hDC, dis->rcItem, Scale(4), bg, border);

        SelectObject(dis->hDC, (dis->CtlID == kControlStart) ? fontBodyBold_ : fontBody_);
        SetTextColor(dis->hDC, textCol);
        SetBkMode(dis->hDC, TRANSPARENT);

        wchar_t btnText[64]{};
        GetWindowTextW(dis->hwndItem, btnText, 64);
        RECT textRect = dis->rcItem;
        DrawTextW(dis->hDC, btnText, -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        return TRUE;
    }

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
                SetStatus(L"Cancelling in-flight operation...");
            }
            return 0;
        default:
            break;
        }
        break;

    case kMessageProgress: {
        auto* payload = reinterpret_cast<ProgressPayload*>(lParam);
        if (payload != nullptr) {
            completedBytes_  = payload->completedBytes;
            totalBytes_      = payload->totalBytes;
            currentSpeedMBps_ = payload->speedMBps;
            currentEtaSec_   = payload->etaSeconds;

            if (totalBytes_ > 0) {
                progressPercent_ = (static_cast<double>(completedBytes_) * 100.0) / static_cast<double>(totalBytes_);
            }

            std::wostringstream oss;
            oss << L"Acquiring: " << FormatBytes(completedBytes_) << L" / " << FormatBytes(totalBytes_)
                << L" (" << std::fixed << std::setprecision(1) << currentSpeedMBps_ << L" MiB/s)";
            statusText_ = oss.str();

            delete payload;
            InvalidateRect(window_, nullptr, FALSE);
        }
        return 0;
    }

    case kMessageWorkerDone:
        HandleWorkerDone(reinterpret_cast<WorkerResult*>(lParam));
        return 0;

    case WM_DPICHANGED: {
        CreateFonts();
        RECT* prc = reinterpret_cast<RECT*>(lParam);
        SetWindowPos(window_, nullptr, prc->left, prc->top, prc->right - prc->left, prc->bottom - prc->top, SWP_NOZORDER | SWP_NOACTIVATE);
        InvalidateRect(window_, nullptr, TRUE);
        return 0;
    }

    case WM_CLOSE:
        if (working_) {
            closing_ = true;
            cancelled_.store(true);
            EnableWindow(cancel_, FALSE);
            SetStatus(L"Cancelling active acquisition before exit...");
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
    controls.dwICC  = ICC_STANDARD_CLASSES | ICC_PROGRESS_CLASS;
    if (!InitCommonControlsEx(&controls)) {
        return 1;
    }

    MainWindow window(instance);
    if (!window.Create()) {
        return 1;
    }
    return window.Run();
}

