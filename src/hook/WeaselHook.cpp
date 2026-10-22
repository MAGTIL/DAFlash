#include "WeaselHook.h"
#include <MinHook.h>
#include <dwrite.h>
#include <iostream>
#include <fstream>

namespace DAFlash {

void LogHook(const std::string& msg) {
    WCHAR path[MAX_PATH];
    HMODULE hMod = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&LogHook), &hMod);
    GetModuleFileNameW(hMod, path, MAX_PATH);
    std::wstring p(path);
    size_t idx = p.find_last_of(L"\\/");
    if (idx != std::wstring::npos) p = p.substr(0, idx + 1);
    p += L"hook.log";

    HANDLE hFile = CreateFileW(
        p.c_str(),
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    if (hFile != INVALID_HANDLE_VALUE) {
        std::string line = msg + "\r\n";
        DWORD written = 0;
        WriteFile(hFile, line.c_str(), static_cast<DWORD>(line.length()), &written, nullptr);
        CloseHandle(hFile);
    }
    OutputDebugStringA(("[DAFlashHook] " + msg + "\n").c_str());
}

static WeaselHookManager* g_pManager = nullptr;

// Function pointer types
typedef HWND (WINAPI* CreateWindowExW_t)(
    DWORD dwExStyle, LPCWSTR lpClassName, LPCWSTR lpWindowName, DWORD dwStyle,
    int X, int Y, int nWidth, int nHeight,
    HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam);

typedef HRESULT (STDMETHODCALLTYPE* CreateTextLayout_t)(
    IDWriteFactory* This,
    const WCHAR* string,
    UINT32 len,
    IDWriteTextFormat* format,
    FLOAT max_width,
    FLOAT max_height,
    IDWriteTextLayout** layout);

typedef uintptr_t RimeSessionId_t;
typedef int RimeBool_t;
typedef struct {
    int data_size;
    char* text;
} RimeCommit_t;

typedef RimeBool_t (*RimeGetCommit_t)(RimeSessionId_t session_id, RimeCommit_t* commit);

typedef BOOL (WINAPI* ReadFile_t)(
    HANDLE hFile,
    LPVOID lpBuffer,
    DWORD nNumberOfBytesToRead,
    LPDWORD lpNumberOfBytesRead,
    LPOVERLAPPED lpOverlapped);

static CreateWindowExW_t fpCreateWindowExW = nullptr;
static CreateTextLayout_t fpCreateTextLayout = nullptr;
static RimeGetCommit_t fpRimeGetCommit = nullptr;
static ReadFile_t fpReadFile = nullptr;

static BOOL WINAPI Hooked_ReadFile(
    HANDLE hFile,
    LPVOID lpBuffer,
    DWORD nNumberOfBytesToRead,
    LPDWORD lpNumberOfBytesRead,
    LPOVERLAPPED lpOverlapped) {

    BOOL res = fpReadFile(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped);
    if (res && lpBuffer && lpNumberOfBytesRead && *lpNumberOfBytesRead >= 12) {
        // Weasel PipeMessage: struct { uint32 Msg; uint32 wParam; uint32 lParam; }
        const uint32_t* pMsg = reinterpret_cast<const uint32_t*>(lpBuffer);
        uint32_t msgType = pMsg[0];
        uint32_t wParam = pMsg[1];

        // WEASEL_IPC_UPDATE_INPUT_POS = 32776 (0x8008)
        if (msgType == 32776) {
            int hi_res = (wParam >> 31) & 0x01;
            int left = ((wParam & 0x7ff) - (wParam & 0x800)) << hi_res;
            int top = (((wParam >> 12) & 0x7ff) - ((wParam >> 12) & 0x800)) << hi_res;
            int height = ((wParam >> 24) & 0x7f) << hi_res;

            if (g_pManager) {
                g_pManager->OnInputPositionUpdated(left, top, height > 0 ? height : 28);
            }
        }
    }
    return res;
}

