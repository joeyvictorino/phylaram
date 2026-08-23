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
#pragma comment(lib, "user32.lib")

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif

namespace {

constexpr wchar_t kWindowClassName[] = L"PhylaRAM_FluentMainWindow";
constexpr UINT kMessageProgress = WM_APP + 1;
constexpr UINT kMessageWorkerDone = WM_APP + 2;

constexpr int kControlDestination = 1001;
constexpr int kControlBrowse      = 1002;
constexpr int kControlStart       = 1003;
constexpr int kControlDryRun      = 1004;
constexpr int kControlCancel      = 1005;
constexpr int kControlRate        = 1006;

// Apple Dark & Microsoft Fluent 2 Dark Color System
namespace Theme {
    constexpr COLORREF kCanvasBg        = RGB(18, 18, 22);      // #121216 Deep Charcoal
    constexpr COLORREF kCardBg          = RGB(28, 29, 36);      // #1C1D24 Surface Layer
    constexpr COLORREF kCardInsetBg     = RGB(36, 38, 48);      // #242630 Control Inset
    constexpr COLORREF kCardBorder      = RGB(52, 55, 70);      // #343746 Subtle Card Stroke
    constexpr COLORREF kCardBorderFocus = RGB(79, 140, 255);    // #4F8CFF Accent Stroke
    constexpr COLORREF kHeaderBadgeBg   = RGB(40, 44, 58);      // #282C3A Pill Badge

    constexpr COLORREF kTextPrimary     = RGB(248, 249, 252);   // #F8F9FC Pure Crisp Light
    constexpr COLORREF kTextSecondary   = RGB(168, 173, 192);   // #A8ADC0 Slate Text
    constexpr COLORREF kTextMuted       = RGB(112, 118, 138);   // #70768A Muted Label
    constexpr COLORREF kTextCyan        = RGB(56, 189, 248);    // #38BDF8 Telemetry Cyan
    constexpr COLORREF kTextEmerald     = RGB(52, 211, 153);    // #34D399 Verified Emerald
    constexpr COLORREF kTextAmber       = RGB(251, 191, 36);    // #FBBF24 Warning Amber
    constexpr COLORREF kTextRose        = RGB(248, 113, 113);   // #F87171 Crimson

    constexpr COLORREF kPrimaryBtnBg    = RGB(37, 99, 235);     // #2563EB Royal Blue Accent
    constexpr COLORREF kPrimaryBtnHover = RGB(59, 130, 246);    // #3B82F6 Bright Blue Hover
    constexpr COLORREF kPrimaryBtnPress = RGB(29, 78, 216);     // #1D4ED8 Deep Blue Press

    constexpr COLORREF kSecondaryBtnBg    = RGB(42, 45, 58);    // #2A2D3A Neutral Card Btn
    constexpr COLORREF kSecondaryBtnHover = RGB(56, 60, 78);    // #383C4E Hover Neutral
    constexpr COLORREF kSecondaryBtnPress = RGB(32, 34, 44);    // #20222C Press Neutral

    constexpr COLORREF kDangerBtnBg    = RGB(153, 27, 27);      // #991B1B Dark Crimson
    constexpr COLORREF kDangerBtnHover = RGB(185, 28, 28);      // #B91C1C
    constexpr COLORREF kDangerBtnPress = RGB(127, 29, 29);      // #7F1D1D

