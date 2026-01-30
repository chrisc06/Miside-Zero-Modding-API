#pragma once
#include <windows.h>
#include <string>
#include <vector>

namespace Injector {
    bool EnableDebugPrivilege();
    bool InjectDLL(DWORD processID, const char* dllPath);
    std::string ExtractDllFromResource(int resourceID);
}

DWORD GetProcId(const char* procName);