static RimeBool_t Hooked_RimeGetCommit(RimeSessionId_t session_id, RimeCommit_t* commit) {
    RimeBool_t res = fpRimeGetCommit(session_id, commit);
    if (res && commit && commit->text && g_pManager) {
        g_pManager->OnRimeCommit(commit->text);
    }
    return res;
}

// Detour for CreateWindowExW
static HWND WINAPI Hooked_CreateWindowExW(
    DWORD dwExStyle, LPCWSTR lpClassName, LPCWSTR lpWindowName, DWORD dwStyle,
    int X, int Y, int nWidth, int nHeight,
    HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam) {

    HWND hWnd = fpCreateWindowExW(
        dwExStyle, lpClassName, lpWindowName, dwStyle,
        X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);

    if (hWnd && lpClassName && !IS_INTRESOURCE(lpClassName)) {
        if (wcscmp(lpClassName, L"WeaselPanel") == 0) {
            LogHook("Hooked_CreateWindowExW: WeaselPanel created");
            if (g_pManager) {
                g_pManager->OnWeaselPanelCreated(hWnd);
            }
        }
    }
    return hWnd;
}

// Detour for IDWriteFactory::CreateTextLayout
static HRESULT STDMETHODCALLTYPE Hooked_CreateTextLayout(
    IDWriteFactory* This,
    const WCHAR* string,
    UINT32 len,
    IDWriteTextFormat* format,
    FLOAT max_width,
    FLOAT max_height,
    IDWriteTextLayout** layout) {

    if (g_pManager && string && len > 0) {
        g_pManager->OnCreateTextLayout(string, len, max_width, max_height);
    }

    return fpCreateTextLayout(This, string, len, format, max_width, max_height, layout);
}

