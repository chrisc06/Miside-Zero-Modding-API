#pragma once
#include <windows.h>
#include <string>

// Starts monitoring for Access Violations and Crashes
void InitCrashHandler();
        
// Removes the monitor
void RemoveCrashHandler();

// Helper to translate Exception Codes to strings
std::string ExceptionCodeToString(DWORD code);
