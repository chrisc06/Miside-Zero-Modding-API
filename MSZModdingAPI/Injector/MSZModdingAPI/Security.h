#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <algorithm>

// --- Security Risk Flags ---
// Used by the scanner to report what it found inside the DLL
enum SecurityFlags {
    RISK_NONE = 0,
    RISK_NETWORK = 1 << 0, // Found ws2_32.dll, wininet, etc.
    RISK_SYSTEM = 1 << 1  // Found ShellExecute, Registry, etc.
};

class DllSecurity {
public:
    // Helper to read the whole file into a string for binary scanning
    static std::string ReadFileContent(const std::string& path) {
        std::ifstream f(path, std::ios::binary);
        if (!f) return "";
        return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    }

    // 1. Verify Mod Identity
    static bool IsValidMod(const std::string& filePath) {
        HMODULE hMod = LoadLibraryExA(filePath.c_str(), NULL, DONT_RESOLVE_DLL_REFERENCES);

        if (!hMod) return false;

        // Check for the mandatory export signature
        bool hasExport = (GetProcAddress(hMod, "MSZ_GetModInfo") != NULL);

        FreeLibrary(hMod);
        return hasExport;
    }

    // 2. Scan for Risks (Static Analysis)
    // Returns a bitmask of what the mod *actually* contains.
    static int ScanForRisks(const std::string& filePath) {
        std::string content = ReadFileContent(filePath);
        if (content.empty()) return RISK_NONE;

        // Convert file content to lowercase for easier searching
        std::transform(content.begin(), content.end(), content.begin(), ::tolower);

        int risks = RISK_NONE;

        // --- CHECK 1: NETWORK CAPABILITIES ---
        // Look for imports associated with Internet Access
        if (content.find("ws2_32.dll") != std::string::npos ||
            content.find("wininet.dll") != std::string::npos ||
            content.find("urlmon.dll") != std::string::npos ||
            content.find("socket") != std::string::npos ||
            content.find("urldownloadtofile") != std::string::npos) {

            risks |= RISK_NETWORK;
        }

        // --- CHECK 2: SYSTEM / DANGEROUS CAPABILITIES ---
        // Look for imports associated with launching processes or editing system settings
        if (content.find("shellexecute") != std::string::npos ||
            content.find("createprocess") != std::string::npos ||
            content.find("regsetvalue") != std::string::npos) {

            risks |= RISK_SYSTEM;
        }

        return risks;
    }
};