// Subclass window procedure for WeaselPanel
LRESULT CALLBACK WeaselHookManager::WeaselPanelSubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    WeaselHookManager& mgr = WeaselHookManager::Instance();

    switch (msg) {
    case WM_PAINT: {
        mgr.BeginPaintCycle();
        LRESULT res = CallWindowProcW(mgr.originalWndProc_, hWnd, msg, wParam, lParam);
        mgr.EndPaintCycle(hWnd);
        return res;
    }
    case WM_SHOWWINDOW: {
        bool visible = (wParam == TRUE);
        mgr.OnWeaselPanelVisibility(hWnd, visible);
        break;
    }
    case WM_WINDOWPOSCHANGED: {
        RECT rc;
        if (GetWindowRect(hWnd, &rc)) {
            mgr.OnWeaselPanelPosChanged(hWnd, rc);
        }
        break;
    }
    case WM_DESTROY: {
        mgr.OnWeaselPanelDestroyed(hWnd);
        break;
    }
    default:
        break;
    }

    if (mgr.originalWndProc_) {
        return CallWindowProcW(mgr.originalWndProc_, hWnd, msg, wParam, lParam);
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

WeaselHookManager& WeaselHookManager::Instance() {
    static WeaselHookManager instance;
    return instance;
}

WeaselHookManager::WeaselHookManager() {
    g_pManager = this;
}

WeaselHookManager::~WeaselHookManager() {
    Shutdown();
    g_pManager = nullptr;
}

bool WeaselHookManager::Initialize() {
    // 1. Setup shared memory & IPC
    hSharedMem_ = CreateFileMappingW(
        INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(IpcData), SHARED_MEM_NAME);
    if (!hSharedMem_) return false;

    pIpcData_ = static_cast<IpcData*>(MapViewOfFile(hSharedMem_, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(IpcData)));
    if (!pIpcData_) return false;

    hMutex_ = CreateMutexW(nullptr, FALSE, MUTEX_NAME);
    hEvent_ = CreateEventW(nullptr, FALSE, FALSE, EVENT_NAME);

    // Initialize MinHook
    if (MH_Initialize() != MH_OK) {
        return false;
    }

    // 2. Hook CreateWindowExW to catch WeaselPanel
    MH_CreateHookApi(L"user32.dll", "CreateWindowExW",
                     reinterpret_cast<LPVOID>(&Hooked_CreateWindowExW),
                     reinterpret_cast<LPVOID*>(&fpCreateWindowExW));

    // 3. Hook IDWriteFactory::CreateTextLayout
    IDWriteFactory* pFactory = nullptr;
    HRESULT hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(&pFactory)
    );

    if (SUCCEEDED(hr) && pFactory) {
        // Retrieve pointer from vtable
        void** vtbl = *reinterpret_cast<void***>(pFactory);
        // CreateTextLayout is member at vtbl index in IDWriteFactory
        // In DWrite.h: QueryInterface(0), AddRef(1), Release(2)...
        // We can inspect pFactory vtable or use pFactory->lpVtbl in C mode
        // In MSVC/MinGW C++ layout:
        // Index 18 is CreateTextLayout (0-2: IUnknown, 3-17: earlier IDWriteFactory methods)
        void* pCreateTextLayout = vtbl[18];
        
        MH_CreateHook(pCreateTextLayout,
                      reinterpret_cast<LPVOID>(&Hooked_CreateTextLayout),
                      reinterpret_cast<LPVOID*>(&fpCreateTextLayout));

        pFactory->Release();
    }

    // 4. Hook RimeGetCommit in rime.dll
    HMODULE hRime = GetModuleHandleW(L"rime.dll");
    if (!hRime) {
        hRime = LoadLibraryW(L"rime.dll");
    }
    if (hRime) {
        void* pRimeGetCommit = reinterpret_cast<void*>(GetProcAddress(hRime, "RimeGetCommit"));
        if (pRimeGetCommit) {
            MH_STATUS s = MH_CreateHook(pRimeGetCommit,
                                        reinterpret_cast<LPVOID>(&Hooked_RimeGetCommit),
                                        reinterpret_cast<LPVOID*>(&fpRimeGetCommit));
            if (s == MH_OK) {
                LogHook("Hooked RimeGetCommit in rime.dll successfully");
            } else {
                LogHook("MH_CreateHook for RimeGetCommit failed: " + std::to_string(s));
            }
        } else {
            LogHook("RimeGetCommit export not found in rime.dll");
        }
    } else {
        LogHook("rime.dll not loaded in process");
    }

    // 5. Hook ReadFile in kernel32.dll to intercept Weasel Named Pipe IPC (WEASEL_IPC_UPDATE_INPUT_POS)
    MH_CreateHookApi(L"kernel32.dll", "ReadFile",
                     reinterpret_cast<LPVOID>(&Hooked_ReadFile),
                     reinterpret_cast<LPVOID*>(&fpReadFile));
    LogHook("Hooked ReadFile in kernel32.dll for Weasel IPC interception");

    // Enable all installed hooks
    MH_EnableHook(MH_ALL_HOOKS);

    // 跨线程枚举当前进程所有窗口，查找已存在的 WeaselPanel
    EnumWindows([](HWND hWnd, LPARAM lParam) -> BOOL {
        DWORD pid = 0;
        GetWindowThreadProcessId(hWnd, &pid);
        if (pid == GetCurrentProcessId()) {
            WCHAR cls[64] = {0};
            if (GetClassNameW(hWnd, cls, 64) && wcscmp(cls, L"WeaselPanel") == 0) {
                auto* self = reinterpret_cast<WeaselHookManager*>(lParam);
                self->OnWeaselPanelCreated(hWnd);
                return FALSE;
            }
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(this));

    return true;
}

void WeaselHookManager::Shutdown() {
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();

    if (hWeaselPanel_ && originalWndProc_) {
        SetWindowLongPtrW(hWeaselPanel_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(originalWndProc_));
        hWeaselPanel_ = nullptr;
        originalWndProc_ = nullptr;
    }

    if (pIpcData_) {
        UnmapViewOfFile(pIpcData_);
        pIpcData_ = nullptr;
    }
    if (hSharedMem_) {
        CloseHandle(hSharedMem_);
        hSharedMem_ = nullptr;
    }
    if (hMutex_) {
        CloseHandle(hMutex_);
        hMutex_ = nullptr;
    }
    if (hEvent_) {
        CloseHandle(hEvent_);
        hEvent_ = nullptr;
    }
}

void WeaselHookManager::OnWeaselPanelCreated(HWND hWnd) {
    LogHook("WeaselHookManager::OnWeaselPanelCreated");
    hWeaselPanel_ = hWnd;
    originalWndProc_ = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WeaselPanelSubclassProc))
    );
    GetWindowRect(hWnd, &currentPanelRect_);
    isPanelVisible_ = IsWindowVisible(hWnd);
}

