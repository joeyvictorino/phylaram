#include "gui.hpp"
#include "phylaram.hpp"
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comdlg32.lib")

// Custom window messages
#define WM_PHYLA_PROGRESS   (WM_USER + 101)
#define WM_PHYLA_COMPLETE   (WM_USER + 102)
#define WM_PHYLA_ERROR      (WM_USER + 103)
#define WM_PHYLA_DRYRUN_DONE (WM_USER + 104)

// Control IDs
enum ControlId : int {
    ID_BTN_START = 1001,
    ID_BTN_BROWSE = 1002,
    ID_BTN_CANCEL = 1003,
    ID_BTN_OPEN_FOLDER = 1004,
    ID_BTN_COPY_HASH = 1005,
    ID_BTN_DRYRUN = 1006,
    ID_BTN_NEW_CAPTURE = 1007,
    ID_EDIT_DESTINATION = 1008,
    ID_CHK_SHA256 = 1009,
    ID_COMBO_RATELIMIT = 1010,
};

enum class GuiState {
    Ready,
    Acquiring,
    Complete,
    Error
};

struct ProgressPayload {
    uint64_t acquired;
    uint64_t total;
    double speedMBs;
    uint32_t etaSeconds;
};

struct CompletePayload {
    AcquisitionSummary summary;
    std::wstring rawPath;
    std::string sha256Hex;
    double totalSeconds;
    double avgSpeedMBs;
};

class PhylaMainWindow {
public:
    PhylaMainWindow() = default;
    ~PhylaMainWindow() {
        CleanupBrushes();
    }

    bool Create(HINSTANCE hInstance);
    void Show(int nCmdShow);
    HWND GetHwnd() const noexcept { return hwnd_; }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    void InitThemeBrushes();
    void CleanupBrushes();
    void CreateControls();
    void LayoutControls();
    void SetState(GuiState newState);
    void UpdateDiskFreeSpace();
    void StartAcquisitionWorker(bool isDryRunOnly);
    void CancelAcquisition();

    // UI Drawing Helpers
    void OnPaint(HDC hdc, const RECT& rc);
    void DrawCard(HDC hdc, const RECT& rc, COLORREF bgCol, COLORREF borderCol, int radius);

    HWND hwnd_ = nullptr;
    HINSTANCE hInstance_ = nullptr;
    HFONT hFontTitle_ = nullptr;
    HFONT hFontHeader_ = nullptr;
    HFONT hFontBody_ = nullptr;
    HFONT hFontMono_ = nullptr;
    HFONT hFontBold_ = nullptr;

    HBRUSH hBrBack_ = nullptr;
    HBRUSH hBrCard_ = nullptr;
    HBRUSH hBrCardBorder_ = nullptr;
    HBRUSH hBrAccent_ = nullptr;
    HBRUSH hBrSuccess_ = nullptr;

    // Controls
    HWND hEditDest_ = nullptr;
    HWND hBtnBrowse_ = nullptr;
    HWND hBtnStart_ = nullptr;
    HWND hBtnDryRun_ = nullptr;
    HWND hBtnCancel_ = nullptr;
    HWND hBtnOpenFolder_ = nullptr;
    HWND hBtnCopyHash_ = nullptr;
    HWND hBtnNewCapture_ = nullptr;
    HWND hChkSha256_ = nullptr;
    HWND hComboRate_ = nullptr;

    // State
    GuiState state_ = GuiState::Ready;
    std::wstring destPath_;
    std::wstring diskSpaceText_ = L"Checking disk space...";
    bool diskSpaceOk_ = true;
    uint64_t totalRamBytes_ = 0;
    uint32_t ramRangeCount_ = 0;
    KernelHints hints_{};

    // Live Acquisition Stats
    std::atomic_bool workerCancelled_{false};
    std::thread workerThread_;
    uint64_t liveAcquired_ = 0;
    uint64_t liveTotal_ = 0;
    double liveSpeed_ = 0.0;
    uint32_t liveEta_ = 0;
    std::wstring errorMessage_;

    // Completion Results
    CompletePayload finalResult_{};
};

static PhylaMainWindow* g_pMainWnd = nullptr;

static std::wstring GetDefaultDestination()
{
    // Generate default timestamped path: C:\Evidence\mem_HOSTNAME_YYYYMMDD_HHMMSS.raw
    wchar_t hostBuffer[MAX_COMPUTERNAME_LENGTH + 1] = { 0 };
    DWORD hostSize = MAX_COMPUTERNAME_LENGTH + 1;
    GetComputerNameW(hostBuffer, &hostSize);

    auto now = std::chrono::system_clock::now();
    auto timeT = std::chrono::system_clock::to_time_t(now);
    struct tm tmNow;
    localtime_s(&tmNow, &timeT);

    wchar_t timeBuf[64];
    wcsftime(timeBuf, 64, L"%Y%m%d_%H%M%S", &tmNow);

    std::wstring basePath = L"C:\\Evidence";
    CreateDirectoryW(basePath.c_str(), nullptr);

    std::wostringstream ss;
    ss << basePath << L"\\mem_" << (hostSize > 0 ? hostBuffer : L"HOST") << L"_" << timeBuf << L".raw";
    return ss.str();
}

