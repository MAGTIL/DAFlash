#include "InputMonitor.h"
#include <uiautomation.h>
#include <oleauto.h>

namespace DAFlash {

static bool IsValidScreenCoord(float x, float y) {
    int minX = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int minY = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int maxX = minX + GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int maxY = minY + GetSystemMetrics(SM_CYVIRTUALSCREEN);
    return (x >= static_cast<float>(minX) && x < static_cast<float>(maxX) &&
            y >= static_cast<float>(minY) && y < static_cast<float>(maxY));
}

static InputMonitor* g_pMonitor = nullptr;

InputMonitor& InputMonitor::Instance() {
    static InputMonitor instance;
    return instance;
}

InputMonitor::InputMonitor() {
    g_pMonitor = this;
}

InputMonitor::~InputMonitor() {
    Shutdown();
    g_pMonitor = nullptr;
}

LRESULT CALLBACK InputMonitor::LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
        auto* pKbd = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        if (g_pMonitor && pKbd) {
            g_pMonitor->OnKeyDown(pKbd->vkCode);
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

bool InputMonitor::Initialize(ParticleManager* pParticleMgr, const EngineConfig* pConfig) {
    pParticleMgr_ = pParticleMgr;
    pConfig_ = pConfig;

    // 1. 初始化 Windows UI Automation 接口 (支持 Chrome / Edge / 微信 / Windows Terminal)
    CoCreateInstance(
        CLSID_CUIAutomation,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_IUIAutomation,
        reinterpret_cast<void**>(&pUIAutomation_)
    );

    // 2. 打开 IPC 共享内存与事件
    hSharedMem_ = OpenFileMappingW(FILE_MAP_READ, FALSE, SHARED_MEM_NAME);
    if (hSharedMem_) {
        pIpcData_ = static_cast<IpcData*>(MapViewOfFile(hSharedMem_, FILE_MAP_READ, 0, 0, sizeof(IpcData)));
    }
    hMutex_ = OpenMutexW(SYNCHRONIZE, FALSE, MUTEX_NAME);
    hEvent_ = OpenEventW(SYNCHRONIZE, FALSE, EVENT_NAME);

    // 3. 安装低级全局键盘钩子
    hKeyboardHook_ = SetWindowsHookExW(
        WH_KEYBOARD_LL,
        LowLevelKeyboardProc,
        GetModuleHandleW(nullptr),
        0
    );

    // 4. 安装系统级 WinEventHook，实时无感监听所有进程中 WeaselPanel 的弹出与移动
    hWinEventHook_ = SetWinEventHook(
        EVENT_OBJECT_SHOW,
        EVENT_OBJECT_LOCATIONCHANGE,
        nullptr,
        WinEventProc,
        0, 0,
        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS
    );

    LogApp("InputMonitor::Initialize: kbdHook=" + std::to_string((uintptr_t)hKeyboardHook_)
           + " winEventHook=" + std::to_string((uintptr_t)hWinEventHook_)
           + " sharedMem=" + std::to_string((uintptr_t)hSharedMem_));

    return (hKeyboardHook_ != nullptr);
}

void InputMonitor::Shutdown() {
    if (hKeyboardHook_) {
        UnhookWindowsHookEx(hKeyboardHook_);
        hKeyboardHook_ = nullptr;
    }
    if (hWinEventHook_) {
        UnhookWinEvent(hWinEventHook_);
        hWinEventHook_ = nullptr;
    }
    if (pUIAutomation_) {
        pUIAutomation_->Release();
        pUIAutomation_ = nullptr;
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

void CALLBACK InputMonitor::WinEventProc(
    HWINEVENTHOOK hWinEventHook,
    DWORD event,
    HWND hwnd,
    LONG idObject,
    LONG idChild,
    DWORD idEventThread,
    DWORD dwmsEventTime
) {
    if (idObject != OBJID_WINDOW || !hwnd) return;

    WCHAR cls[64] = {0};
    if (GetClassNameW(hwnd, cls, 64) > 0 && wcscmp(cls, L"WeaselPanel") == 0) {
        if (event == EVENT_OBJECT_SHOW || event == EVENT_OBJECT_LOCATIONCHANGE) {
            RECT rc;
            if (GetWindowRect(hwnd, &rc) && (rc.right > rc.left && rc.bottom > rc.top)) {
                if (g_pMonitor) {
                    g_pMonitor->cachedX_ = static_cast<float>(rc.left + 15);
                    g_pMonitor->cachedY_ = static_cast<float>(rc.top - 2);
                    g_pMonitor->cachedW_ = static_cast<float>(rc.right - rc.left);
                    g_pMonitor->cachedH_ = static_cast<float>(rc.bottom - rc.top);
                    g_pMonitor->isPanelVisible_ = true;
                    g_pMonitor->lastPanelSeenTime_ = std::chrono::steady_clock::now();
                }
            }
        } else if (event == EVENT_OBJECT_HIDE) {
            if (g_pMonitor) {
                g_pMonitor->isPanelVisible_ = false;
                g_pMonitor->lastPanelSeenTime_ = std::chrono::steady_clock::now();
            }
        }
    }
}

struct WeaselSearchCtx {
    HWND foundHwnd = nullptr;
    RECT foundRect = {0, 0, 0, 0};
};

static BOOL CALLBACK EnumWeaselSearchProc(HWND hWnd, LPARAM lParam) {
    WCHAR cls[64] = {0};
    if (GetClassNameW(hWnd, cls, 64) > 0 && wcscmp(cls, L"WeaselPanel") == 0) {
        LONG style = GetWindowLongW(hWnd, GWL_STYLE);
        if (IsWindowVisible(hWnd) || (style & WS_VISIBLE)) {
            RECT rc;
            if (GetWindowRect(hWnd, &rc) && (rc.right > rc.left && rc.bottom > rc.top)) {
                auto* ctx = reinterpret_cast<WeaselSearchCtx*>(lParam);
                ctx->foundHwnd = hWnd;
                ctx->foundRect = rc;
                return FALSE; // 立即命中并退出
            }
        }
    }
    return TRUE;
}

HWND InputMonitor::FindActiveWeaselPanel(HWND fg) const {
    WeaselSearchCtx ctx;
    EnumWindows(EnumWeaselSearchProc, reinterpret_cast<LPARAM>(&ctx));
    return ctx.foundHwnd;
}

bool InputMonitor::GetCaretViaUIA(POINT& pt) const {
    if (!pUIAutomation_) return false;

    IUIAutomationElement* pFocused = nullptr;
    HRESULT hr = pUIAutomation_->GetFocusedElement(&pFocused);
    if (FAILED(hr) || !pFocused) return false;

    bool found = false;

    // 尝试 TextPattern2 (Windows 8.1+ / Chrome / Edge / Windows Terminal / VS Code)
    IUIAutomationTextPattern2* pTextPattern2 = nullptr;
    hr = pFocused->GetCurrentPatternAs(UIA_TextPattern2Id, IID_IUIAutomationTextPattern2, reinterpret_cast<void**>(&pTextPattern2));
    if (SUCCEEDED(hr) && pTextPattern2) {
        BOOL isActive = FALSE;
        IUIAutomationTextRange* pCaretRange = nullptr;
        hr = pTextPattern2->GetCaretRange(&isActive, &pCaretRange);
        if (SUCCEEDED(hr) && pCaretRange) {
            SAFEARRAY* pRects = nullptr;
            hr = pCaretRange->GetBoundingRectangles(&pRects);
            if (SUCCEEDED(hr) && pRects) {
                double* pData = nullptr;
                if (SUCCEEDED(SafeArrayAccessData(pRects, reinterpret_cast<void**>(&pData)))) {
                    long ubound = 0;
                    SafeArrayGetUBound(pRects, 1, &ubound);
                    if (ubound >= 3 && (pData[2] > 0 || pData[3] > 0)) {
                        pt.x = static_cast<LONG>(pData[0]);
                        pt.y = static_cast<LONG>(pData[1]);
                        found = true;
                    }
                    SafeArrayUnaccessData(pRects);
                }
                SafeArrayDestroy(pRects);
            }
            pCaretRange->Release();
        }
        pTextPattern2->Release();
    }

    if (!found) {
        // 尝试标准 TextPattern GetSelection
        IUIAutomationTextPattern* pTextPattern = nullptr;
        hr = pFocused->GetCurrentPatternAs(UIA_TextPatternId, IID_IUIAutomationTextPattern, reinterpret_cast<void**>(&pTextPattern));
        if (SUCCEEDED(hr) && pTextPattern) {
            IUIAutomationTextRangeArray* pRanges = nullptr;
            hr = pTextPattern->GetSelection(&pRanges);
            if (SUCCEEDED(hr) && pRanges) {
                int count = 0;
                pRanges->get_Length(&count);
                if (count > 0) {
                    IUIAutomationTextRange* pRange = nullptr;
                    pRanges->GetElement(0, &pRange);
                    if (pRange) {
                        SAFEARRAY* pRects = nullptr;
                        hr = pRange->GetBoundingRectangles(&pRects);
                        if (SUCCEEDED(hr) && pRects) {
                            double* pData = nullptr;
                            if (SUCCEEDED(SafeArrayAccessData(pRects, reinterpret_cast<void**>(&pData)))) {
                                long ubound = 0;
                                SafeArrayGetUBound(pRects, 1, &ubound);
                                if (ubound >= 3 && (pData[2] > 0 || pData[3] > 0)) {
                                    pt.x = static_cast<LONG>(pData[0]);
                                    pt.y = static_cast<LONG>(pData[1]);
                                    found = true;
                                }
                                SafeArrayUnaccessData(pRects);
                            }
                            SafeArrayDestroy(pRects);
                        }
                        pRange->Release();
                    }
                }
                pRanges->Release();
            }
            pTextPattern->Release();
        }
    }

    pFocused->Release();
    return found;
}

POINT InputMonitor::GetCurrentCaretPos() const {
    POINT pt = {0, 0};

    // 1. 若候选框可见，或曾缓存过有效坐标，直接返回 (0 毫秒开销)
    if (IsValidScreenCoord(cachedX_, cachedY_)) {
        pt.x = static_cast<LONG>(cachedX_);
        pt.y = static_cast<LONG>(cachedY_);
        return pt;
    }

    // 2. 检查 UI Automation (Chrome / Edge / Terminal / VS Code / 现代窗口)
    POINT uiaPt = {0, 0};
    if (GetCaretViaUIA(uiaPt)) {
        if (IsValidScreenCoord(static_cast<float>(uiaPt.x), static_cast<float>(uiaPt.y))) {
            return uiaPt;
        }
    }

    // 3. 经典 Win32 光标检测 (Notepad++, Scintilla)
    HWND fg = GetForegroundWindow();
    if (fg) {
        DWORD tid = GetWindowThreadProcessId(fg, nullptr);
        GUITHREADINFO gti = { sizeof(GUITHREADINFO) };
        if (GetGUIThreadInfo(tid, &gti)) {
            bool validCaretRect = (gti.rcCaret.right > gti.rcCaret.left && gti.rcCaret.bottom > gti.rcCaret.top);
            if (gti.hwndCaret && validCaretRect) {
                POINT caretPt = { gti.rcCaret.left, gti.rcCaret.top };
                if (ClientToScreen(gti.hwndCaret, &caretPt)) {
                    if (IsValidScreenCoord(static_cast<float>(caretPt.x), static_cast<float>(caretPt.y))) {
                        return caretPt;
                    }
                }
            }
        }
    }

    // 4. 兜底回退：取鼠标指针
    GetCursorPos(&pt);
    return pt;
}

void InputMonitor::UpdateActiveCaretPosition() {
    // 0. 优先采纳 IPC 共享内存中由 Weasel Hook 截获到的精准输入坐标
    if (pIpcData_ && IsValidScreenCoord(pIpcData_->commitX, pIpcData_->commitY)) {
        cachedX_ = pIpcData_->commitX;
        cachedY_ = pIpcData_->commitY;
        cachedW_ = pIpcData_->commitW > 0 ? pIpcData_->commitW : 80.0f;
        cachedH_ = pIpcData_->commitH > 0 ? pIpcData_->commitH : 30.0f;
        lastPanelSeenTime_ = std::chrono::steady_clock::now();
        return;
    }

    HWND fg = GetForegroundWindow();
    if (!fg) return;

    // 1. 检查活动的小狼毫候选框 WeaselPanel
    HWND hWeasel = FindActiveWeaselPanel(fg);
    if (hWeasel) {
        RECT rc;
        if (GetWindowRect(hWeasel, &rc) && (rc.right > rc.left && rc.bottom > rc.top)) {
            cachedX_ = static_cast<float>(rc.left + 15);
            cachedY_ = static_cast<float>(rc.top - 2);
            cachedW_ = static_cast<float>(rc.right - rc.left);
            cachedH_ = static_cast<float>(rc.bottom - rc.top);
            isPanelVisible_ = true;
            lastPanelSeenTime_ = std::chrono::steady_clock::now();
            return;
        }
    }

    // 2. 若候选框最近 1500ms 内刚刚关闭，保护 cachedX_/cachedY_ 不被覆盖
    auto now = std::chrono::steady_clock::now();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastPanelSeenTime_).count();
    if (elapsedMs < 1500 && IsValidScreenCoord(cachedX_, cachedY_)) {
        isPanelVisible_ = false;
        return;
    }

    // 3. UI Automation 检测 (支持 Chrome, Edge, Terminal, VS Code, 微信等)
    POINT uiaPt = {0, 0};
    if (GetCaretViaUIA(uiaPt)) {
        if (IsValidScreenCoord(static_cast<float>(uiaPt.x), static_cast<float>(uiaPt.y))) {
            cachedX_ = static_cast<float>(uiaPt.x);
            cachedY_ = static_cast<float>(uiaPt.y);
            return;
        }
    }

    // 4. 经典 Win32 光标检测 (仅在有真实 Win32 Caret 窗口时采纳)
    DWORD tid = GetWindowThreadProcessId(fg, nullptr);
    GUITHREADINFO gti = { sizeof(GUITHREADINFO) };
    if (GetGUIThreadInfo(tid, &gti)) {
        bool validCaretRect = (gti.rcCaret.right > gti.rcCaret.left && gti.rcCaret.bottom > gti.rcCaret.top);
        if (gti.hwndCaret && validCaretRect) {
            POINT pt = { gti.rcCaret.left, gti.rcCaret.top };
            if (ClientToScreen(gti.hwndCaret, &pt)) {
                if (IsValidScreenCoord(static_cast<float>(pt.x), static_cast<float>(pt.y))) {
                    cachedX_ = static_cast<float>(pt.x);
                    cachedY_ = static_cast<float>(pt.y);
                    return;
                }
            }
        }
    }

    // 5. 若尚未获得过有效坐标，则以鼠标指针位置作为初始缓存
    if (!IsValidScreenCoord(cachedX_, cachedY_)) {
        POINT pt;
        GetCursorPos(&pt);
        if (IsValidScreenCoord(static_cast<float>(pt.x), static_cast<float>(pt.y))) {
            cachedX_ = static_cast<float>(pt.x);
            cachedY_ = static_cast<float>(pt.y);
        }
    }
}

void InputMonitor::OnKeyDown(DWORD vkCode) {
    if (!pParticleMgr_ || !pConfig_ || !pConfig_->enabled) return;

    // 1. WPM 速率计算
    auto now = std::chrono::steady_clock::now();
    keystrokeTimes_.push_back(now);

    while (!keystrokeTimes_.empty()) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - keystrokeTimes_.front()).count();
        if (elapsed > 4000) {
            keystrokeTimes_.pop_front();
        } else {
            break;
        }
    }

    if (keystrokeTimes_.size() > 1) {
        auto durationSec = std::chrono::duration_cast<std::chrono::milliseconds>(now - keystrokeTimes_.front()).count() / 1000.0f;
        if (durationSec > 0.5f) {
            currentWpm_ = (keystrokeTimes_.size() / 5.0f) * (60.0f / durationSec);
        }
    } else {
        currentWpm_ = 0.0f;
    }

    inCombo_ = (currentWpm_ >= pConfig_->combo_wpm_threshold);

    // 2. 行为级特效映射
    if (vkCode == VK_BACK) {
        // 退格键：向下沉降的暗红/灰色余烬
        float dropX = 0.0f, dropY = 0.0f;
        if (isPanelVisible_ && IsValidScreenCoord(cachedX_, cachedY_)) {
            dropX = cachedX_;
            dropY = cachedY_;
        } else {
            POINT pt = GetCurrentCaretPos();
            dropX = static_cast<float>(pt.x);
            dropY = static_cast<float>(pt.y);
        }
        pParticleMgr_->EmitBackspaceDrop(dropX, dropY, *pConfig_);
    } else if (vkCode == VK_SPACE || vkCode == VK_RETURN) {
        // 空格/回车选词上屏
        if (!cachedCandidate_.empty() && IsValidScreenCoord(cachedX_, cachedY_)) {
            lastCommitTime_ = now;
            lastEmittedCommitText_ = cachedCandidate_;
            pParticleMgr_->EmitCommitText(
                cachedCandidate_, cachedX_, cachedY_, cachedW_, cachedH_, *pConfig_, inCombo_
            );
            cachedCandidate_.clear();
            cachedCandidateList_.clear();
        } else {
            POINT pt = GetCurrentCaretPos();
            pParticleMgr_->EmitCommitText(
                L"✦", static_cast<float>(pt.x), static_cast<float>(pt.y), 30.0f, 20.0f, *pConfig_, inCombo_
            );
        }
    } else if (vkCode >= '1' && vkCode <= '9') {
        // 数字键选词上屏
        size_t idx = vkCode - '1';
        if (idx < cachedCandidateList_.size() && !cachedCandidateList_[idx].empty() && IsValidScreenCoord(cachedX_, cachedY_)) {
            std::wstring chosen = cachedCandidateList_[idx];
            lastCommitTime_ = now;
            lastEmittedCommitText_ = chosen;
            pParticleMgr_->EmitCommitText(
                chosen, cachedX_, cachedY_, cachedW_, cachedH_, *pConfig_, inCombo_
            );
            cachedCandidate_.clear();
            cachedCandidateList_.clear();
        } else {
            POINT pt = GetCurrentCaretPos();
            pParticleMgr_->EmitTypingBubble(static_cast<float>(pt.x), static_cast<float>(pt.y), *pConfig_, inCombo_);
        }
    } else if (vkCode >= 'A' && vkCode <= 'Z') {
        // 普通字母输入：光标或候选框处向上发射冷光小气泡
        float bubbleX = 0.0f, bubbleY = 0.0f;
        if (isPanelVisible_ && IsValidScreenCoord(cachedX_, cachedY_)) {
            bubbleX = cachedX_;
            bubbleY = cachedY_ - 18.0f;
        } else {
            POINT pt = GetCurrentCaretPos();
            bubbleX = static_cast<float>(pt.x);
            bubbleY = static_cast<float>(pt.y);
        }
        pParticleMgr_->EmitTypingBubble(bubbleX, bubbleY, *pConfig_, inCombo_);
    }
}

void InputMonitor::PollIpcEvents() {
    // 延迟连接 shared memory
    if (!pIpcData_) {
        if (!hSharedMem_) {
            hSharedMem_ = OpenFileMappingW(FILE_MAP_READ, FALSE, SHARED_MEM_NAME);
        }
        if (hSharedMem_) {
            pIpcData_ = static_cast<IpcData*>(MapViewOfFile(hSharedMem_, FILE_MAP_READ, 0, 0, sizeof(IpcData)));
            if (pIpcData_) {
                LogApp("PollIpcEvents: Successfully mapped shared memory!");
            }
        }
        if (!pIpcData_) return;
    }
    if (!pParticleMgr_ || !pConfig_) return;

    if (pIpcData_->sequence != lastIpcSeq_) {
        lastIpcSeq_ = pIpcData_->sequence;

        if (pIpcData_->eventType == IpcEventType::CandidatesShown) {
            if (IsValidScreenCoord(pIpcData_->commitX, pIpcData_->commitY)) {
                cachedX_ = pIpcData_->commitX;
                cachedY_ = pIpcData_->commitY;
                cachedW_ = pIpcData_->commitW > 0 ? pIpcData_->commitW : 80.0f;
                cachedH_ = pIpcData_->commitH > 0 ? pIpcData_->commitH : 30.0f;
                isPanelVisible_ = true;
                lastPanelSeenTime_ = std::chrono::steady_clock::now();
            }
            if (pIpcData_->committedText[0] != L'\0') {
                cachedCandidate_ = pIpcData_->committedText;
            }

            cachedCandidateList_.clear();
            for (uint32_t i = 0; i < pIpcData_->candidateCount; ++i) {
                cachedCandidateList_.push_back(pIpcData_->candidates[i].text);
            }
        } else if (pIpcData_->eventType == IpcEventType::WindowHidden) {
            isPanelVisible_ = false;
        } else if (pIpcData_->eventType == IpcEventType::CandidateCommitted) {
            std::wstring committed(pIpcData_->committedText);
            if (!committed.empty()) {
                auto now = std::chrono::steady_clock::now();
                auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastCommitTime_).count();
                // 防抖：若 250ms 内已经由键盘事件触发过同一词汇，避免重复产生双份粒子
                if (committed != lastEmittedCommitText_ || elapsedMs > 250) {
                    lastCommitTime_ = now;
                    lastEmittedCommitText_ = committed;

                    float emitX = IsValidScreenCoord(pIpcData_->commitX, pIpcData_->commitY) ? pIpcData_->commitX : cachedX_;
                    float emitY = IsValidScreenCoord(pIpcData_->commitX, pIpcData_->commitY) ? pIpcData_->commitY : cachedY_;

                    if (!IsValidScreenCoord(emitX, emitY)) {
                        POINT pt = GetCurrentCaretPos();
                        emitX = static_cast<float>(pt.x);
                        emitY = static_cast<float>(pt.y);
                    }

                    int u8len = WideCharToMultiByte(CP_UTF8, 0, committed.c_str(), -1, nullptr, 0, nullptr, nullptr);
                    std::string u8(u8len > 0 ? u8len - 1 : 0, 0);
                    if (u8len > 1) WideCharToMultiByte(CP_UTF8, 0, committed.c_str(), -1, &u8[0], u8len, nullptr, nullptr);

                    LogApp("PollIpcEvents: Emitting commit '" + u8 + "' at (" + std::to_string(emitX) + ", " + std::to_string(emitY) + ")");

                    pParticleMgr_->EmitCommitText(
                        committed,
                        emitX,
                        emitY,
                        cachedW_ > 0 ? cachedW_ : 80.0f,
                        cachedH_ > 0 ? cachedH_ : 30.0f,
                        *pConfig_,
                        inCombo_
                    );
                }
            }
            cachedCandidate_.clear();
            cachedCandidateList_.clear();
            isPanelVisible_ = false;
        }
    }
}

} // namespace DAFlash

