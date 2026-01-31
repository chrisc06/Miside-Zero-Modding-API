#include "memory.hpp"

#include <windows.h>
#include <cstdint>
#include <iostream>
#include <iomanip>

#define CELL_DEBUG
#ifdef CELL_DEBUG
#define LOG(x) std::cout << "\033[32m[Cell\033[37mHook]\033[0m " << x << std::endl
#else
#define LOG(x) do {} while(0)
#endif

namespace cell::memory {

    bool make_executable(void* addr, std::size_t size) {
        DWORD old;
        if (VirtualProtect(addr, size, PAGE_EXECUTE_READWRITE, &old)) return true;
        LOG("Allocator: Failed to force PAGE_EXECUTE_READWRITE! Error: " << GetLastError());
        return false;
    }

    void* allocate_gateway(void* target, std::size_t size) {
        LOG("Allocator: Requesting " << std::dec << size << " bytes near 0x" << std::hex << target);

        SYSTEM_INFO si;
        GetSystemInfo(&si);
        const auto page_size = si.dwPageSize;

        // Without this cast, ~(page_size - 1) is 32-bit (0xFFFFF000)
        const std::uintptr_t mask = ~(static_cast<std::uintptr_t>(page_size) - 1);

        const auto alloc_size = (size + page_size - 1) & mask;
        const auto target_addr = reinterpret_cast<std::uintptr_t>(target);

        const std::int64_t max_dist = 0x40000000;

        const auto min_addr = (target_addr > (std::uintptr_t)max_dist) ? target_addr - max_dist : (std::uintptr_t)si.lpMinimumApplicationAddress;
        auto max_addr = target_addr + max_dist;
        if (max_addr > (std::uintptr_t)si.lpMaximumApplicationAddress) max_addr = (std::uintptr_t)si.lpMaximumApplicationAddress;

        // [FIX] Use the 64-bit mask
        auto current = (target_addr + page_size - 1) & mask;

        while (current < max_addr) {
            MEMORY_BASIC_INFORMATION mbi;
            if (VirtualQuery((void*)current, &mbi, sizeof(mbi)) == 0) break;

            if (mbi.State == MEM_FREE && mbi.RegionSize >= alloc_size) {
                void* ptr = VirtualAlloc((void*)current, alloc_size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
                if (ptr) {
                    if (make_executable(ptr, alloc_size)) {
                        LOG("Allocator: [UP] Found slot at 0x" << std::hex << ptr);
                        return ptr;
                    }
                }
            }
            current += mbi.RegionSize;
        }

        LOG("Allocator: [UP] failed. Switching to [DOWN] scan...");

        // [FIX] Use the 64-bit mask
        current = (target_addr & mask) - page_size;

        while (current > min_addr) {
            MEMORY_BASIC_INFORMATION mbi;
            if (VirtualQuery((void*)current, &mbi, sizeof(mbi)) == 0) break;

            if (mbi.State == MEM_FREE) {
                const auto free_end = (std::uintptr_t)mbi.BaseAddress + mbi.RegionSize;
                auto try_addr = free_end - alloc_size;

                // [FIX] Use the 64-bit mask
                try_addr = try_addr & mask;

                if (try_addr >= (std::uintptr_t)mbi.BaseAddress) {
                    void* ptr = VirtualAlloc((void*)try_addr, alloc_size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
                    if (ptr) {
                        if (make_executable(ptr, alloc_size)) {
                            LOG("Allocator: [DOWN] Found slot at 0x" << std::hex << ptr);
                            return ptr;
                        }
                    }
                }
            }
            if ((std::uintptr_t)mbi.BaseAddress < page_size) break;
            current = (std::uintptr_t)mbi.BaseAddress - page_size;
        }

        LOG("Allocator: CRITICAL FAILURE! No memory slot found.");
        return nullptr;
    }

    void free_gateway(void* gateway) {
        if (gateway) VirtualFree(gateway, 0, MEM_RELEASE);
    }
}