    constexpr COLORREF kProgressTrack  = RGB(24, 25, 32);      // #181920 Inset Track
    constexpr COLORREF kProgressGrad1  = RGB(6, 182, 212);      // #06B6D4 Electric Cyan
    constexpr COLORREF kProgressGrad2  = RGB(37, 99, 235);      // #2563EB Royal Blue
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

void DrawRoundedCard(HDC hdc, const RECT& rc, int radius, COLORREF bgColor, COLORREF borderColor)
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

void DrawHorizontalGradient(HDC hdc, const RECT& rc, COLORREF c1, COLORREF c2)
{
    TRIVERTEX vertex[2];
    vertex[0].x = rc.left;
    vertex[0].y = rc.top;
    vertex[0].Red = static_cast<COLOR16>(GetRValue(c1) << 8);
    vertex[0].Green = static_cast<COLOR16>(GetGValue(c1) << 8);
    vertex[0].Blue = static_cast<COLOR16>(GetBValue(c1) << 8);
    vertex[0].Alpha = 0x0000;

    vertex[1].x = rc.right;
    vertex[1].y = rc.bottom;
    vertex[1].Red = static_cast<COLOR16>(GetRValue(c2) << 8);
    vertex[1].Green = static_cast<COLOR16>(GetGValue(c2) << 8);
    vertex[1].Blue = static_cast<COLOR16>(GetBValue(c2) << 8);
    vertex[1].Alpha = 0x0000;

    GRADIENT_RECT gRect;
    gRect.UpperLeft = 0;
    gRect.LowerRight = 1;

    GradientFill(hdc, vertex, 2, &gRect, 1, GRADIENT_FILL_RECT_H);
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
    HFONT fontHero_       = nullptr;
    HFONT fontTitle_      = nullptr;
    HFONT fontSubtitle_   = nullptr;
    HFONT fontCardHead_   = nullptr;
    HFONT fontBody_       = nullptr;
    HFONT fontBodyBold_   = nullptr;
    HFONT fontMono_       = nullptr;
    HFONT fontMetricVal_  = nullptr;
    HFONT fontMetricLbl_  = nullptr;
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
    std::wstring statusText_  = L"Ready. System process DTB and memory topology validated.";
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

    fontHero_      = makeFont(20, FW_BOLD,     L"Segoe UI Variable Display");
    fontTitle_     = makeFont(15, FW_SEMIBOLD, L"Segoe UI Variable Display");
    fontSubtitle_  = makeFont(9,  FW_NORMAL,   L"Segoe UI Variable Text");
    fontCardHead_  = makeFont(11, FW_SEMIBOLD, L"Segoe UI Variable Display");
    fontBody_      = makeFont(10, FW_NORMAL,   L"Segoe UI Variable Text");
    fontBodyBold_  = makeFont(10, FW_SEMIBOLD, L"Segoe UI Variable Text");
    fontMono_      = makeFont(9,  FW_NORMAL,   L"Cascadia Code");
    fontMetricVal_ = makeFont(16, FW_BOLD,     L"Segoe UI Variable Display");
    fontMetricLbl_ = makeFont(8,  FW_SEMIBOLD, L"Segoe UI Variable Text");

    // Fallbacks if Segoe UI Variable is not installed
    if (!fontHero_) fontHero_ = makeFont(20, FW_BOLD, L"Segoe UI");
    if (!fontMono_) fontMono_ = makeFont(9, FW_NORMAL, L"Consolas");

    brushCanvas_ = CreateSolidBrush(Theme::kCanvasBg);
    brushCard_   = CreateSolidBrush(Theme::kCardBg);
    brushInset_  = CreateSolidBrush(Theme::kCardInsetBg);
}

void MainWindow::CleanupGdi()
{
    if (fontHero_)      { DeleteObject(fontHero_); fontHero_ = nullptr; }
    if (fontTitle_)     { DeleteObject(fontTitle_); fontTitle_ = nullptr; }
    if (fontSubtitle_)  { DeleteObject(fontSubtitle_); fontSubtitle_ = nullptr; }
    if (fontCardHead_)  { DeleteObject(fontCardHead_); fontCardHead_ = nullptr; }
    if (fontBody_)      { DeleteObject(fontBody_); fontBody_ = nullptr; }
    if (fontBodyBold_)  { DeleteObject(fontBodyBold_); fontBodyBold_ = nullptr; }
    if (fontMono_)      { DeleteObject(fontMono_); fontMono_ = nullptr; }
    if (fontMetricVal_) { DeleteObject(fontMetricVal_); fontMetricVal_ = nullptr; }
    if (fontMetricLbl_) { DeleteObject(fontMetricLbl_); fontMetricLbl_ = nullptr; }

    if (brushCanvas_)   { DeleteObject(brushCanvas_); brushCanvas_ = nullptr; }
    if (brushCard_)     { DeleteObject(brushCard_); brushCard_ = nullptr; }
    if (brushInset_)    { DeleteObject(brushInset_); brushInset_ = nullptr; }
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
    windowClass.hbrBackground = nullptr; // Double buffered paint

    if (RegisterClassExW(&windowClass) == 0 &&
        GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    const int initialWidth  = 860;
    const int initialHeight = 620;

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

    // Apply Native Windows 11 Dark Mode & Rounded Corners
    BOOL darkMode = TRUE;
    DwmSetWindowAttribute(window_, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));
    DwmSetWindowAttribute(window_, 19, &darkMode, sizeof(darkMode)); // Win10 build 17763 fallback
    DWM_WINDOW_CORNER_PREFERENCE corners = DWMWCP_ROUND;
    DwmSetWindowAttribute(window_, DWMWA_WINDOW_CORNER_PREFERENCE, &corners, sizeof(corners));

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
        return 0; // Seamless dark border painted by parent
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

    // Destination Path Edit Control
    destination_ = CreateWindowExW(
        0,
        L"EDIT",
        DefaultEvidencePath().c_str(),
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | WS_TABSTOP,
        Scale(44), Scale(128), Scale(630), Scale(30),
        window_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kControlDestination)),
        instance_,
        nullptr);
    SendMessageW(destination_, WM_SETFONT, reinterpret_cast<WPARAM>(fontBody_), TRUE);
    SetWindowSubclass(destination_, &MainWindow::CustomEditProc, kControlDestination, 0);

