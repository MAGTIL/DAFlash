#include "WeaselHook.h"

static HANDLE g_hInitThread = nullptr;

DWORD WINAPI HookThreadProc(LPVOID lpParam) {
    DAFlash::LogHook("HookThreadProc running");
    Sleep(50);
    bool ok = DAFlash::WeaselHookManager::Instance().Initialize();
    DAFlash::LogHook(ok ? "WeaselHookManager initialized successfully" : "WeaselHookManager initialization failed");
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        DAFlash::LogHook("DllMain: DLL_PROCESS_ATTACH");
        DisableThreadLibraryCalls(hinstDLL);
        g_hInitThread = CreateThread(nullptr, 0, HookThreadProc, nullptr, 0, nullptr);
        if (!g_hInitThread) {
            DAFlash::LogHook("CreateThread failed in DllMain");
        }
        break;
    case DLL_PROCESS_DETACH:
        if (lpvReserved == nullptr) {
            DAFlash::LogHook("DllMain: DLL_PROCESS_DETACH");
            DAFlash::WeaselHookManager::Instance().Shutdown();
            if (g_hInitThread) {
                CloseHandle(g_hInitThread);
                g_hInitThread = nullptr;
            }
        }
        break;
    }
    return TRUE;
}
