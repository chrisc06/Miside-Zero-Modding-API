#include "../include/core.hpp"
#include "hde/hde.hpp"
#include "../include/memory.hpp"
#include "../include/utils.hpp"
#include <windows.h>
#include <cstring>
#include <vector>
#include <iostream>
#include <iomanip>

namespace cell {

    hook::hook(void* tgt, void* dtr, void** org)
        : target(tgt), detour(dtr), original_out(org) {
    }

    hook::~hook() {
        if (enabled) restore();
        if (trampoline) memory::free_gateway(trampoline);
    }

    void* resolve_jumps(void* addr) {
        auto* p = reinterpret_cast<std::uint8_t*>(addr);
        int depth = 0;
        while (depth < 10) {
            if (p[0] == 0xE9) {
                const auto offset = *reinterpret_cast<std::int32_t*>(p + 1);
                void* target = p + 5 + offset;

                p = (std::uint8_t*)target;
            }
            else if (p[0] == 0xFF && p[1] == 0x25) {
                const auto offset = *reinterpret_cast<std::int32_t*>(p + 2);
                const auto* target_ptr = reinterpret_cast<std::uintptr_t*>(p + 6 + offset);
                void* target = reinterpret_cast<std::uint8_t*>(*target_ptr);

                p = (std::uint8_t*)target;
            }
            else break;
            depth++;
        }
        return p;
    }

    bool hook::prepare() {
        if (trampoline) return true;

        void* real_target = resolve_jumps(target);
        if (real_target != target) {
            LOG("Resolved Thunk: 0x" << std::hex << target << " -> 0x" << real_target);
            target = real_target;
        }

        LOG("Prepare Target: 0x" << std::hex << target);

        std::size_t len = 0;
        auto* ptr = reinterpret_cast<std::uint8_t*>(target);

        while (len < 5) {
            const auto ins = hde::decode(ptr + len);
            if (ins.length == 0) {
                LOG("ERROR: Unknown instruction opcode at offset " << std::dec << len << ": 0x" << std::hex << (int)*(ptr + len));
                return false;
            }
            len += ins.length;
        }
        patch_size = len;
        LOG("Stolen bytes: " << std::dec << patch_size);

        constexpr std::size_t relay_size = 14;
        // Allocate 128 bytes for instruction expansion
        trampoline = memory::allocate_gateway(target, len + 128 + relay_size);
        if (!trampoline) {
            LOG("ERROR: memory::allocate_gateway failed!");
            return false;
        }

        auto* p_base = reinterpret_cast<std::uint8_t*>(trampoline);

        // JMP [RIP+0] -> Detour
        p_base[0] = 0xFF; p_base[1] = 0x25;
        *reinterpret_cast<std::int32_t*>(p_base + 2) = 0;
        *reinterpret_cast<std::uint64_t*>(p_base + 6) = reinterpret_cast<std::uintptr_t>(detour);

        auto* p_original = p_base + relay_size;
        auto* src_ptr = ptr;
        std::size_t processed = 0;

        LOG("Building Relay...");

        while (processed < len) {
            const auto ins = hde::decode(src_ptr);
            bool instruction_handled = false;

            if (ins.is_rip_relative) {
                const auto original_disp = *reinterpret_cast<std::int32_t*>(src_ptr + ins.disp_offset);
                const auto target_data_addr = reinterpret_cast<std::uintptr_t>(src_ptr) + ins.length + original_disp;
                const auto current_rip = reinterpret_cast<std::uintptr_t>(p_original) + ins.length;
                const auto new_disp = static_cast<std::int64_t>(target_data_addr) - current_rip;

                if (new_disp >= INT32_MIN && new_disp <= INT32_MAX) {
                    std::memcpy(p_original, src_ptr, ins.length);
                    *reinterpret_cast<std::int32_t*>(p_original + ins.disp_offset) = static_cast<std::int32_t>(new_disp);
                    p_original += ins.length;
                    instruction_handled = true;
                }
                else if (ins.opcode == 0x8D) { // LEA
                    LOG(" > Detected Out-of-Range LEA (Op: 0x8D). Rewriting to Absolute MOV...");

                    std::uint8_t rex = 0;
                    std::uint8_t modrm = 0;

                    if ((src_ptr[0] & 0xF0) == 0x40) {
                        rex = src_ptr[0];
                        modrm = src_ptr[2];
                    }
                    else {
                        modrm = src_ptr[1];
                    }

                    std::uint8_t reg = (modrm >> 3) & 7;
                    if (rex & 0x04) reg += 8;

                    // Rewrite: MOV REG, IMM64
                    if (reg < 8) {
                        p_original[0] = 0x48;
                        p_original[1] = 0xB8 + reg;
                    }
                    else {
                        p_original[0] = 0x49;
                        p_original[1] = 0xB8 + (reg - 8);
                    }

                    *reinterpret_cast<std::uint64_t*>(p_original + 2) = target_data_addr;

                    p_original += 10;
                    instruction_handled = true;
                    LOG("   -> Rewrote LEA to MOV R" << std::dec << (int)reg << ", 0x" << std::hex << target_data_addr);
                }
                else {
                    LOG("ERROR: Out-of-Range RIP Relative instruction that is NOT LEA! Opcode: 0x" << std::hex << (int)ins.opcode);
                    LOG("   -> Target Data: 0x" << target_data_addr);
                    LOG("   -> Trampoline RIP: 0x" << current_rip);
                    return false;
                }
            }

            if (!instruction_handled) {
                std::memcpy(p_original, src_ptr, ins.length);
                p_original += ins.length;
            }

            src_ptr += ins.length;
            processed += ins.length;
        }

        // JMP Back
#ifdef _WIN64
        p_original[0] = 0xFF; p_original[1] = 0x25;
        *reinterpret_cast<std::int32_t*>(p_original + 2) = 0;
        *reinterpret_cast<std::uint64_t*>(p_original + 6) = reinterpret_cast<std::uint64_t>(ptr + len);
#else
        p_original[0] = 0xE9;
        *reinterpret_cast<std::int32_t*>(p_original + 1) = reinterpret_cast<std::uintptr_t>(ptr + len) - reinterpret_cast<std::uintptr_t>(p_original + 5);
#endif

        std::size_t trampoline_len = (std::uintptr_t)p_original + 14 - (std::uintptr_t)trampoline;
        FlushInstructionCache(GetCurrentProcess(), trampoline, trampoline_len);
        LOG("Trampoline flushed. Total Size: " << std::dec << trampoline_len);

        original_bytes.resize(patch_size);
        std::memcpy(original_bytes.data(), target, patch_size);

        if (original_out) *original_out = (p_base + relay_size);

        LOG("Prepare success.");
        return true;
    }

