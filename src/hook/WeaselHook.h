#pragma once

#include "Common.h"
#include "IpcProtocol.h"

namespace DAFlash {

void LogHook(const std::string& msg);

class WeaselHookManager {
public:
    static WeaselHookManager& Instance();

    bool Initialize();
    void Shutdown();

    // Hook callbacks
    void OnWeaselPanelCreated(HWND hWnd);
    void OnWeaselPanelDestroyed(HWND hWnd);
    void OnWeaselPanelPosChanged(HWND hWnd, const RECT& screenRect);
    void OnWeaselPanelVisibility(HWND hWnd, bool visible);

    void BeginPaintCycle();
    void EndPaintCycle(HWND hWnd);

    void OnCreateTextLayout(const WCHAR* text, UINT32 length, FLOAT maxWidth, FLOAT maxHeight);
    void OnDrawTextLayout(FLOAT originX, FLOAT originY, IDWriteTextLayout* layout);
    void OnRimeCommit(const char* utf8Text);
    void OnInputPositionUpdated(int left, int top, int height);

private:
    WeaselHookManager();
    ~WeaselHookManager();

    void NotifyIpc(IpcEventType type);

    HANDLE hSharedMem_ = nullptr;
    IpcData* pIpcData_ = nullptr;
    HANDLE hMutex_ = nullptr;
    HANDLE hEvent_ = nullptr;

    HWND hWeaselPanel_ = nullptr;
    WNDPROC originalWndProc_ = nullptr;
    RECT currentPanelRect_ = {};
    bool isPanelVisible_ = false;

    struct CandidateEntry {
        std::wstring text;
        float w;
        float h;
    };

    // Candidates captured during paint
    std::vector<CandidateEntry> tempCandidates_;
    std::vector<CandidateEntry> currentCandidates_;

    // Cache of candidates recorded during recent paint
    std::wstring lastCommittedCandidate_;
    float lastCommittedX_ = 0.0f;
    float lastCommittedY_ = 0.0f;
    float lastCommittedW_ = 0.0f;
    float lastCommittedH_ = 0.0f;

    static LRESULT CALLBACK WeaselPanelSubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    friend LRESULT CALLBACK WeaselPanelSubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
};

} // namespace DAFlash

