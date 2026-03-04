#pragma once
#include <unordered_map>
#include <mutex>
#include <string>
#include "Hook.h"

namespace Scanner {
    inline std::unordered_map<std::string, uintptr_t> Cache;
    inline std::mutex CacheMutex;

    uintptr_t GetMethod(const char* assemblyName, const char* namespaze, const char* className, const char* methodName, int argsCount);
    uintptr_t GetField(const char* assemblyName, const char* namespaze, const char* className, const char* fieldName);
    void* GetClass(const char* assemblyName, const char* namespaze, const char* className);

    void Initialize();
}