void WeaselHookManager::OnWeaselPanelDestroyed(HWND hWnd) {
    LogHook("WeaselHookManager::OnWeaselPanelDestroyed");
    if (hWeaselPanel_ == hWnd) {
        hWeaselPanel_ = nullptr;
        originalWndProc_ = nullptr;
    }
}

void WeaselHookManager::OnWeaselPanelPosChanged(HWND hWnd, const RECT& screenRect) {
    currentPanelRect_ = screenRect;
}

void WeaselHookManager::OnWeaselPanelVisibility(HWND hWnd, bool visible) {
    bool wasVisible = isPanelVisible_;
    isPanelVisible_ = visible;

    if (wasVisible && !visible) {
        // Window transitioned from shown to hidden
        if (!lastCommittedCandidate_.empty()) {
            if (pIpcData_ && hMutex_) {
                WaitForSingleObject(hMutex_, 50);
                pIpcData_->eventType = IpcEventType::CandidateCommitted;
                pIpcData_->sequence++;
                wcsncpy_s(pIpcData_->committedText, lastCommittedCandidate_.c_str(), 63);
                pIpcData_->commitX = lastCommittedX_;
                pIpcData_->commitY = lastCommittedY_;
                pIpcData_->commitW = lastCommittedW_;
                pIpcData_->commitH = lastCommittedH_;
                ReleaseMutex(hMutex_);
                if (hEvent_) SetEvent(hEvent_);
            }
        }
        NotifyIpc(IpcEventType::WindowHidden);
    }
}

void WeaselHookManager::BeginPaintCycle() {
    tempCandidates_.clear();
}

void WeaselHookManager::EndPaintCycle(HWND hWnd) {
    GetWindowRect(hWnd, &currentPanelRect_);
    if (!tempCandidates_.empty()) {
        currentCandidates_ = tempCandidates_;
        lastCommittedCandidate_ = currentCandidates_[0].text;
        lastCommittedX_ = static_cast<float>(currentPanelRect_.left + 15);
        lastCommittedY_ = static_cast<float>(currentPanelRect_.top + 8);
        lastCommittedW_ = currentCandidates_[0].w > 0 ? currentCandidates_[0].w : 60.0f;
        lastCommittedH_ = currentCandidates_[0].h > 0 ? currentCandidates_[0].h : 28.0f;

        if (pIpcData_ && hMutex_) {
            WaitForSingleObject(hMutex_, 50);
            pIpcData_->eventType = IpcEventType::CandidatesShown;
            pIpcData_->sequence++;
            pIpcData_->candidateCount = static_cast<uint32_t>((std::min)(currentCandidates_.size(), (size_t)16));
            pIpcData_->commitX = lastCommittedX_;
            pIpcData_->commitY = lastCommittedY_;
            pIpcData_->commitW = lastCommittedW_;
            pIpcData_->commitH = lastCommittedH_;
            wcsncpy_s(pIpcData_->committedText, lastCommittedCandidate_.c_str(), 63);

            for (uint32_t i = 0; i < pIpcData_->candidateCount; ++i) {
                wcsncpy_s(pIpcData_->candidates[i].text, currentCandidates_[i].text.c_str(), 63);
                pIpcData_->candidates[i].x = lastCommittedX_;
                pIpcData_->candidates[i].y = lastCommittedY_;
                pIpcData_->candidates[i].width = currentCandidates_[i].w;
                pIpcData_->candidates[i].height = currentCandidates_[i].h;
                pIpcData_->candidates[i].isHighlighted = (i == 0);
            }
            ReleaseMutex(hMutex_);
            if (hEvent_) SetEvent(hEvent_);
        }
    }
}

