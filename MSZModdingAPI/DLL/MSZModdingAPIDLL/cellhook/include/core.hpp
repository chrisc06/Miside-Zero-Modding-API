#pragma once
#include <vector>
#include <memory>
#include <atomic>
#include <cstdint>

namespace cell {

    enum class status {
        success,
        unknown_error,
        allocation_failed,
        decoding_failed,
        not_hooked
    };

    class hook {
        friend class transaction;
    public:
        hook(void* target, void* detour, void** original_out);
        ~hook();
        bool prepare();
        void apply();
        void restore();
        bool is_enabled() const { return enabled; }

    private:
        void* target = nullptr;
        void* detour = nullptr;
        void** original_out = nullptr;
        void* trampoline = nullptr;
        std::vector<std::uint8_t> original_bytes;
        std::atomic<bool> enabled = false;
        size_t patch_size = 0;
    };

    class transaction {
    public:
        transaction() = default;

        template <typename T>
        void add(T* target, T* detour, T** original_out = nullptr) {
            add_internal((void*)target, (void*)detour, (void**)original_out);
        }

        void add(void* target, void* detour, void** original_out = nullptr) {
            add_internal(target, detour, original_out);
        }

        template <typename T>
        void add(void* target, void* detour, T** original_out) {
            add_internal(target, detour, (void**)original_out);
        }

        status commit();
        status rollback();

    private:
        void add_internal(void* target, void* detour, void** original_out);
        std::vector<std::shared_ptr<hook>> queue;
    };
}