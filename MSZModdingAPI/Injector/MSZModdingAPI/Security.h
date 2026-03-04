#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <algorithm>


enum SecurityFlags {
    RISK_NONE = 0,
    RISK_NETWORK = 1 << 0,
    RISK_SYSTEM = 1 << 1 
};

class DllSecurity {
public:
    static std::string ReadFileContent(const std::string& path) {
        std::ifstream f(path, std::ios::binary);
        if (!f) return "";
        return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    }

    static bool IsValidMod(const std::string& filePath) {
        HMODULE hMod = LoadLibraryExA(filePath.c_str(), NULL, DONT_RESOLVE_DLL_REFERENCES);

        if (!hMod) return false;

        // Check for the mandatory export signature
        bool hasExport = (GetProcAddress(hMod, "MSZ_GetModInfo") != NULL);

        FreeLibrary(hMod);
        return hasExport;
    }

    static int ScanForRisks(const std::string& filePath) {
        std::string content = ReadFileContent(filePath);
        if (content.empty()) return RISK_NONE;

        // Convert file content to lowercase for easier searching
        std::transform(content.begin(), content.end(), content.begin(), ::tolower);

        int risks = RISK_NONE;

        if (content.find("ws2_32.dll") != std::string::npos ||
            content.find("wininet.dll") != std::string::npos ||
            content.find("urlmon.dll") != std::string::npos ||
            content.find("socket") != std::string::npos ||
            content.find("urldownloadtofile") != std::string::npos) {

            risks |= RISK_NETWORK;
        }

        if (content.find("shellexecute") != std::string::npos ||
            content.find("createprocess") != std::string::npos ||
            content.find("regsetvalue") != std::string::npos) {

            risks |= RISK_SYSTEM;
        }

        return risks;
    }
};