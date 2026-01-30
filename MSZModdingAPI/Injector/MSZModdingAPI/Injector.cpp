#include <iostream>
#include <windows.h>
#include <tlhelp32.h>
#include <string>
#include "Injector.h"
#include <fstream>

namespace Injector {

    std::string ExtractDllFromResource(int resourceID) {
        HRSRC hRes = FindResource(NULL, MAKEINTRESOURCE(resourceID), RT_RCDATA);
        if (!hRes) return "";

        HGLOBAL hData = LoadResource(NULL, hRes);
        if (!hData) return "";

        void* pData = LockResource(hData);
        DWORD dSize = SizeofResource(NULL, hRes);

        char tempPath[MAX_PATH];
        GetTempPathA(MAX_PATH, tempPath);
        std::string fullPath = std::string(tempPath) + "MSZModdingAPI.dll";

        std::ofstream out(fullPath, std::ios::binary);
        if (!out.is_open()) return "";

        out.write((char*)pData, dSize);
        out.close();

        return fullPath;
    }

    bool EnableDebugPrivilege() {
        HANDLE hToken;
        LUID luid;
        TOKEN_PRIVILEGES tkp;

        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
            return false;
        }

        if (!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid)) {
            CloseHandle(hToken);
            return false;
        }

        tkp.PrivilegeCount = 1;
        tkp.Privileges[0].Luid = luid;
        tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

        bool result = AdjustTokenPrivileges(hToken, FALSE, &tkp, sizeof(tkp), NULL, NULL);
        CloseHandle(hToken);
        return result;
    }

    bool InjectDLL(DWORD processID, const char* dllPath) {
        //  - Visualizing how CreateRemoteThread works
        HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processID);
        if (!hProcess) {
            std::cerr << "[!] Failed to open process. Error: " << GetLastError() << std::endl;
            std::cerr << "    (Hint: Run this injector as Administrator!)" << std::endl;
            return false;
        }

        void* pDllPath = VirtualAllocEx(hProcess, 0, strlen(dllPath) + 1, MEM_COMMIT, PAGE_READWRITE);
        if (!pDllPath) {
            std::cerr << "[!] Failed to allocate memory." << std::endl;
            CloseHandle(hProcess);
            return false;
        }

        if (!WriteProcessMemory(hProcess, pDllPath, (void*)dllPath, strlen(dllPath) + 1, 0)) {
            std::cerr << "[!] Failed to write memory." << std::endl;
            VirtualFreeEx(hProcess, pDllPath, 0, MEM_RELEASE);
            CloseHandle(hProcess);
            return false;
        }

        HANDLE hThread = CreateRemoteThread(hProcess, 0, 0,
            (LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleA("Kernel32.dll"), "LoadLibraryA"),
            pDllPath, 0, 0);

        if (!hThread) {
            std::cerr << "[!] Failed to create remote thread." << std::endl;
            VirtualFreeEx(hProcess, pDllPath, 0, MEM_RELEASE);
            CloseHandle(hProcess);
            return false;
        }

        // Wait for LoadLibrary to finish
        WaitForSingleObject(hThread, INFINITE);

        // Clean up memory in the target process (remove the path string, not the DLL)
        VirtualFreeEx(hProcess, pDllPath, 0, MEM_RELEASE);
        CloseHandle(hThread);
        CloseHandle(hProcess);

        return true;
    }
}
DWORD GetProcId(const char* procName) {
    DWORD procId = 0;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (hSnap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 procEntry;
        procEntry.dwSize = sizeof(procEntry);

        if (Process32First(hSnap, &procEntry)) {
            do {
                // Convert the process name in the snapshot to a standard string for comparison
                // This handles the WCHAR vs CHAR issue automatically
#ifdef UNICODE
                std::wstring wideProcName(procEntry.szExeFile);
                std::string currentEntry(wideProcName.begin(), wideProcName.end());
#else
                std::string currentEntry = procEntry.szExeFile;
#endif

                if (_stricmp(currentEntry.c_str(), procName) == 0) {
                    procId = procEntry.th32ProcessID;
                    break;
                }
            } while (Process32Next(hSnap, &procEntry));
        }
    }
    CloseHandle(hSnap);
    return procId;
}