    void hook::apply() {
        LOG("Applying hook to 0x" << std::hex << target);

        std::int64_t dist = static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(trampoline)) - (static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(target)) + 5);

        if (dist > INT32_MAX || dist < INT32_MIN) {
            LOG("CRITICAL ERROR: Jump distance > 2GB! The Allocator returned a bad address.");
            LOG("  Target: " << target);
            LOG("  Gateway: " << trampoline);
            return;
        }

        DWORD old;
        VirtualProtect(target, patch_size, PAGE_EXECUTE_READWRITE, &old);
        auto* ptr = reinterpret_cast<std::uint8_t*>(target);

        ptr[0] = 0xE9;
        *reinterpret_cast<std::int32_t*>(ptr + 1) = static_cast<std::int32_t>(dist);

        for (std::size_t i = 5; i < patch_size; i++) ptr[i] = 0x90;

        VirtualProtect(target, patch_size, old, &old);
        FlushInstructionCache(GetCurrentProcess(), target, patch_size);
        enabled = true;
        LOG("Hook applied successfully.");
    }

    void hook::restore() {
        LOG("Restoring hook...");
        DWORD old;
        VirtualProtect(target, patch_size, PAGE_EXECUTE_READWRITE, &old);
        std::memcpy(target, original_bytes.data(), patch_size);
        VirtualProtect(target, patch_size, old, &old);
        FlushInstructionCache(GetCurrentProcess(), target, patch_size);
        enabled = false;
    }

    void transaction::add_internal(void* target, void* detour, void** original_out) {
        queue.push_back(std::make_shared<hook>(target, detour, original_out));
    }

    status transaction::commit() {
        LOG("Committing transaction. Items: " << queue.size());
        for (const auto& h : queue) if (!h->prepare()) {
            LOG("Prepare failed.");
            return status::decoding_failed;
        }

        utils::set_all_threads_state(true);
        for (const auto& h : queue) {
            if (!h->is_enabled()) {
                auto* trampoline_code_start = (void*)((std::uintptr_t)h->trampoline + 14);
                utils::fix_thread_contexts(h->target, h->patch_size, trampoline_code_start);
                h->apply();
            }
        }
        utils::set_all_threads_state(false);
        LOG("Commit finished.");
        return status::success;
    }

    status transaction::rollback() {
        utils::set_all_threads_state(true);
        for (const auto& h : queue) if (h->is_enabled()) h->restore();
        utils::set_all_threads_state(false);
        return status::success;
    }
}