static bool IsCandidateString(const WCHAR* text, UINT32 length) {
    if (!text || length == 0) return false;
    if (length == 1 && (text[0] == L'中' || text[0] == L'英')) return false;
    if (length <= 3 && (text[0] == L'<' || text[0] == L'>' || text[0] == L'.')) return false;
    if (iswdigit(text[0])) {
        if (length <= 3 && (text[1] == L'.' || text[1] == L' ' || text[1] == L'\0')) return false;
    }
    for (UINT32 i = 0; i < length; ++i) {
        if (text[i] >= 0x2E80) return true; // CJK character
    }
    if (length >= 1 && length <= 32 && !iswspace(text[0])) {
        return true;
    }
    return false;
}

void WeaselHookManager::OnCreateTextLayout(const WCHAR* text, UINT32 length, FLOAT maxWidth, FLOAT maxHeight) {
    if (!text || length == 0) return;
    if (!IsCandidateString(text, length)) return;

    std::wstring str(text, length);
    size_t start = 0;
    while (start < str.size() && (iswdigit(str[start]) || str[start] == L'.' || str[start] == L' ')) {
        start++;
    }
    if (start < str.size()) {
        std::wstring cand = str.substr(start);
        tempCandidates_.push_back({ cand, maxWidth, maxHeight });

        if (tempCandidates_.size() == 1) {
            lastCommittedCandidate_ = cand;
            lastCommittedX_ = static_cast<float>(currentPanelRect_.left + 15);
            lastCommittedY_ = static_cast<float>(currentPanelRect_.top + 8);
            lastCommittedW_ = maxWidth > 0 ? maxWidth : 60.0f;
            lastCommittedH_ = maxHeight > 0 ? maxHeight : 28.0f;
        }
    }
}

void WeaselHookManager::OnRimeCommit(const char* utf8Text) {
    if (!utf8Text || !pIpcData_) return;
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8Text, -1, nullptr, 0);
    if (len <= 1) return;

    std::wstring wstr(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8Text, -1, &wstr[0], len);
    wstr.resize(len - 1);

    wcsncpy_s(pIpcData_->committedText, wstr.c_str(), 63);
    pIpcData_->eventType = IpcEventType::CandidateCommitted;
    MemoryBarrier();
    pIpcData_->sequence++;

    if (hEvent_) {
        SetEvent(hEvent_);
    }

    LogHook("OnRimeCommit: " + std::string(utf8Text) + " (seq=" + std::to_string(pIpcData_->sequence) + ") at (" + std::to_string(pIpcData_->commitX) + ", " + std::to_string(pIpcData_->commitY) + ")");
}

void WeaselHookManager::OnInputPositionUpdated(int left, int top, int height) {
    if (!pIpcData_) return;
    pIpcData_->commitX = static_cast<float>(left);
    pIpcData_->commitY = static_cast<float>(top);
    pIpcData_->commitW = 80.0f;
    pIpcData_->commitH = height > 0 ? static_cast<float>(height) : 28.0f;
    pIpcData_->eventType = IpcEventType::CandidatesShown;
    MemoryBarrier();
    pIpcData_->sequence++;
    if (hEvent_) SetEvent(hEvent_);
    LogHook("OnInputPositionUpdated: (" + std::to_string(left) + ", " + std::to_string(top) + ", h=" + std::to_string(height) + ")");
}

void WeaselHookManager::NotifyIpc(IpcEventType type) {
    if (!pIpcData_) return;

    pIpcData_->eventType = type;
    MemoryBarrier();
    pIpcData_->sequence++;

    if (hEvent_) {
        SetEvent(hEvent_);
    }
}

} // namespace DAFlash

