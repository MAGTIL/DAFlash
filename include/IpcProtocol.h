#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <cstdint>

namespace DAFlash {

constexpr const wchar_t* SHARED_MEM_NAME = L"Local\\DAFlash_Weasel_IPC";
constexpr const wchar_t* MUTEX_NAME = L"Local\\DAFlash_Weasel_Mutex";
constexpr const wchar_t* EVENT_NAME = L"Local\\DAFlash_Weasel_Event";

enum class IpcEventType : uint32_t {
    None = 0,
    PositionUpdated,      // 光标/输入坐标更新 (高精度 TSF 管道)
    CandidatesShown,      // 候选词列表展示/更新
    CandidateCommitted,   // 某个候选词被选中上屏
    PreeditInput,         // 正在键入拼音 (Preedit)
    WindowHidden          // 候选框隐藏
};

struct CandidateItem {
    wchar_t text[64];
    float x;              // 屏幕绝对坐标 X
    float y;              // 屏幕绝对坐标 Y
    float width;
    float height;
    bool isHighlighted;
};

struct IpcData {
    uint32_t version;
    uint32_t sequence;
    IpcEventType eventType;
    uint32_t candidateCount;
    int32_t highlightedIndex;
    
    // 上屏候选词信息
    wchar_t committedText[64];
    float commitX;
    float commitY;
    float commitW;
    float commitH;

    // 候选列表条目
    CandidateItem candidates[16];

    // 拼音预编辑区
    wchar_t preeditText[64];
    float preeditX;
    float preeditY;
    float preeditW;
    float preeditH;
};

} // namespace DAFlash
