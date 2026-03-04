#include <iostream>
#include <windows.h>
#include <filesystem>
#include <vector>
#include <thread>
#include "Injector.h"
#include "Security.h" 

namespace fs = std::filesystem;

const char* REQUIRED_EXPORT = "MSZ_GetModInfo";
const char* REQUIRED_ENTRY_EXPORT = "MSZ_OnLoad";
const char* MODS_FOLDER = "Mods";

std::vector<fs::path> dlls;

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
            dlls.push_back(entry.path());
        }
    }

    std::sort(dlls.begin(), dlls.end(),
        [](const fs::path& a, const fs::path& b) {
            std::string as = a.filename().string();
            std::string bs = b.filename().string();
            std::transform(as.begin(), as.end(), as.begin(), ::tolower);
            std::transform(bs.begin(), bs.end(), bs.begin(), ::tolower);
            return as < bs;
        });

    for (auto& p : dlls) {
        std::string dllPath = fs::absolute(p).string();
        std::string dllName = p.filename().string();
         
        std::cout << "  [?] Checking: " << dllName << "... ";

        if (!DllSecurity::IsValidMod(dllPath)) {
            std::cout << "[BLOCKED]\n";
            std::cout << "      -> Missing '" << REQUIRED_EXPORT << "'.\n";
            continue;
        }

        std::cout << "[VERIFIED] -> Injecting...\n";
        if (!Injector::InjectDLL(pi.dwProcessId, dllPath.c_str())) {
            std::cout << "      -> [!] Injection failed\n";
        }
    }

    // 5. Resume
    std::cout << "[!] Resuming game..." << std::endl;
    ResumeThread(pi.hThread);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return 0;
}