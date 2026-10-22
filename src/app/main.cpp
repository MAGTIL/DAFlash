#include "Common.h"
#include "Config.h"
#include "OverlayWindow.h"
#include "D2DRenderer.h"
#include "ParticleManager.h"
#include "InputMonitor.h"
#include "Injector.h"

#include <shellapi.h>
#include <timeapi.h>

static void LogDebug(const std::string& str) {
    DAFlash::LogApp(str);
}

#define WM_TRAYICON (WM_USER + 101)
#define IDM_THEME_CYBERPUNK  2001
#define IDM_THEME_GOLDEN     2002
#define IDM_THEME_CLASSIC    2003
#define IDM_THEME_RAINBOW    2004
#define IDM_TOGGLE_SHAKE     2005
#define IDM_EXIT             2006

using namespace DAFlash;

static NOTIFYICONDATAW g_nid = {};
static EngineConfig g_config;
static bool g_running = true;

// 托盘消息处理
LRESULT CALLBACK TrayWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_TRAYICON) {
        if (LOWORD(lParam) == WM_RBUTTONUP || LOWORD(lParam) == WM_LBUTTONUP) {
            POINT pt;
            GetCursorPos(&pt);
            HMENU hMenu = CreatePopupMenu();
            AppendMenuW(hMenu, MF_STRING, IDM_THEME_CYBERPUNK, L"主题: 赛博朋克 (Cyberpunk)");
            AppendMenuW(hMenu, MF_STRING, IDM_THEME_GOLDEN, L"主题: 璀璨流金 (Golden)");
            AppendMenuW(hMenu, MF_STRING, IDM_THEME_CLASSIC, L"主题: 极地冷光 (Classic)");
            AppendMenuW(hMenu, MF_STRING, IDM_THEME_RAINBOW, L"主题: 彩虹炫彩 (Rainbow)");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(hMenu, MF_STRING | (g_config.combo_shake_enabled ? MF_CHECKED : MF_UNCHECKED),
                        IDM_TOGGLE_SHAKE, L"连击暴击屏幕微震 (Shake)");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(hMenu, MF_STRING, IDM_EXIT, L"退出 DA Flash");

            SetForegroundWindow(hWnd);
            int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hWnd, nullptr);
            DestroyMenu(hMenu);

            switch (cmd) {
            case IDM_THEME_CYBERPUNK:
                g_config.theme = ParticleTheme::Cyberpunk;
                break;
            case IDM_THEME_GOLDEN:
                g_config.theme = ParticleTheme::Golden;
                break;
            case IDM_THEME_CLASSIC:
                g_config.theme = ParticleTheme::Classic;
                break;
            case IDM_THEME_RAINBOW:
                g_config.theme = ParticleTheme::Rainbow;
                break;
            case IDM_TOGGLE_SHAKE:
                g_config.combo_shake_enabled = !g_config.combo_shake_enabled;
                break;
            case IDM_EXIT:
                g_running = false;
                PostQuitMessage(0);
                break;
            default:
                break;
            }
        }
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static LONG WINAPI GlobalCrashHandler(EXCEPTION_POINTERS* ep) {
    std::string hexCode = "0x" + std::to_string(ep->ExceptionRecord->ExceptionCode);
    LogApp("FATAL CRASH! ExceptionCode: " + hexCode +
           " address: 0x" + std::to_string((uintptr_t)ep->ExceptionRecord->ExceptionAddress));
    return EXCEPTION_CONTINUE_SEARCH;
}

