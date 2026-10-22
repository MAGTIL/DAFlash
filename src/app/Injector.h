#pragma once

#include "Common.h"
#include <string>

namespace DAFlash {

class Injector {
public:
    static DWORD FindWeaselServerProcess();
    static bool IsDllLoaded(DWORD pid, const std::wstring& dllName);
    static bool InjectDll(DWORD pid, const std::wstring& dllPath);
};

} // namespace DAFlash

