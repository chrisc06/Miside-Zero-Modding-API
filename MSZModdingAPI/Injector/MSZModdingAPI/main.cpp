#include <iostream>
#include <windows.h>
#include <filesystem>
#include <vector>
#include <thread>
#include "Injector.h"
#include "Security.h"

namespace fs = std::filesystem;

const char* REQUIRED_EXPORT = "MSZ_Mod_Entry";
const char* MODS_FOLDER = "Mods";

int main() {
    const char* targetExe = "MiSide Zero.exe";
    const char* apiDllName = "MSZModdingAPIDLL.dll";

    SetConsoleTitleA("MSZ Modding API - Secure Launcher");

    // 1. Setup Folders
    fs::path modPath = fs::current_path() / MODS_FOLDER;
    if (!fs::exists(modPath)) {
        fs::create_directory(modPath);
        std::cout << "[*] Created 'Mods' directory. Put your DLLs there." << std::endl;
    }

    // 2. Launch Game (Suspended)
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };

    std::cout << "[*] Launching " << targetExe << "..." << std::endl;
    if (!CreateProcessA(targetExe, NULL, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &si, &pi)) {
        std::cerr << "[!] Error: Could not launch game." << std::endl;
        return 1;
    }

    std::cout << "[*] Injecting Core API..." << std::endl;
    Injector::InjectDLL(pi.dwProcessId, apiDllName);

    std::cout << "[*] Scanning 'Mods' folder..." << std::endl;

    for (const auto& entry : fs::directory_iterator(MODS_FOLDER)) {
        if (entry.path().extension() == ".dll") {
            std::string dllPath = entry.path().string();
            std::string dllName = entry.path().filename().string();

            std::cout << "  [?] Checking: " << dllName << "... ";

            // SECURITY CHECK: Does it have the ModInfo struct?
            if (!DllSecurity::IsValidMod(dllPath)) {
                // REJECTED
                std::cout << "[BLOCKED]" << std::endl;
                std::cout << "      -> Missing 'MSZ_GetModInfo'. This DLL is not a valid mod." << std::endl;
                continue;
            }

            // APPROVED
            std::cout << "[VERIFIED] -> Injecting..." << std::endl;
            Injector::InjectDLL(pi.dwProcessId, dllPath.c_str());
        }
    }

    // 5. Resume
    std::cout << "[!] Resuming game..." << std::endl;
    ResumeThread(pi.hThread);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return 0;
}
