#pragma once

#include "Common.h"

namespace DAFlash {

class OverlayWindow {
public:
    OverlayWindow();
    ~OverlayWindow();

    bool Create();
    void Destroy();
    void Show(bool show = true);
    void UpdateBounds();

    HWND GetHWnd() const { return hWnd_; }
    int GetWidth() const { return width_; }
    int GetHeight() const { return height_; }
    int GetLeft() const { return left_; }
    int GetTop() const { return top_; }

private:
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    HWND hWnd_ = nullptr;
    int left_ = 0;
    int top_ = 0;
    int width_ = 0;
    int height_ = 0;
};

} // namespace DAFlash

