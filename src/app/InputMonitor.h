#pragma once

#include "Common.h"
#include "IpcProtocol.h"
#include "Config.h"
#include "ParticleManager.h"
#include <deque>

struct IUIAutomation;

namespace DAFlash {

class InputMonitor {
public:
    static InputMonitor& Instance();

    bool Initialize(ParticleManager* pParticleMgr, const EngineConfig* pConfig);
    void Shutdown();
    void PollIpcEvents();
    void UpdateActiveCaretPosition();

    bool IsInCombo() const { return inCombo_; }
    float GetCurrentWpm() const { return currentWpm_; }
    POINT GetCurrentCaretPos() const;

private:
    InputMonitor();
    ~InputMonitor();

    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
    void OnKeyDown(DWORD vkCode);

    ParticleManager* pParticleMgr_ = nullptr;
    const EngineConfig* pConfig_ = nullptr;

    HHOOK hKeyboardHook_ = nullptr;

    // IPC handles
    HANDLE hSharedMem_ = nullptr;
    IpcData* pIpcData_ = nullptr;
    HANDLE hMutex_ = nullptr;
    HANDLE hEvent_ = nullptr;
    uint32_t lastIpcSeq_ = 0;
    uint32_t lastReadCommitSeq_ = 0;
    uint32_t lastCaretSeq_ = 0;
    uint32_t lastPanelSeq_ = 0;
    bool isComposing_ = false;

    // WPM & Combo
    std::deque<std::chrono::steady_clock::time_point> keystrokeTimes_;
    float currentWpm_ = 0.0f;
    bool inCombo_ = false;

    // Cache of last known commit text for Space / digit press
    std::wstring cachedCandidate_ = L"";
    std::vector<std::wstring> cachedCandidateList_;
    float cachedX_ = -999999.0f;
    float cachedY_ = -999999.0f;
    float cachedW_ = 80.0f;
    float cachedH_ = 30.0f;
    bool isPanelVisible_ = false;

    // Dedup guard
    std::wstring lastEmittedCommitText_;
    std::chrono::steady_clock::time_point lastCommitTime_;

    // Multi-tier Caret detection helpers
    ::IUIAutomation* pUIAutomation_ = nullptr;
    HWINEVENTHOOK hWinEventHook_ = nullptr;
    static void CALLBACK WinEventProc(
        HWINEVENTHOOK hWinEventHook,
        DWORD event,
        HWND hwnd,
        LONG idObject,
        LONG idChild,
        DWORD idEventThread,
        DWORD dwmsEventTime
    );

    bool GetCaretViaUIA(POINT& pt) const;
    HWND FindActiveWeaselPanel(HWND fg) const;
    mutable std::chrono::steady_clock::time_point lastPanelSeenTime_;
};

} // namespace DAFlash