    // Browse Button
    browse_ = createButton(L"Browse...", 686, 126, 130, 34, kControlBrowse);

    // Rate Limit Combobox
    rate_ = CreateWindowExW(
        0,
        WC_COMBOBOXW,
        L"",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP,
        Scale(44), Scale(178), Scale(180), Scale(180),
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
        {L"Unlimited Throughput", 0},
        {L"500 MiB/s Pacing",     500},
        {L"250 MiB/s Pacing",     250},
        {L"100 MiB/s Pacing",     100},
        {L"25 MiB/s Pacing",      25},
        {L"1 MiB/s Forensic Low", 1},
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

    // Primary Action Buttons
    start_  = createButton(L"⚡ Start RAM Capture", 44, 500, 240, 44, kControlStart);
    dryRun_ = createButton(L"🔍 Inspect Topology", 296, 500, 200, 44, kControlDryRun);
    cancel_ = createButton(L"✕ Cancel", 508, 500, 130, 44, kControlCancel);

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
        L"RAW physical memory image (*.raw)\0*.raw\0All files (*.*)\0*.*\0";
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
            L"Please specify a valid evidence destination path.",
            L"PhylaRAM — Destination Required",
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
                  ? L"Acquiring physical memory via KMDF Ring 0 driver..."
                  : L"Inspecting physical-memory topology & Ring 0 hints...");

