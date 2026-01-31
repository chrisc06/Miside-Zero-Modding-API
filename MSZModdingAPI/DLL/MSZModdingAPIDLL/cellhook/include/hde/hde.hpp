#pragma once
#include <cstdint>

namespace cell::hde {

    struct instruction {
        std::uint8_t length;
        std::uint8_t opcode;
        bool is_rip_relative;
        std::uint8_t disp_offset;
        std::uint8_t imm_offset;
        std::uint8_t imm_size;
    };

    instruction decode(const void* address);
}