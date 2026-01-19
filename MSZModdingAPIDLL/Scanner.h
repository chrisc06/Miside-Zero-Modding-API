#pragma once
#include <map>
#include <string>
#include "Hook.h"

namespace Scanner {
    inline std::map<std::string, uintptr_t> Cache;

    uintptr_t GetMethod(const char* assemblyName, const char* namespaze, const char* className, const char* methodName, int argsCount);
    uintptr_t GetField(const char* assemblyName, const char* namespaze, const char* className, const char* fieldName);
    void* GetClass(const char* assemblyName, const char* namespaze, const char* className);

    void Initialize();
}