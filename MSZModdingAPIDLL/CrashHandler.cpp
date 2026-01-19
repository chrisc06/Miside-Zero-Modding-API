#include "CrashHandler.h"
#include "API.h"

PVOID HandlerHandle = nullptr;

const char* GetExceptionName(DWORD code) {
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION: return "Access Violation";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "Out of Bounds";
    case EXCEPTION_ILLEGAL_INSTRUCTION: return "Illegal Instruction";
    case EXCEPTION_STACK_OVERFLOW: return "Stack Overflow";
    default: return "Unknown Exception";
    }
}

LONG CALLBACK VectoredExceptionHandler(PEXCEPTION_POINTERS ExceptionInfo) {
    DWORD code = ExceptionInfo->ExceptionRecord->ExceptionCode;

    if (code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_ILLEGAL_INSTRUCTION) {

        LogE("--------------------------------------------------");
        LogE("!!! CRASH INTERCEPTED !!!");
        LogE("Code: %s", GetExceptionName(code));
        LogE("Faulting Address: 0x%p", ExceptionInfo->ExceptionRecord->ExceptionAddress);

        if (code == EXCEPTION_ACCESS_VIOLATION) {
            uintptr_t type = ExceptionInfo->ExceptionRecord->ExceptionInformation[0];
            uintptr_t addr = ExceptionInfo->ExceptionRecord->ExceptionInformation[1];
            LogE("Memory %s at address: 0x%p", (type == 0 ? "READ" : "WRITE"), (void*)addr);
        }
        LogE("--------------------------------------------------");
        LogW("Thread suspended to prevent game exit. Mod functionality will most likely be broken until next launch.");

        Sleep(INFINITE);

        return EXCEPTION_CONTINUE_EXECUTION;
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

void InitCrashHandler() {
    HandlerHandle = AddVectoredExceptionHandler(0, VectoredExceptionHandler);
}

void RemoveCrashHandler() {
    if (HandlerHandle) {
        RemoveVectoredExceptionHandler(HandlerHandle);
        HandlerHandle = nullptr;
    }
}