#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <chrono>

namespace DAFlash {

// Color helpers
inline D2D1_COLOR_F MakeColor(float r, float g, float b, float a = 1.0f) {
    return D2D1::ColorF(r, g, b, a);
}

inline D2D1_COLOR_F MakeColorHex(uint32_t rgb, float a = 1.0f) {
    float r = ((rgb >> 16) & 0xFF) / 255.0f;
    float g = ((rgb >> 8) & 0xFF) / 255.0f;
    float b = (rgb & 0xFF) / 255.0f;
    return D2D1::ColorF(r, g, b, a);
}

// Lerp color
inline D2D1_COLOR_F LerpColor(const D2D1_COLOR_F& c1, const D2D1_COLOR_F& c2, float t) {
    t = (std::max)(0.0f, (std::min)(1.0f, t));
    return D2D1::ColorF(
        c1.r + (c2.r - c1.r) * t,
        c1.g + (c2.g - c1.g) * t,
        c1.b + (c2.b - c1.b) * t,
        c1.a + (c2.a - c1.a) * t
    );
}

// Random float in range [min, max]
inline float RandomFloat(float min, float max) {
    float r = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
    return min + r * (max - min);
}

inline void LogApp(const std::string& msg) {
    WCHAR path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring p(path);
    size_t idx = p.find_last_of(L"\\/");
    if (idx != std::wstring::npos) p = p.substr(0, idx + 1);
    p += L"debug.log";

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
}

} // namespace DAFlash