static uint64_t GetDiskFree(const std::wstring& filePath, uint64_t& totalDiskBytes)
{
    std::wstring root;
    size_t colon = filePath.find(L':');
    if (colon != std::wstring::npos && colon > 0) {
        root = filePath.substr(0, colon + 1) + L"\\";
    } else {
        root = L"C:\\";
    }

    ULARGE_INTEGER freeBytesCaller, totalBytes, totalFree;
    if (GetDiskFreeSpaceExW(root.c_str(), &freeBytesCaller, &totalBytes, &totalFree)) {
        totalDiskBytes = totalBytes.QuadPart;
        return freeBytesCaller.QuadPart;
    }
    return 0;
}

void PhylaMainWindow::InitThemeBrushes()
{
    // Deep Charcoal / Microsoft Fluent Dark Mode Palette
    hBrBack_ = CreateSolidBrush(RGB(24, 26, 31));        // #181A1F
    hBrCard_ = CreateSolidBrush(RGB(33, 37, 43));        // #21252B
    hBrCardBorder_ = CreateSolidBrush(RGB(50, 56, 66));  // #323842
    hBrAccent_ = CreateSolidBrush(RGB(0, 120, 212));     // #0078D4
    hBrSuccess_ = CreateSolidBrush(RGB(16, 124, 65));    // #107C41

    // Crisp typography
    hFontTitle_ = CreateFontW(22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Variable Display");
    if (!hFontTitle_) {
        hFontTitle_ = CreateFontW(22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                 OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                 DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    }

    hFontHeader_ = CreateFontW(16, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                              DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    hFontBody_ = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                            DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    hFontBold_ = CreateFontW(14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                            DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    hFontMono_ = CreateFontW(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                            FIXED_PITCH | FF_MODERN, L"Cascadia Code");
    if (!hFontMono_) {
        hFontMono_ = CreateFontW(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                 OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                 FIXED_PITCH | FF_MODERN, L"Consolas");
    }
}

void PhylaMainWindow::CleanupBrushes()
{
    if (hBrBack_) DeleteObject(hBrBack_);
    if (hBrCard_) DeleteObject(hBrCard_);
    if (hBrCardBorder_) DeleteObject(hBrCardBorder_);
    if (hBrAccent_) DeleteObject(hBrAccent_);
    if (hBrSuccess_) DeleteObject(hBrSuccess_);

    if (hFontTitle_) DeleteObject(hFontTitle_);
    if (hFontHeader_) DeleteObject(hFontHeader_);
    if (hFontBody_) DeleteObject(hFontBody_);
    if (hFontBold_) DeleteObject(hFontBold_);
    if (hFontMono_) DeleteObject(hFontMono_);
}

bool PhylaMainWindow::Create(HINSTANCE hInstance)
{
    hInstance_ = hInstance;
    g_pMainWnd = this;

    InitThemeBrushes();

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = PhylaMainWindow::WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = LoadIcon(nullptr, IDI_SHIELD);
    wc.hbrBackground = hBrBack_;
    wc.lpszClassName = L"PhylaRAM_MainWindowClass";

    RegisterClassExW(&wc);

    int w = 680;
    int h = 540;
    int x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;

    hwnd_ = CreateWindowExW(
        0,
        wc.lpszClassName,
        L"PhylaRAM 0.1.0-alpha — Physical Memory Acquisition",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        x, y, w, h,
        nullptr, nullptr, hInstance, this
    );

    if (!hwnd_) {
        return false;
    }

    // Enable Windows 11 Immersive Dark Mode on Window Frame
    BOOL darkMode = TRUE;
    DwmSetWindowAttribute(hwnd_, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &darkMode, sizeof(darkMode));

    destPath_ = GetDefaultDestination();

    CreateControls();
    SetState(GuiState::Ready);
    UpdateDiskFreeSpace();

    // Query preliminary memory layout from OS
    MEMORYSTATUSEX memStatus{};
    memStatus.dwLength = sizeof(memStatus);
    if (GlobalMemoryStatusEx(&memStatus)) {
        totalRamBytes_ = memStatus.ullTotalPhys;
    }

    return true;
}

void PhylaMainWindow::Show(int nCmdShow)
{
    ShowWindow(hwnd_, nCmdShow);
    UpdateWindow(hwnd_);
}

void PhylaMainWindow::CreateControls()
{
    // Destination Edit Box
    hEditDest_ = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", destPath_.c_str(),
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        30, 210, 500, 28,
        hwnd_, (HMENU)ID_EDIT_DESTINATION, hInstance_, nullptr
    );
    SendMessageW(hEditDest_, WM_SETFONT, (WPARAM)hFontBody_, TRUE);

    // Browse Button
    hBtnBrowse_ = CreateWindowExW(
        0, L"BUTTON", L"Browse...",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        540, 210, 100, 28,
        hwnd_, (HMENU)ID_BTN_BROWSE, hInstance_, nullptr
    );
    SendMessageW(hBtnBrowse_, WM_SETFONT, (WPARAM)hFontBody_, TRUE);

    // Checkbox: SHA-256
    hChkSha256_ = CreateWindowExW(
        0, L"BUTTON", L"Compute Logical SHA-256 Checksum",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        30, 280, 280, 24,
        hwnd_, (HMENU)ID_CHK_SHA256, hInstance_, nullptr
    );
    SendMessageW(hChkSha256_, WM_SETFONT, (WPARAM)hFontBody_, TRUE);
    Button_SetCheck(hChkSha256_, BST_CHECKED);

    // Combo: Rate Limit
    hComboRate_ = CreateWindowExW(
        0, L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        450, 280, 190, 140,
        hwnd_, (HMENU)ID_COMBO_RATELIMIT, hInstance_, nullptr
    );
    SendMessageW(hComboRate_, WM_SETFONT, (WPARAM)hFontBody_, TRUE);
    ComboBox_AddString(hComboRate_, L"Maximum Speed (No Limit)");
    ComboBox_AddString(hComboRate_, L"Throttle: 500 MB/s");
    ComboBox_AddString(hComboRate_, L"Throttle: 250 MB/s");
    ComboBox_AddString(hComboRate_, L"Throttle: 100 MB/s");
    ComboBox_SetCurSel(hComboRate_, 0);

    // Primary Action Button: Start Memory Capture
    hBtnStart_ = CreateWindowExW(
        0, L"BUTTON", L"Start Memory Capture",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        30, 425, 380, 44,
        hwnd_, (HMENU)ID_BTN_START, hInstance_, nullptr
    );
    SendMessageW(hBtnStart_, WM_SETFONT, (WPARAM)hFontHeader_, TRUE);

    // Dry Run Triage Button
    hBtnDryRun_ = CreateWindowExW(
        0, L"BUTTON", L"Dry-Run Triage",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        425, 425, 215, 44,
        hwnd_, (HMENU)ID_BTN_DRYRUN, hInstance_, nullptr
    );
    SendMessageW(hBtnDryRun_, WM_SETFONT, (WPARAM)hFontHeader_, TRUE);

    // Cancel Button (Screen 2)
    hBtnCancel_ = CreateWindowExW(
        0, L"BUTTON", L"Cancel Capture",
        WS_CHILD | BS_PUSHBUTTON,
        240, 430, 200, 40,
        hwnd_, (HMENU)ID_BTN_CANCEL, hInstance_, nullptr
    );
    SendMessageW(hBtnCancel_, WM_SETFONT, (WPARAM)hFontHeader_, TRUE);

    // Open Evidence Folder Button (Screen 3)
    hBtnOpenFolder_ = CreateWindowExW(
        0, L"BUTTON", L"Open Evidence Folder",
        WS_CHILD | BS_DEFPUSHBUTTON,
        30, 425, 300, 44,
        hwnd_, (HMENU)ID_BTN_OPEN_FOLDER, hInstance_, nullptr
    );
    SendMessageW(hBtnOpenFolder_, WM_SETFONT, (WPARAM)hFontHeader_, TRUE);

    // Copy Hash Button (Screen 3)
    hBtnCopyHash_ = CreateWindowExW(
        0, L"BUTTON", L"Copy SHA-256",
        WS_CHILD | BS_PUSHBUTTON,
        345, 425, 140, 44,
        hwnd_, (HMENU)ID_BTN_COPY_HASH, hInstance_, nullptr
    );
    SendMessageW(hBtnCopyHash_, WM_SETFONT, (WPARAM)hFontHeader_, TRUE);

    // New Capture Button (Screen 3)
    hBtnNewCapture_ = CreateWindowExW(
        0, L"BUTTON", L"New Capture",
        WS_CHILD | BS_PUSHBUTTON,
500, 425, 140, 44,
        hwnd_, (HMENU)ID_BTN_NEW_CAPTURE, hInstance_, nullptr
    );
    SendMessageW(hBtnNewCapture_, WM_SETFONT, (WPARAM)hFontHeader_, TRUE);
}

void PhylaMainWindow::SetState(GuiState newState)
{
    state_ = newState;

    bool isReady = (state_ == GuiState::Ready);
    bool isAcquiring = (state_ == GuiState::Acquiring);
    bool isComplete = (state_ == GuiState::Complete);

    ShowWindow(hEditDest_, isReady ? SW_SHOW : SW_HIDE);
    ShowWindow(hBtnBrowse_, isReady ? SW_SHOW : SW_HIDE);
    ShowWindow(hChkSha256_, isReady ? SW_SHOW : SW_HIDE);
    ShowWindow(hComboRate_, isReady ? SW_SHOW : SW_HIDE);
    ShowWindow(hBtnStart_, isReady ? SW_SHOW : SW_HIDE);
    ShowWindow(hBtnDryRun_, isReady ? SW_SHOW : SW_HIDE);

    ShowWindow(hBtnCancel_, isAcquiring ? SW_SHOW : SW_HIDE);

    ShowWindow(hBtnOpenFolder_, isComplete ? SW_SHOW : SW_HIDE);
    ShowWindow(hBtnCopyHash_, isComplete ? SW_SHOW : SW_HIDE);
    ShowWindow(hBtnNewCapture_, (isComplete || state_ == GuiState::Error) ? SW_SHOW : SW_HIDE);

    InvalidateRect(hwnd_, nullptr, TRUE);
}

void PhylaMainWindow::UpdateDiskFreeSpace()
{
    wchar_t buf[MAX_PATH];
    GetWindowTextW(hEditDest_, buf, MAX_PATH);
    destPath_ = buf;

    uint64_t totalDisk = 0;
    uint64_t freeBytes = GetDiskFree(destPath_, totalDisk);

    double freeGb = static_cast<double>(freeBytes) / (1024.0 * 1024.0 * 1024.0);
    double totalGb = static_cast<double>(totalDisk) / (1024.0 * 1024.0 * 1024.0);

    diskSpaceOk_ = (freeBytes > (totalRamBytes_ + (1024ull * 1024ull * 1024ull))); // RAM + 1GB safety margin

    std::wostringstream ss;
    ss << std::fixed << std::setprecision(1) << freeGb << L" GB Free of " << totalGb << L" GB · ";
    if (diskSpaceOk_) {
        ss << L"Space Confirmed ✔";
    } else {
        ss << L"⚠️ Insufficient Disk Space!";
    }
    diskSpaceText_ = ss.str();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void PhylaMainWindow::StartAcquisitionWorker(bool isDryRunOnly)
{
    if (workerThread_.joinable()) {
        workerThread_.join();
    }

    workerCancelled_.store(false);
    SetState(GuiState::Acquiring);

    wchar_t pathBuf[MAX_PATH];
    GetWindowTextW(hEditDest_, pathBuf, MAX_PATH);
    std::wstring finalRawPath = pathBuf;
    bool calcHash = (Button_GetCheck(hChkSha256_) == BST_CHECKED);

    int rateSel = ComboBox_GetCurSel(hComboRate_);
    uint32_t rateLimit = 0;
    if (rateSel == 1) rateLimit = 500;
    else if (rateSel == 2) rateLimit = 250;
    else if (rateSel == 3) rateLimit = 100;

    workerThread_ = std::thread([this, finalRawPath, calcHash, rateLimit, isDryRunOnly]() {
        if (!IsAdministrator()) {
            PostMessageW(hwnd_, WM_PHYLA_ERROR, 0, (LPARAM) new std::wstring(L"PhylaRAM requires elevated Administrator privileges."));
            return;
        }

        std::wstring reason;
        if (!IsSupportedWindows(reason)) {
            PostMessageW(hwnd_, WM_PHYLA_ERROR, 0, (LPARAM) new std::wstring(reason));
            return;
        }

        std::wstring driverPath;
        if (!ExtractEmbeddedDriver(driverPath)) {
            PostMessageW(hwnd_, WM_PHYLA_ERROR, 0, (LPARAM) new std::wstring(L"Failed to extract embedded kernel driver resource."));
            return;
        }

        std::wstring serviceError;
        if (!InstallAndStartDriver(driverPath, serviceError)) {
            DeleteFileW(driverPath.c_str());
            PostMessageW(hwnd_, WM_PHYLA_ERROR, 0, (LPARAM) new std::wstring(L"Failed to start driver service: " + serviceError));
            return;
        }

        DeviceSession device;
        if (!device.Open()) {
            StopAndDeleteDriver();
            PostMessageW(hwnd_, WM_PHYLA_ERROR, 0, (LPARAM) new std::wstring(L"Unable to establish handle to \\\\.\\PhylaRAM. (Error " + std::to_wstring(device.LastError()) + L")"));
            return;
        }

        if (isDryRunOnly) {
            uint64_t highestEnd = 0, totalBytes = 0;
            std::vector<MemoryRun> runs;
            device.Query(highestEnd, totalBytes, runs);
            device.QueryHints(hints_);

            ReadResult rrSample;
            phylaram::WaveletEntropyMetrics* pEntropy = new phylaram::WaveletEntropyMetrics();
            if (!runs.empty() && device.Read(0, 0, 4096, rrSample) && rrSample.copied > 0) {
                *pEntropy = phylaram::AnalyzeWaveletEntropy(rrSample.data.data(), rrSample.copied);
            }
            StopAndDeleteDriver();
            PostMessageW(hwnd_, WM_PHYLA_DRYRUN_DONE, 0, (LPARAM)pEntropy);
            return;
        }

        std::wstring rawPartial = finalRawPath + L".partial";
        std::wstring mapFinal = finalRawPath + L".map.json";
        std::wstring mapPartial = finalRawPath + L".map.json.partial";
        std::wstring hashFinal = finalRawPath + L".sha256";
        std::wstring hashPartial = finalRawPath + L".sha256.partial";

        RawWriter writer;
        Sha256 hasher;
        if (calcHash && !hasher.Initialize()) {
            StopAndDeleteDriver();
            PostMessageW(hwnd_, WM_PHYLA_ERROR, 0, (LPARAM) new std::wstring(L"Failed to initialize CNG SHA-256 cryptographic provider."));
            return;
        }

        AcquisitionSummary summary;
        AcquisitionConfig config;
        config.quiet = true;
        config.rateLimitMBps = rateLimit;
        config.onProgress = [](uint64_t acq, uint64_t tot, double spd, uint32_t eta, void* data) {
            HWND h = static_cast<HWND>(data);
            ProgressPayload* p = new ProgressPayload{acq, tot, spd, eta};
            PostMessageW(h, WM_PHYLA_PROGRESS, 0, (LPARAM)p);
        };
        config.callbackData = hwnd_;

        auto startTime = std::chrono::steady_clock::now();
        bool success = Acquire(device, writer, calcHash ? &hasher : nullptr, summary, workerCancelled_, config);

        bool topologyChanged = false;
        device.End(topologyChanged);
        summary.topologyChanged = topologyChanged;

        StopAndDeleteDriver();

        if (!success || workerCancelled_.load()) {
            writer.Close();
            DeleteFileW(rawPartial.c_str());
            DeleteFileW(mapPartial.c_str());
            DeleteFileW(hashPartial.c_str());
            if (workerCancelled_.load()) {
                PostMessageW(hwnd_, WM_PHYLA_ERROR, 0, (LPARAM) new std::wstring(L"Memory acquisition was cancelled by user."));
            } else {
                PostMessageW(hwnd_, WM_PHYLA_ERROR, 0, (LPARAM) new std::wstring(L"Acquisition failed due to device or I/O error."));
            }
            return;
        }

        std::string shaHex;
        if (calcHash) {
            hasher.Finish(shaHex);
            summary.sha256 = shaHex;
        }

        // Write Map & SHA Sidecars
        WriteMapJson(mapPartial, summary);
        if (calcHash) {
            std::filesystem::path p(finalRawPath);
            WriteSha256Sidecar(hashPartial, p.filename().wstring(), shaHex);
        }

        // Atomic file promotion
        PromoteStagingFile(rawPartial, finalRawPath);
        PromoteStagingFile(mapPartial, mapFinal);
        if (calcHash) {
            PromoteStagingFile(hashPartial, hashFinal);
        }

        auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime).count();
        double totalSecs = static_cast<double>(elapsedMs) / 1000.0;
        double avgSpeed = (totalSecs > 0.0) ? (static_cast<double>(summary.acquiredBytes) / (1024.0 * 1024.0)) / totalSecs : 0.0;

        CompletePayload* comp = new CompletePayload{summary, finalRawPath, shaHex, totalSecs, avgSpeed};
        PostMessageW(hwnd_, WM_PHYLA_COMPLETE, 0, (LPARAM)comp);
    });
}

void PhylaMainWindow::CancelAcquisition()
{
    workerCancelled_.store(true);
}

void PhylaMainWindow::DrawCard(HDC hdc, const RECT& rc, COLORREF bgCol, COLORREF borderCol, int radius)
{
    HBRUSH hBg = CreateSolidBrush(bgCol);
    HPEN hPen = CreatePen(PS_SOLID, 1, borderCol);
    HGDIOBJ oldBg = SelectObject(hdc, hBg);
    HGDIOBJ oldPen = SelectObject(hdc, hPen);

    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, radius, radius);

    SelectObject(hdc, oldBg);
    SelectObject(hdc, oldPen);
    DeleteObject(hBg);
    DeleteObject(hPen);
}

void PhylaMainWindow::OnPaint(HDC hdc, const RECT& clientRc)
{
    // Double buffered drawing to eliminate flicker
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, clientRc.right, clientRc.bottom);
    HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

    FillRect(memDC, &clientRc, hBrBack_);
    SetBkMode(memDC, TRANSPARENT);

    // 1. Header Banner
    SelectObject(memDC, hFontTitle_);
    SetTextColor(memDC, RGB(255, 255, 255));
    TextOutW(memDC, 30, 24, L"PhylaRAM", 8);

    SelectObject(memDC, hFontBody_);
    SetTextColor(memDC, RGB(74, 158, 255)); // Accent blue
    TextOutW(memDC, 155, 30, L"v0.1.0-alpha", 12);

    SetTextColor(memDC, RGB(157, 165, 180));
    TextOutW(memDC, 30, 56, L"Live Kernel Physical Memory Acquisition for Windows", 51);

    if (state_ == GuiState::Ready) {
        // Card 1: RAM Topology Summary
        RECT rcCard1{ 30, 90, 650, 165 };
        DrawCard(memDC, rcCard1, RGB(33, 37, 43), RGB(50, 56, 66), 10);

        SelectObject(memDC, hFontHeader_);
        SetTextColor(memDC, RGB(255, 255, 255));
        TextOutW(memDC, 50, 105, L"System Memory Topology", 22);

        SelectObject(memDC, hFontBody_);
        SetTextColor(memDC, RGB(200, 205, 215));
        double ramGb = static_cast<double>(totalRamBytes_) / (1024.0 * 1024.0 * 1024.0);
        std::wostringstream ssRam;
        ssRam << std::fixed << std::setprecision(1) << ramGb << L" GB Physical Memory Detected · Flat RAW Mapping";
        TextOutW(memDC, 50, 130, ssRam.str().c_str(), static_cast<int>(ssRam.str().length()));

        // Label: Save Evidence To
        SelectObject(memDC, hFontBold_);
        SetTextColor(memDC, RGB(255, 255, 255));
        TextOutW(memDC, 30, 185, L"Save Evidence Image Destination:", 32);

        // Disk space status
        SelectObject(memDC, hFontBody_);
        SetTextColor(memDC, diskSpaceOk_ ? RGB(0, 204, 106) : RGB(248, 81, 73));
        TextOutW(memDC, 30, 245, diskSpaceText_.c_str(), static_cast<int>(diskSpaceText_.length()));

        // Options Header
        SelectObject(memDC, hFontBold_);
        SetTextColor(memDC, RGB(255, 255, 255));
        TextOutW(memDC, 30, 280, L"", 0);
    }
    else if (state_ == GuiState::Acquiring) {
        // Card: Live Acquisition Progress
        RECT rcCard{ 30, 100, 650, 390 };
        DrawCard(memDC, rcCard, RGB(33, 37, 43), RGB(50, 56, 66), 12);

        SelectObject(memDC, hFontHeader_);
        SetTextColor(memDC, RGB(255, 255, 255));
        TextOutW(memDC, 50, 120, L"Acquiring Physical RAM...", 25);

        // Progress Bar
        RECT rcProgBack{ 50, 155, 630, 185 };
        DrawCard(memDC, rcProgBack, RGB(24, 26, 31), RGB(60, 66, 76), 6);

        uint64_t total = (liveTotal_ > 0) ? liveTotal_ : 1;
        int progWidth = static_cast<int>((liveAcquired_ * (rcProgBack.right - rcProgBack.left)) / total);
        if (progWidth > 0) {
            RECT rcProgFill{ rcProgBack.left, rcProgBack.top, rcProgBack.left + progWidth, rcProgBack.bottom };
            DrawCard(memDC, rcProgFill, RGB(0, 120, 212), RGB(74, 158, 255), 6);
        }

        // Percentage & Transfer metrics
        uint64_t percent = (liveAcquired_ * 100) / total;
        std::wostringstream ssMet;
        ssMet << percent << L"%  [" << (liveAcquired_ / (1024 * 1024)) << L" / " << (liveTotal_ / (1024 * 1024)) << L" MiB]";
        SelectObject(memDC, hFontBold_);
        SetTextColor(memDC, RGB(255, 255, 255));
        TextOutW(memDC, 50, 200, ssMet.str().c_str(), static_cast<int>(ssMet.str().length()));

        std::wostringstream ssSpd;
        uint32_t etaM = liveEta_ / 60;
        uint32_t etaS = liveEta_ % 60;
        ssSpd << L"Throughput: " << std::fixed << std::setprecision(1) << liveSpeed_ << L" MB/s   ·   ETA: "
              << std::setfill(L'0') << std::setw(2) << etaM << L":" << std::setw(2) << etaS;
        SelectObject(memDC, hFontBody_);
        SetTextColor(memDC, RGB(74, 158, 255));
        TextOutW(memDC, 50, 230, ssSpd.str().c_str(), static_cast<int>(ssSpd.str().length()));

        // Reassurance text
        SetTextColor(memDC, RGB(157, 165, 180));
        TextOutW(memDC, 50, 270, L"Please keep this system powered on while memory is securely captured.", 69);
        TextOutW(memDC, 50, 292, L"Direct DMA-safe kernel streaming to disk via MmCopyMemory.", 58);
    }
    else if (state_ == GuiState::Complete) {
        // Card: Secured Evidence Summary
        RECT rcCard{ 30, 100, 650, 390 };
        DrawCard(memDC, rcCard, RGB(33, 37, 43), RGB(16, 124, 65), 12);

        SelectObject(memDC, hFontTitle_);
        SetTextColor(memDC, RGB(0, 204, 106)); // Emerald green
        TextOutW(memDC, 50, 120, L"✔ Physical Memory Secured", 25);

        SelectObject(memDC, hFontBody_);
        SetTextColor(memDC, RGB(200, 205, 215));
        TextOutW(memDC, 50, 155, L"Forensic memory image and cryptographic sidecars successfully created:", 70);

        // Result details
        SelectObject(memDC, hFontMono_);
        SetTextColor(memDC, RGB(255, 255, 255));
        std::wostringstream ssRaw;
        ssRaw << L"• RAW Image : " << finalResult_.rawPath;
        TextOutW(memDC, 50, 190, ssRaw.str().c_str(), static_cast<int>(ssRaw.str().length()));

        std::wostringstream ssMap;
        ssMap << L"• Map JSON  : " << finalResult_.rawPath << L".map.json";
        TextOutW(memDC, 50, 215, ssMap.str().c_str(), static_cast<int>(ssMap.str().length()));

        std::wostringstream ssSha;
        ssSha << L"• SHA-256   : " << std::wstring(finalResult_.sha256Hex.begin(), finalResult_.sha256Hex.end());
        TextOutW(memDC, 50, 240, ssSha.str().c_str(), static_cast<int>(ssSha.str().length()));

        std::wostringstream ssPerf;
        ssPerf << L"• Avg Speed : " << std::fixed << std::setprecision(1) << finalResult_.avgSpeedMBs << L" MB/s (Elapsed: "
               << std::fixed << std::setprecision(2) << finalResult_.totalSeconds << L"s)";
        TextOutW(memDC, 50, 265, ssPerf.str().c_str(), static_cast<int>(ssPerf.str().length()));

        SelectObject(memDC, hFontBody_);
        SetTextColor(memDC, RGB(157, 165, 180));
        TextOutW(memDC, 50, 310, L"Evidence bundle is verified and ready for Volatility 3, MemProcFS, or your IR team.", 83);
    }
    else if (state_ == GuiState::Error) {
        RECT rcCard{ 30, 100, 650, 390 };
        DrawCard(memDC, rcCard, RGB(33, 37, 43), RGB(248, 81, 73), 12);

        SelectObject(memDC, hFontTitle_);
        SetTextColor(memDC, RGB(248, 81, 73));
        TextOutW(memDC, 50, 120, L"⚠️ Acquisition Incomplete", 25);

        SelectObject(memDC, hFontBody_);
        SetTextColor(memDC, RGB(255, 255, 255));
        TextOutW(memDC, 50, 160, errorMessage_.c_str(), static_cast<int>(errorMessage_.length()));

        SetTextColor(memDC, RGB(157, 165, 180));
        TextOutW(memDC, 50, 210, L"Please verify that:", 19);
        TextOutW(memDC, 65, 235, L"1. The application was launched with Administrator privileges.", 62);
        TextOutW(memDC, 65, 260, L"2. Test-signing mode is enabled if using test-signed pre-release drivers.", 73);
        TextOutW(memDC, 65, 285, L"3. Target drive has sufficient free disk space and write permissions.", 69);
    }

    BitBlt(hdc, 0, 0, clientRc.right, clientRc.bottom, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);
}

LRESULT PhylaMainWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd_, &ps);
        RECT rc;
        GetClientRect(hwnd_, &rc);
        OnPaint(hdc, rc);
        EndPaint(hwnd_, &ps);
        return 0;
    }

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        int code = HIWORD(wParam);

        if (id == ID_BTN_BROWSE && code == BN_CLICKED) {
            wchar_t fileBuf[MAX_PATH];
            GetWindowTextW(hEditDest_, fileBuf, MAX_PATH);

            OPENFILENAMEW ofn{};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hwnd_;
            ofn.lpstrFilter = L"RAW Physical Memory Image (*.raw)\0*.raw\0All Files (*.*)\0*.*\0";
            ofn.lpstrFile = fileBuf;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
            ofn.lpstrDefExt = L"raw";

            if (GetSaveFileNameW(&ofn)) {
                SetWindowTextW(hEditDest_, fileBuf);
                UpdateDiskFreeSpace();
            }
            return 0;
        }

        if (id == ID_EDIT_DESTINATION && code == EN_CHANGE) {
            UpdateDiskFreeSpace();
            return 0;
        }

        if (id == ID_BTN_START && code == BN_CLICKED) {
            StartAcquisitionWorker(false);
            return 0;
        }

        if (id == ID_BTN_DRYRUN && code == BN_CLICKED) {
            StartAcquisitionWorker(true);
            return 0;
        }

        if (id == ID_BTN_CANCEL && code == BN_CLICKED) {
            CancelAcquisition();
            return 0;
        }

        if (id == ID_BTN_OPEN_FOLDER && code == BN_CLICKED) {
            // Open Explorer selecting the raw memory image
            std::wstring param = L"/select,\"" + finalResult_.rawPath + L"\"";
            ShellExecuteW(nullptr, L"open", L"explorer.exe", param.c_str(), nullptr, SW_SHOWNORMAL);
            return 0;
        }

        if (id == ID_BTN_COPY_HASH && code == BN_CLICKED) {
            if (OpenClipboard(hwnd_)) {
                EmptyClipboard();
                size_t len = finalResult_.sha256Hex.length() + 1;
                HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
                if (hMem) {
                    memcpy(GlobalLock(hMem), finalResult_.sha256Hex.c_str(), len);
                    GlobalUnlock(hMem);
                    SetClipboardData(CF_TEXT, hMem);
                }
                CloseClipboard();
                MessageBoxW(hwnd_, L"SHA-256 digest copied to clipboard!", L"PhylaRAM", MB_OK | MB_ICONINFORMATION);
            }
            return 0;
        }

        if (id == ID_BTN_NEW_CAPTURE && code == BN_CLICKED) {
            destPath_ = GetDefaultDestination();
            SetWindowTextW(hEditDest_, destPath_.c_str());
            SetState(GuiState::Ready);
            UpdateDiskFreeSpace();
            return 0;
        }
        break;
    }

    case WM_PHYLA_PROGRESS: {
        ProgressPayload* p = reinterpret_cast<ProgressPayload*>(lParam);
        if (p) {
            liveAcquired_ = p->acquired;
            liveTotal_ = p->total;
            liveSpeed_ = p->speedMBs;
            liveEta_ = p->etaSeconds;
            delete p;
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
        return 0;
    }

    case WM_PHYLA_COMPLETE: {
        CompletePayload* p = reinterpret_cast<CompletePayload*>(lParam);
        if (p) {
            finalResult_ = *p;
            delete p;
            SetState(GuiState::Complete);
        }
        return 0;
    }

    case WM_PHYLA_ERROR: {
        std::wstring* pErr = reinterpret_cast<std::wstring*>(lParam);
        if (pErr) {
            errorMessage_ = *pErr;
            delete pErr;
            SetState(GuiState::Error);
        }
        return 0;
    }

    case WM_PHYLA_DRYRUN_DONE: {
        SetState(GuiState::Ready);
        phylaram::WaveletEntropyMetrics* pEnt = reinterpret_cast<phylaram::WaveletEntropyMetrics*>(lParam);
        std::wostringstream ss;
        ss << L"PhylaRAM Dry-Run Telemetry & Wavelet Triage Completed:\n\n"
           << L"• System DTB (CR3) : 0x" << std::hex << std::uppercase << hints_.directoryTableBase << std::dec << L"\n"
           << L"• Executing KPCR   : 0x" << std::hex << std::uppercase << hints_.kpcrAddress << std::dec << L"\n"
           << L"• NTOSKRNL Base    : 0x" << std::hex << std::uppercase << hints_.kernelBase << std::dec << L"\n"
           << L"• Windows Build    : " << hints_.buildNumber << L"\n"
           << L"• Hypervisor       : " << (hints_.hypervisorPresent ? L"Present" : L"None") << L"\n";
        if (pEnt && pEnt->totalBytesAnalyzed > 0) {
            ss << L"\n[Wavelet Transition Triage]\n"
               << L"• Identity Density : " << std::fixed << std::setprecision(1) << (pEnt->identityDensity * 100.0f) << L"%\n"
               << L"• Transition Energy: " << std::fixed << std::setprecision(3) << pEnt->transitionEnergy << L"\n"
               << L"• Predictability   : " << std::fixed << std::setprecision(1) << (pEnt->predictionConfidence * 100.0f) << L"%\n"
               << L"• SGH5 Orbit Hash  : 0x" << std::hex << std::uppercase << pEnt->orbitHash << std::dec << L"\n"
               << L"• Classification   : " << std::wstring(pEnt->categoryName.begin(), pEnt->categoryName.end()) << L"\n";
        }
        if (pEnt) delete pEnt;
        MessageBoxW(hwnd_, ss.str().c_str(), L"PhylaRAM Dry-Run Telemetry", MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    case WM_CTLCOLORSTATIC: {
        HDC hdcStatic = (HDC)wParam;
        SetTextColor(hdcStatic, RGB(255, 255, 255));
        SetBkColor(hdcStatic, RGB(24, 26, 31));
        return (LRESULT)hBrBack_;
    }

    case WM_DESTROY:
        if (workerThread_.joinable()) {
            workerCancelled_.store(true);
            workerThread_.join();
        }
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd_, msg, wParam, lParam);
}

LRESULT CALLBACK PhylaMainWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (g_pMainWnd) {
        return g_pMainWnd->HandleMessage(msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int LaunchGui(HINSTANCE hInstance)
{
    // Initialize Windows Common Controls v6
    INITCOMMONCONTROLSEX icce{};
    icce.dwSize = sizeof(icce);
    icce.dwICC = ICC_STANDARD_CLASSES | ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&icce);

    PhylaMainWindow mainWnd;
    if (!mainWnd.Create(hInstance)) {
        return 1;
    }

    mainWnd.Show(SW_SHOWNORMAL);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return static_cast<int>(msg.wParam);
}