    worker_ = std::thread([this, operation, outputPath, rateLimit]() {
        auto* result = new WorkerResult{};
        result->operation = operation;

        if (!IsAdministrator()) {
            result->error = L"PhylaRAM requires Administrator privileges to manage kernel driver.";
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
                result->error = L"Unable to communicate with \\\\.\\PhylaRAM (Error " +
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
        std::wostringstream message;
        message << L"Physical memory evidence bundle finalized successfully!\n\n"
                << L"Acquired RAM: " << FormatBytes(capture.summary.acquiredBytes) << L"\n"
                << L"Logical RAW Size: " << FormatBytes(capture.summary.highestPhysicalEnd) << L"\n"
                << L"Cryptographic SHA-256:\n" << ToWide(capture.summary.sha256) << L"\n\n"
                << L"Artifacts written:\n"
                << L"• " << capture.summary.rawPath << L"\n"
                << L"• " << capture.summary.mapPath << L"\n"
                << L"• " << capture.summary.sha256Path << L"\n\n"
                << L"Run 'phylaram-verify' for independent offline verification.";

        if (result.cleanupError != ERROR_SUCCESS) {
            message << L"\n\nNotice: Driver cleanup returned code " << result.cleanupError << L".";
        }
        SetStatus(L"Capture complete & cryptographically finalized. SHA-256 sidecar generated.");
        MessageBoxW(
            window_,
            message.str().c_str(),
            L"PhylaRAM — Acquisition Successful",
            MB_OK | (result.cleanupError == ERROR_SUCCESS ? MB_ICONINFORMATION : MB_ICONWARNING));
        return;
    }

    if (capture.status == EvidenceCaptureStatus::Incomplete) {
        std::wostringstream message;
        message << L"Evidence bundle finalized as INCOMPLETE.\n\n"
                << L"Unreadable Spans: " << capture.summary.unreadableBytes << L" bytes\n"
                << L"Hardware Topology Changed: "
                << (capture.summary.topologyChanged ? L"Yes" : L"No") << L"\n\n"
                << L"The canonical map accurately records these unreadable regions.\n"
                << L"Review provenance metadata prior to forensic analysis.";
        if (result.cleanupError != ERROR_SUCCESS) {
            message << L"\n\nDriver cleanup returned code " << result.cleanupError << L".";
        }
        SetStatus(L"Capture finalized as INCOMPLETE; unreadable hardware spans preserved in map.");
        MessageBoxW(
            window_,
            message.str().c_str(),
            L"PhylaRAM — Incomplete Capture",
            MB_OK | MB_ICONWARNING);
        return;
    }

    std::wstring message = capture.error.empty()
                               ? L"The acquisition could not produce a finalized evidence bundle."
                               : capture.error;
    if (capture.systemError != ERROR_SUCCESS) {
        message += L"\nSystem error: " + std::to_wstring(capture.systemError) + L".";
    }
    if (result.cleanupError != ERROR_SUCCESS) {
        message += L"\nDriver cleanup error: " + std::to_wstring(result.cleanupError) + L".";
    }

    SetStatus(capture.status == EvidenceCaptureStatus::Cancelled
                  ? L"Acquisition cancelled by user. Partial staging files securely cleaned."
                  : L"Acquisition failed. Staged files cleaned up.");
    MessageBoxW(
        window_,
        message.c_str(),
        L"PhylaRAM — Capture Status",
        MB_OK | (capture.status == EvidenceCaptureStatus::Cancelled ? MB_ICONWARNING : MB_ICONERROR));
}

void MainWindow::ShowDryRunResult(const WorkerResult& result)
{
    if (!result.dryRunSucceeded || !result.error.empty()) {
        std::wstring message = result.error.empty()
                                   ? L"Dry-run topology inspection failed."
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
    rangesText_ = std::to_wstring(result.ranges.size()) + L" physical ranges (" +
                  FormatBytes(result.totalPhysicalBytes) + L" RAM)";

    std::wostringstream message;
    message << L"Hardware Physical Memory Topology:\n"
            << L"• Total Physical RAM: " << FormatBytes(result.totalPhysicalBytes) << L" (" << result.totalPhysicalBytes << L" bytes)\n"
            << L"• Highest Physical End: 0x" << std::hex << std::uppercase << result.highestPhysicalEnd << std::dec << L"\n"
            << L"• Disjoint Memory Runs: " << result.ranges.size() << L"\n"
            << L"• Topology Stability: " << (result.topologyChanged ? L"Modified during session" : L"Stable") << L"\n\n";

    if (result.hints.available) {
        message << L"Live Kernel Telemetry (Ring 0):\n"
                << L"• System Process DTB (CR3): " << dtbText_ << L"\n"
                << L"• NT Kernel Base: " << kbaseText_ << L"\n"
                << L"• Windows OS Build: " << result.hints.buildNumber << L" (NT "
                << result.hints.majorVersion << L"." << result.hints.minorVersion << L")\n";
    }

    SetStatus(result.topologyChanged
                  ? L"Inspection completed; physical topology modified during session."
                  : L"Inspection complete. Physical address ranges and DTB hints verified.");

    InvalidateRect(window_, nullptr, FALSE);

    MessageBoxW(
        window_,
        message.str().c_str(),
        L"PhylaRAM — Memory Topology Inspection",
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

    // Double buffering surface
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBitmap = CreateCompatibleBitmap(hdc, clientRect.right, clientRect.bottom);
    HGDIOBJ oldBmp = SelectObject(memDC, memBitmap);

    SetBkMode(memDC, TRANSPARENT);

    // 1. Canvas Background
    FillSolidRect(memDC, &clientRect, Theme::kCanvasBg);

    // 2. Header Area
    {
        RECT headerRect{Scale(24), Scale(20), clientRect.right - Scale(24), Scale(80)};

        // App Emblem / Icon Box
        RECT iconRect{headerRect.left, headerRect.top, headerRect.left + Scale(44), headerRect.top + Scale(44)};
        DrawRoundedCard(memDC, iconRect, Scale(8), Theme::kPrimaryBtnBg, Theme::kCardBorderFocus);

        SelectObject(memDC, fontTitle_);
        SetTextColor(memDC, RGB(255, 255, 255));
        RECT iconTextRect = iconRect;
        DrawTextW(memDC, L"RAM", -1, &iconTextRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // App Title & Subtitle
        SelectObject(memDC, fontHero_);
        SetTextColor(memDC, Theme::kTextPrimary);
        RECT titleRect{headerRect.left + Scale(56), headerRect.top - Scale(2), headerRect.left + Scale(350), headerRect.top + Scale(26)};
        DrawTextW(memDC, L"PhylaRAM", -1, &titleRect, DT_LEFT | DT_TOP | DT_SINGLELINE);

        SelectObject(memDC, fontSubtitle_);
        SetTextColor(memDC, Theme::kTextSecondary);
        RECT subRect{headerRect.left + Scale(58), headerRect.top + Scale(26), headerRect.left + Scale(400), headerRect.top + Scale(44)};
        DrawTextW(memDC, L"Kernel-Level Physical Memory Acquisition Engine", -1, &subRect, DT_LEFT | DT_TOP | DT_SINGLELINE);

        // Header Badges (Right side)
        int badgeRight = headerRect.right;
        auto drawBadge = [&](const wchar_t* text, COLORREF textCol, int width) {
            RECT bRect{badgeRight - Scale(width), headerRect.top + Scale(8), badgeRight, headerRect.top + Scale(32)};
            DrawRoundedCard(memDC, bRect, Scale(12), Theme::kHeaderBadgeBg, Theme::kCardBorder);
            SelectObject(memDC, fontMetricLbl_);
            SetTextColor(memDC, textCol);
            DrawTextW(memDC, text, -1, &bRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            badgeRight -= Scale(width + 8);
        };

        drawBadge(L"ADMINISTRATOR", Theme::kTextEmerald, 110);
        drawBadge(L"KMDF 1.15 RING 0", Theme::kTextCyan, 120);
        drawBadge(L"v0.1.0-alpha", Theme::kTextSecondary, 88);
    }

    // 3. Card 1: Evidence Output & Settings
    {
        RECT cardRect{Scale(24), Scale(80), clientRect.right - Scale(24), Scale(230)};
        DrawRoundedCard(memDC, cardRect, Scale(10), Theme::kCardBg, Theme::kCardBorder);

        // Card Header
        SelectObject(memDC, fontCardHead_);
        SetTextColor(memDC, Theme::kTextPrimary);
        RECT headRect{cardRect.left + Scale(20), cardRect.top + Scale(14), cardRect.right - Scale(20), cardRect.top + Scale(34)};
        DrawTextW(memDC, L"1. EVIDENCE DESTINATION & CONFIGURATION", -1, &headRect, DT_LEFT | DT_SINGLELINE);

        // Destination Input Inset Box
        RECT inputInset{Scale(40), Scale(124), Scale(676), Scale(162)};
        DrawRoundedCard(memDC, inputInset, Scale(6), Theme::kCardInsetBg, Theme::kCardBorder);

        // Rate Limit Label & Info Note
        SelectObject(memDC, fontBodyBold_);
        SetTextColor(memDC, Theme::kTextSecondary);

        RECT noteRect{Scale(240), Scale(180), cardRect.right - Scale(20), Scale(215)};
        SelectObject(memDC, fontSubtitle_);
        SetTextColor(memDC, Theme::kTextMuted);
        DrawTextW(memDC, L"• Mandatory bit-for-bit flat RAW + Logical SHA-256 + Canonical Map-2 sidecars\n• Sparse hole preservation with zero-backed accounting", -1, &noteRect, DT_LEFT);
    }

    // 4. Card 2: Live Acquisition Telemetry HUD
    {
        RECT cardRect{Scale(24), Scale(242), clientRect.right - Scale(24), Scale(482)};
        DrawRoundedCard(memDC, cardRect, Scale(10), Theme::kCardBg, Theme::kCardBorder);

        // Card Header
        SelectObject(memDC, fontCardHead_);
        SetTextColor(memDC, Theme::kTextPrimary);
        RECT headRect{cardRect.left + Scale(20), cardRect.top + Scale(14), cardRect.right - Scale(20), cardRect.top + Scale(34)};
        DrawTextW(memDC, L"2. LIVE FORENSIC TELEMETRY & TOPOLOGY", -1, &headRect, DT_LEFT | DT_SINGLELINE);

        // Progress Bar Track
        RECT progTrack{Scale(40), Scale(280), clientRect.right - Scale(40), Scale(302)};
        DrawRoundedCard(memDC, progTrack, Scale(5), Theme::kProgressTrack, Theme::kCardBorder);

        // Progress Bar Fill
        if (progressPercent_ > 0.0) {
            int trackWidth = progTrack.right - progTrack.left - Scale(4);
            int fillWidth = static_cast<int>(trackWidth * std::min(progressPercent_ / 100.0, 1.0));
            if (fillWidth > Scale(6)) {
                RECT fillRect{progTrack.left + Scale(2), progTrack.top + Scale(2), progTrack.left + Scale(2) + fillWidth, progTrack.bottom - Scale(2)};
                DrawRoundedCard(memDC, fillRect, Scale(4), Theme::kPrimaryBtnBg, Theme::kPrimaryBtnHover);
                DrawHorizontalGradient(memDC, fillRect, Theme::kProgressGrad1, Theme::kProgressGrad2);
            }
        }

        // 4 Telemetry Metrics Tiles
        int cardInnerWidth = (clientRect.right - Scale(48)) - Scale(40);
        int tileWidth = (cardInnerWidth - Scale(36)) / 4;
        int tileTop = Scale(314);
        int tileHeight = Scale(72);

        auto drawMetricTile = [&](int index, const wchar_t* label, const std::wstring& value, COLORREF valColor) {
            int tileLeft = Scale(40) + index * (tileWidth + Scale(12));
            RECT tileRect{tileLeft, tileTop, tileLeft + tileWidth, tileTop + tileHeight};
            DrawRoundedCard(memDC, tileRect, Scale(6), Theme::kCardInsetBg, Theme::kCardBorder);

            // Label
            RECT lblRect{tileRect.left + Scale(10), tileRect.top + Scale(8), tileRect.right - Scale(10), tileRect.top + Scale(24)};
            SelectObject(memDC, fontMetricLbl_);
            SetTextColor(memDC, Theme::kTextMuted);
            DrawTextW(memDC, label, -1, &lblRect, DT_LEFT | DT_SINGLELINE);

            // Value
            RECT valRect{tileRect.left + Scale(10), tileRect.top + Scale(26), tileRect.right - Scale(10), tileRect.bottom - Scale(8)};
            SelectObject(memDC, fontMetricVal_);
            SetTextColor(memDC, valColor);
            DrawTextW(memDC, value.c_str(), -1, &valRect, DT_LEFT | DT_SINGLELINE);
        };

        std::wostringstream speedStr;
        speedStr << std::fixed << std::setprecision(1) << currentSpeedMBps_ << L" MiB/s";

        std::wostringstream progStr;
        progStr << std::fixed << std::setprecision(1) << progressPercent_ << L"%";

        std::wostringstream etaStr;
        if (currentEtaSec_ > 0) {
            etaStr << (currentEtaSec_ / 60) << L":" << std::setw(2) << std::setfill(L'0') << (currentEtaSec_ % 60);
        } else {
            etaStr << L"--:--";
        }

        drawMetricTile(0, L"THROUGHPUT", speedStr.str(), Theme::kTextCyan);
        drawMetricTile(1, L"PROGRESS", progStr.str(), Theme::kTextPrimary);
        drawMetricTile(2, L"ETA REMAINING", etaStr.str(), Theme::kTextAmber);
        drawMetricTile(3, L"STATUS", working_ ? L"ACQUIRING" : L"READY", working_ ? Theme::kTextCyan : Theme::kTextEmerald);

        // Hardware & Kernel Telemetry Strip
        RECT stripRect{Scale(40), Scale(398), clientRect.right - Scale(40), Scale(464)};
        DrawRoundedCard(memDC, stripRect, Scale(6), Theme::kCardInsetBg, Theme::kCardBorder);

        SelectObject(memDC, fontMono_);
        SetTextColor(memDC, Theme::kTextSecondary);

        std::wostringstream line1, line2;
        line1 << L"System DTB (CR3) : " << dtbText_ << L"    |    NT Kernel Base: " << kbaseText_;
        line2 << L"Memory Topology  : " << rangesText_;

        RECT t1Rect{stripRect.left + Scale(12), stripRect.top + Scale(10), stripRect.right - Scale(12), stripRect.top + Scale(28)};
        RECT t2Rect{stripRect.left + Scale(12), stripRect.top + Scale(32), stripRect.right - Scale(12), stripRect.bottom - Scale(8)};
        DrawTextW(memDC, line1.str().c_str(), -1, &t1Rect, DT_LEFT | DT_SINGLELINE);
        DrawTextW(memDC, line2.str().c_str(), -1, &t2Rect, DT_LEFT | DT_SINGLELINE);
    }

    // 5. Action Bar & Status Footer
    {
        // Status Bar String
        SelectObject(memDC, fontBody_);
        SetTextColor(memDC, working_ ? Theme::kTextCyan : Theme::kTextSecondary);
        RECT statusRect{Scale(44), Scale(554), clientRect.right - Scale(44), Scale(574)};
        DrawTextW(memDC, statusText_.c_str(), -1, &statusRect, DT_LEFT | DT_SINGLELINE);
    }

    // Blit double buffer to screen
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
        return 1; // Handled in WM_PAINT

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

        COLORREF bg = Theme::kSecondaryBtnBg;
        COLORREF border = Theme::kCardBorder;
        COLORREF textCol = Theme::kTextPrimary;

        if (dis->CtlID == kControlStart) {
            bg = isDisable ? Theme::kHeaderBadgeBg
                           : (isPress ? Theme::kPrimaryBtnPress : (isHover ? Theme::kPrimaryBtnHover : Theme::kPrimaryBtnBg));
            border = isDisable ? Theme::kCardBorder : Theme::kCardBorderFocus;
            textCol = isDisable ? Theme::kTextMuted : RGB(255, 255, 255);
        } else if (dis->CtlID == kControlCancel) {
            bg = isDisable ? Theme::kHeaderBadgeBg
                           : (isPress ? Theme::kDangerBtnPress : (isHover ? Theme::kDangerBtnHover : Theme::kDangerBtnBg));
            border = isDisable ? Theme::kCardBorder : Theme::kTextRose;
            textCol = isDisable ? Theme::kTextMuted : RGB(255, 255, 255);
        } else {
            bg = isDisable ? Theme::kHeaderBadgeBg
                           : (isPress ? Theme::kSecondaryBtnPress : (isHover ? Theme::kSecondaryBtnHover : Theme::kSecondaryBtnBg));
            border = isDisable ? Theme::kCardBorder : (isHover ? Theme::kCardBorderFocus : Theme::kCardBorder);
            textCol = isDisable ? Theme::kTextMuted : Theme::kTextPrimary;
        }

        DrawRoundedCard(dis->hDC, dis->rcItem, Scale(6), bg, border);

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
                SetStatus(L"Cancellation requested. Safely completing in-flight chunk...");
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
            oss << L"Acquiring RAM: " << FormatBytes(completedBytes_) << L" / " << FormatBytes(totalBytes_)
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
