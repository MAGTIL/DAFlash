#include "OverlayWindow.h"
#include <dwmapi.h>

namespace DAFlash {

static const wchar_t* OVERLAY_CLASS_NAME = L"DAFlashOverlayWindowClass";

LRESULT CALLBACK OverlayWindow::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_ERASEBKGND:
        return 1; // 阻止默认背景擦除，避免闪烁
    case WM_DISPLAYCHANGE: {
        auto* pThis = reinterpret_cast<OverlayWindow*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
        if (pThis) {
            pThis->UpdateBounds();
        }
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
}

OverlayWindow::OverlayWindow() = default;

OverlayWindow::~OverlayWindow() {
    Destroy();
}

void OverlayWindow::UpdateBounds() {
    // 获取完整虚拟多显示器物理像素桌面尺寸与原点
    left_ = GetSystemMetrics(SM_XVIRTUALSCREEN);
    top_ = GetSystemMetrics(SM_YVIRTUALSCREEN);
    width_ = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    height_ = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    if (width_ <= 0 || height_ <= 0) {
        width_ = GetSystemMetrics(SM_CXSCREEN);
        height_ = GetSystemMetrics(SM_CYSCREEN);
        left_ = 0;
        top_ = 0;
    }

    if (hWnd_) {
        SetWindowPos(hWnd_, HWND_TOPMOST, left_, top_, width_, height_,
                     SWP_NOACTIVATE);
    }
}

bool OverlayWindow::Create() {
    HINSTANCE hInstance = GetModuleHandleW(nullptr);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = OVERLAY_CLASS_NAME;
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    wc.hbrBackground = nullptr;
    RegisterClassExW(&wc);

    UpdateBounds();

    // 关键样式：
    // WS_EX_LAYERED: 支持透明通道
    // WS_EX_TRANSPARENT: 鼠标绝对穿透，不截获任何点击
    // WS_EX_NOACTIVATE: 绝对不抢占打字输入焦点
    // WS_EX_TOPMOST: 悬浮于所有普通窗口最上层
    // WS_EX_TOOLWINDOW: 不在任务栏显示图标
    DWORD exStyle = WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOPMOST | WS_EX_TOOLWINDOW;

    hWnd_ = CreateWindowExW(
        exStyle,
        OVERLAY_CLASS_NAME,
        L"DA Flash Visual Canvas",
        WS_POPUP,
        left_, top_, width_, height_,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!hWnd_) return false;

    SetWindowLongPtrW(hWnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    // 设置 RGB(0,0,0) 为透明色键，确保 Direct2D 黑色清屏区域 100% 透明且鼠标完全穿透
    SetLayeredWindowAttributes(hWnd_, RGB(0, 0, 0), 0, LWA_COLORKEY);

    // 开启 DWM 玻璃透明扩展
    MARGINS margins = {-1, -1, -1, -1};
    DwmExtendFrameIntoClientArea(hWnd_, &margins);

    return true;
}

void OverlayWindow::Show(bool show) {
    if (hWnd_) {
        ShowWindow(hWnd_, show ? SW_SHOWNOACTIVATE : SW_HIDE);
        if (show) {
            SetWindowPos(hWnd_, HWND_TOPMOST, left_, top_, width_, height_,
                         SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
        }
    }
}

void OverlayWindow::Destroy() {
    if (hWnd_) {
        DestroyWindow(hWnd_);
        hWnd_ = nullptr;
    }
}

} // namespace DAFlash

