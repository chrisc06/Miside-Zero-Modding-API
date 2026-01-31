#include "hde/hde.hpp"
#include <array>

namespace cell::hde {

    namespace internal {
        constexpr std::uint8_t f_none = 0x00;
        constexpr std::uint8_t f_modrm = 0x01;
        constexpr std::uint8_t f_imm8 = 0x02;
        constexpr std::uint8_t f_imm16 = 0x04;
        constexpr std::uint8_t f_imm32 = 0x08;
        constexpr std::uint8_t f_imm64 = 0x10;
        constexpr std::uint8_t f_rel = 0x20;

        constexpr std::array<std::uint8_t, 256> generate_table() {
            std::array<std::uint8_t, 256> t = {};

            for (int i = 0; i < 256; i++) t[i] = f_modrm;

            for (int i = 0x50; i <= 0x5F; i++) t[i] = f_none;

            t[0x80] = f_modrm | f_imm8;
            t[0x81] = f_modrm | f_imm32;
            t[0x82] = f_modrm | f_imm8;
            t[0x83] = f_modrm | f_imm8;

            t[0xA0] = f_imm64; t[0xA1] = f_imm64;
            t[0xA2] = f_imm64; t[0xA3] = f_imm64;

            for (int i = 0xB0; i <= 0xB7; i++) t[i] = f_imm8;
            for (int i = 0xB8; i <= 0xBF; i++) t[i] = f_imm32;

            t[0xC0] = f_modrm | f_imm8;
            t[0xC1] = f_modrm | f_imm8;
            t[0xC2] = f_imm16;
            t[0xC3] = f_none;
            t[0xC6] = f_modrm | f_imm8;
            t[0xC7] = f_modrm | f_imm32;

            t[0xE8] = f_imm32 | f_rel;
            t[0xE9] = f_imm32 | f_rel;
            t[0xEB] = f_imm8 | f_rel;

            return t;
        }

        constexpr auto opcode_table = generate_table();
    }

    instruction decode(const void* address) {
        auto* p = reinterpret_cast<const std::uint8_t*>(address);
        const auto* start = p;

        instruction ins = {};

        bool rex_w = false;
        bool op_size_override = false;
        bool addr_size_override = false;
        bool vex = false;
        std::uint8_t vex_op = 0;

        while (true) {
            const auto b = *p;

            if (b == 0xC4 || b == 0xC5 || b == 0x62) {
                vex = true; vex_op = b; break;
            }
            else if (b == 0x8F && ((p[1] & 0x38) != 0)) {
                vex = true; vex_op = b; break;
            }
            else if ((b & 0xF0) == 0x40) {
                if (b & 0x08) rex_w = true;
            }
            else if (b == 0x66) op_size_override = true;
            else if (b == 0x67) addr_size_override = true;
            else if (b == 0xF0 || b == 0xF2 || b == 0xF3 || b == 0x2E || b == 0x36 || b == 0x3E || b == 0x26 || b == 0x64 || b == 0x65) {}
            else break;
            p++;
        }

        std::uint8_t b = 0;
        std::uint8_t flags = 0;
        bool has_modrm = false;

        if (vex) {
            p++;
            if (vex_op == 0xC5) p++;
            else if (vex_op == 0xC4 || vex_op == 0x8F) p += 2;
            else if (vex_op == 0x62) p += 3;

            b = *p++;
            has_modrm = true;

            if (b == 0xC2 || b == 0xC4 || b == 0xC5 || b == 0xC6 ||
                b == 0xCC || b == 0xCD || b == 0xCE || b == 0xCF)
                flags |= internal::f_imm8;
        }
        else {
            b = *p++;
            if (b == 0x0F) {
                b = *p++;
                if (b == 0x0F) flags = internal::f_modrm | internal::f_imm8;
                else if ((b & 0xF0) == 0x80) flags = internal::f_imm32 | internal::f_rel;
                else flags = internal::f_modrm;
            }
            else {
                flags = internal::opcode_table[b];

                if (b >= 0xA0 && b <= 0xA3) {
                    if (addr_size_override) flags = internal::f_imm32;
                    else flags = internal::f_imm64;
                }
                else if (b >= 0xB8 && b <= 0xBF) {
                    flags = (rex_w ? internal::f_imm64 : internal::f_imm32);
                }
            }
        }

        ins.opcode = b;
        if (flags & internal::f_modrm) has_modrm = true;

        if (has_modrm) {
            const auto modrm = *p++;
            const auto mod = (modrm >> 6) & 0x03;
            const auto rm = (modrm & 0x07);

            if (mod != 3 && rm == 4) p++;

            if (mod == 1) { ins.disp_offset = (std::uint8_t)(p - start); p += 1; }
            else if (mod == 2) { ins.disp_offset = (std::uint8_t)(p - start); p += 4; }
            else if (mod == 0 && rm == 5) {
                ins.is_rip_relative = true;
                ins.disp_offset = (std::uint8_t)(p - start);
                p += 4;
            }
        }

        if (flags & (internal::f_imm8 | internal::f_imm16 | internal::f_imm32 | internal::f_imm64)) {
            ins.imm_offset = (std::uint8_t)(p - start);

            if (flags & internal::f_imm8) { ins.imm_size = 1; p += 1; }
            else if (flags & internal::f_imm16) { ins.imm_size = 2; p += 2; }
            else if (flags & internal::f_imm32) {
                ins.imm_size = 4;
                if (op_size_override && !(flags & internal::f_rel)) p += 2;
                else p += 4;
            }
            else if (flags & internal::f_imm64) { ins.imm_size = 8; p += 8; }
        }

        ins.length = (std::uint8_t)(p - start);
        return ins;
    }
}