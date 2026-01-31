#include "utils.hpp"

#include <windows.h>
#include <tlhelp32.h>
#include <vector>

namespace cell::utils {
    void set_all_threads_state(bool suspend) {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snap == INVALID_HANDLE_VALUE) return;

        THREADENTRY32 te = { sizeof(THREADENTRY32) };
        const auto my_tid = GetCurrentThreadId();
        const auto my_pid = GetCurrentProcessId();

        if (Thread32First(snap, &te)) {
            do {
                if (te.th32OwnerProcessID == my_pid && te.th32ThreadID != my_tid) {
                    HANDLE h_thread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
                    if (h_thread) {
                        if (suspend) SuspendThread(h_thread);
                        else ResumeThread(h_thread);
                        CloseHandle(h_thread);
                    }
                }
            } while (Thread32Next(snap, &te));
        }
        CloseHandle(snap);
    }

    void fix_thread_contexts(void* target, std::size_t len, void* trampoline_rx) {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snap == INVALID_HANDLE_VALUE) return;

        THREADENTRY32 te = { sizeof(THREADENTRY32) };
        const auto my_tid = GetCurrentThreadId();
        const auto my_pid = GetCurrentProcessId();

        const auto u_target = reinterpret_cast<std::uintptr_t>(target);
        const auto u_tramp = reinterpret_cast<std::uintptr_t>(trampoline_rx);

        if (Thread32First(snap, &te)) {
            do {
                if (te.th32OwnerProcessID == my_pid && te.th32ThreadID != my_tid) {
                    HANDLE h_thread = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, FALSE, te.th32ThreadID);
                    if (h_thread) {
                        CONTEXT ctx;
                        ctx.ContextFlags = CONTEXT_CONTROL;
                        if (GetThreadContext(h_thread, &ctx)) {
#ifdef _WIN64
                            const auto ip = ctx.Rip;
#else
                            const auto ip = ctx.Eip;
#endif

                            if (ip >= u_target && ip < (u_target + len)) {
                                const auto offset = ip - u_target;
                                const auto new_ip = u_tramp + offset;
#ifdef _WIN64
                                ctx.Rip = new_ip;
#else
                                ctx.Eip = new_ip;
#endif
                                SetThreadContext(h_thread, &ctx);
                                LOG(" > Fixed Thread IP: 0x" << std::hex << ip << " -> 0x" << new_ip);
                            }
                        }
                        CloseHandle(h_thread);
                    }
                }
            } while (Thread32Next(snap, &te));
        }
        CloseHandle(snap);
    }
}