static HICON CreateDAFlashTrayIcon() {
    int iconSize = 32;
    BITMAPV5HEADER bi = {};
    bi.bV5Size = sizeof(BITMAPV5HEADER);
    bi.bV5Width = iconSize;
    bi.bV5Height = -iconSize; // top-down
    bi.bV5Planes = 1;
    bi.bV5BitCount = 32;
    bi.bV5Compression = BI_BITFIELDS;
    bi.bV5RedMask   = 0x00FF0000;
    bi.bV5GreenMask = 0x0000FF00;
    bi.bV5BlueMask  = 0x000000FF;
    bi.bV5AlphaMask = 0xFF000000;

    HDC hdcScreen = GetDC(nullptr);
    uint32_t* pPixels = nullptr;
    HBITMAP hColorBmp = CreateDIBSection(
        hdcScreen, reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS,
        reinterpret_cast<void**>(&pPixels), nullptr, 0
    );
    ReleaseDC(nullptr, hdcScreen);

    if (!hColorBmp || !pPixels) return LoadIconW(nullptr, MAKEINTRESOURCEW(32512));

    float cx = 15.5f, cy = 15.5f, rOuter = 13.5f, rInner = 7.5f;
    for (int y = 0; y < iconSize; ++y) {
        for (int x = 0; x < iconSize; ++x) {
            float dx = x - cx;
            float dy = y - cy;
            float dist = sqrtf(dx * dx + dy * dy);
            int idx = y * iconSize + x;

            if (dist <= rOuter && dist >= rInner) {
                // Neon Cyan outer ring (RGB: 0, 229, 255)
                pPixels[idx] = 0xFF00E5FF;
            } else if (dist < rInner) {
                // Vibrant Purple core (RGB: 170, 0, 255)
                pPixels[idx] = 0xFFAA00FF;
            } else if (dist <= rOuter + 1.5f) {
                // Soft glow edge
                float alpha = (1.0f - (dist - rOuter) / 1.5f);
                uint8_t a = static_cast<uint8_t>(alpha * 200.0f);
                pPixels[idx] = (a << 24) | (0 << 16) | (static_cast<uint8_t>(229 * alpha) << 8) | static_cast<uint8_t>(255 * alpha);
            } else {
                pPixels[idx] = 0x00000000; // Transparent
            }
        }
    }

    HBITMAP hMaskBmp = CreateBitmap(iconSize, iconSize, 1, 1, nullptr);

    ICONINFO ii = {};
    ii.fIcon = TRUE;
    ii.hbmColor = hColorBmp;
    ii.hbmMask = hMaskBmp;

    HICON hIcon = CreateIconIndirect(&ii);

    DeleteObject(hColorBmp);
    DeleteObject(hMaskBmp);
    return hIcon ? hIcon : LoadIconW(nullptr, MAKEINTRESOURCEW(32512));
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    SetUnhandledExceptionFilter(GlobalCrashHandler);
    LogApp("DAFlash WinMain starting...");
    HWINSTA hUserSta = OpenWindowStationW(L"WinSta0", FALSE, MAXIMUM_ALLOWED);
    if (hUserSta) {
        SetProcessWindowStation(hUserSta);
        HDESK hUserDesk = OpenDesktopW(L"Default", 0, FALSE, MAXIMUM_ALLOWED);
        if (hUserDesk) {
            SetThreadDesktop(hUserDesk);
        }
    }

    LogDebug("DAFlash WinMain started");
    timeBeginPeriod(1);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    // 1. 启用 Per-Monitor DPI Aware v2，防止多屏幕不同缩放导致坐标错位与模糊虚拟化
    typedef BOOL (WINAPI* PFN_SetProcessDpiAwarenessContext)(DPI_AWARENESS_CONTEXT);
    auto pfnSetDpi = reinterpret_cast<PFN_SetProcessDpiAwarenessContext>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetProcessDpiAwarenessContext")
    );
    if (pfnSetDpi) {
        pfnSetDpi(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        LogDebug("DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 set");
    } else {
        SetProcessDPIAware();
        LogDebug("Fallback to SetProcessDPIAware");
    }

    // 2. 创建系统托盘宿主窗口
    WNDCLASSEXW twc = {};
    twc.cbSize = sizeof(WNDCLASSEXW);
    twc.lpfnWndProc = TrayWndProc;
    twc.hInstance = hInstance;
    twc.lpszClassName = L"DAFlashTrayHost";
    RegisterClassExW(&twc);

    HWND hTrayWnd = CreateWindowExW(
        WS_EX_TOOLWINDOW, L"DAFlashTrayHost", L"DAFlashTray",
        WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, hInstance, nullptr
    );

    g_nid.cbSize = sizeof(NOTIFYICONDATAW);
    g_nid.hWnd = hTrayWnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = CreateDAFlashTrayIcon();
    wcscpy_s(g_nid.szTip, L"DA Flash (多巴胺输入特效) 运行中");
    BOOL trayRes = Shell_NotifyIconW(NIM_ADD, &g_nid);
    LogDebug("Tray icon added, result=" + std::to_string(trayRes));

    // 3. 创建全屏透明画布视窗与渲染器
    OverlayWindow overlay;
    if (!overlay.Create()) {
        LogDebug("OverlayWindow create failed");
        return -1;
    }
    LogDebug("OverlayWindow created: left=" + std::to_string(overlay.GetLeft())
             + " top=" + std::to_string(overlay.GetTop())
             + " w=" + std::to_string(overlay.GetWidth())
             + " h=" + std::to_string(overlay.GetHeight()));

    D2DRenderer renderer;
    if (!renderer.Initialize(overlay.GetHWnd(), overlay.GetWidth(), overlay.GetHeight())) {
        LogDebug("D2DRenderer initialize failed");
        return -1;
    }
    LogDebug("D2DRenderer initialized");

    overlay.Show(true);

    // 4. 粒子管理器与输入监听
    ParticleManager particleMgr;
    particleMgr.Initialize(g_config.max_particles);

    InputMonitor::Instance().Initialize(&particleMgr, &g_config);
    LogDebug("InputMonitor initialized");

    // 5. 自动检测并注入 WeaselServer.exe
    WCHAR exeDir[MAX_PATH] = {0};
    GetModuleFileNameW(nullptr, exeDir, MAX_PATH);
    std::wstring dir(exeDir);
    size_t pos = dir.find_last_of(L"\\/");
    if (pos != std::string::npos) {
        dir = dir.substr(0, pos + 1);
    }
    std::wstring hookDllPath = dir + L"DAFlashHook.dll";

    DWORD weaselPid = Injector::FindWeaselServerProcess();
    if (weaselPid > 0) {
        if (!Injector::IsDllLoaded(weaselPid, L"DAFlashHook.dll")) {
            LogDebug("Injecting DAFlashHook.dll into WeaselServer");
            Injector::InjectDll(weaselPid, hookDllPath);
        } else {
            LogDebug("DAFlashHook.dll already loaded in WeaselServer");
        }
    }

    int primW = GetSystemMetrics(SM_CXSCREEN);
    int primH = GetSystemMetrics(SM_CYSCREEN);
    particleMgr.EmitCommitText(L"✦ DA Flash (Dopamine) ✦", primW * 0.5f - 140.0f, primH * 0.5f - 20.0f, 280.0f, 40.0f, g_config, true);
    LogDebug("Welcome particles emitted, entering render loop");

    // 6. 60+ FPS 主渲染管线循环
    auto lastTime = std::chrono::steady_clock::now();
    auto lastInjectCheck = std::chrono::steady_clock::now();
    bool lastHadParticles = true;
    float shakeTime = 0.0f;

    MSG msg;
    while (g_running) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                LogDebug("WM_QUIT received in PeekMessage");
                g_running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!g_running) break;

        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration_cast<std::chrono::microseconds>(now - lastTime).count() / 1000000.0f;
        lastTime = now;

        // 自动维持注入：若 WeaselServer 重启或重新上线，自动补注
        if (std::chrono::duration_cast<std::chrono::seconds>(now - lastInjectCheck).count() >= 2) {
            lastInjectCheck = now;
            DWORD pid = Injector::FindWeaselServerProcess();
            if (pid > 0 && !Injector::IsDllLoaded(pid, L"DAFlashHook.dll")) {
                LogDebug("Auto-injecting DAFlashHook.dll into WeaselServer (PID " + std::to_string(pid) + ")");
                Injector::InjectDll(pid, hookDllPath);
            }
        }

        static int frameCount = 0;
        frameCount++;
        if (frameCount <= 3) {
            LogApp("Frame " + std::to_string(frameCount) + " start");
        }

        // 周期性在渲染主循环中更新光标与候选框位置
        InputMonitor::Instance().UpdateActiveCaretPosition();
        if (frameCount <= 3) LogApp("Frame " + std::to_string(frameCount) + " UpdateActiveCaretPosition done");

        // 轮询 IPC 选词事件
        InputMonitor::Instance().PollIpcEvents();
        if (frameCount <= 3) LogApp("Frame " + std::to_string(frameCount) + " PollIpcEvents done");

        // 物理逻辑更新
        particleMgr.Update(dt, g_config);
        if (frameCount <= 3) LogApp("Frame " + std::to_string(frameCount) + " particleMgr.Update done");

        bool hasActiveParticles = (particleMgr.GetActiveCount() > 0);

        // 节能机制：当没有活跃粒子且上帧已清屏时，低频等待，降低 CPU 消耗至 0%
        if (!hasActiveParticles && !lastHadParticles) {
            Sleep(8);
            continue;
        }

        // 连击暴击屏幕微震计算
        float shakeX = 0.0f;
        float shakeY = 0.0f;
        if (InputMonitor::Instance().IsInCombo() && g_config.combo_shake_enabled && hasActiveParticles) {
            shakeTime += dt;
            shakeX = sinf(shakeTime * 45.0f) * 2.5f;
            shakeY = cosf(shakeTime * 38.0f) * 2.0f;
        } else {
            shakeTime = 0.0f;
        }

        // 硬件加速渲染
        renderer.BeginDraw();
        if (frameCount <= 3) LogApp("Frame " + std::to_string(frameCount) + " BeginDraw done");
        renderer.Clear();
        if (frameCount <= 3) LogApp("Frame " + std::to_string(frameCount) + " Clear done");
        if (hasActiveParticles) {
            float drawOffsetX = shakeX - static_cast<float>(overlay.GetLeft());
            float drawOffsetY = shakeY - static_cast<float>(overlay.GetTop());
            renderer.DrawParticles(particleMgr.GetParticles(), drawOffsetX, drawOffsetY);
        }
        if (frameCount <= 3) LogApp("Frame " + std::to_string(frameCount) + " DrawParticles done");
        bool drawOk = renderer.EndDraw();
        if (frameCount <= 3) LogApp("Frame " + std::to_string(frameCount) + " EndDraw done (ok=" + std::to_string(drawOk) + ")");
        if (!drawOk) {
            static int drawFailCount = 0;
            if (drawFailCount++ < 5) {
                LogDebug("renderer.EndDraw failed, count=" + std::to_string(drawFailCount));
            }
        }

        lastHadParticles = hasActiveParticles;

        // 维持约 60~120 FPS 渲染节奏
        Sleep(8);
    }

    LogApp("Render loop exited: g_running=" + std::to_string(g_running) + " msg=" + std::to_string(msg.message));

    // 退出清理
    InputMonitor::Instance().Shutdown();
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
    DestroyWindow(hTrayWnd);
    timeEndPeriod(1);
    CoUninitialize();

    return 0;
}
