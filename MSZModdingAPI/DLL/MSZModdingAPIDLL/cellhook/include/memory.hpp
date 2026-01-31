#pragma once
#include <cstddef>

namespace cell::memory {
    void* allocate_gateway(void* origin, size_t size);

    void free_gateway(void* address);
}