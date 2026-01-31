#pragma once
#include <cstddef>
#include <iostream>
#include <iomanip>

// Uncomment for debug (lol)
//#define CELL_DEBUG

#ifdef CELL_DEBUG
#define LOG(x) std::cout << "\033[32m[Cell\033[37mHook]\033[0m " << x << std::endl
#else
#define LOG(x) do {} while(0)
#endif

namespace cell::utils {
    void set_all_threads_state(bool suspend);
    void fix_thread_contexts(void* target, std::size_t len, void* trampoline_rx);
}