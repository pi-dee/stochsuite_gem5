/*
 * x86_insn_len.hh — minimal x86-64 instruction-length decoder
 *
 * Shared between StochBranchMonitor and StochPrefetchMonitor.  Both need to
 * walk instruction streams at ELF-analysis time to find a specific opcode
 * (Jcc for branches, memory-read for loads) after a __stoch_* label.
 *
 * This is NOT a general-purpose disassembler.  Its only contract is:
 *   • return the correct byte length for every instruction it recognises
 *   • return 0 for unrecognised encodings so the caller can abort the scan
 *   • never misidentify a displacement or immediate byte as an opcode
 *
 * The general x86-64 instruction format:
 *   [legacy prefixes]  [REX]  opcode  [ModRM [SIB] [disp]]  [imm]
 */

#ifndef __STOCHSUITE_X86_INSN_LEN_HH__
#define __STOCHSUITE_X86_INSN_LEN_HH__

#include <cstddef>
#include <cstdint>

namespace gem5
{

/**
 * Return the byte length of one x86-64 instruction whose first byte is at
 * p[0], with `avail` bytes available.  Returns 0 on decode error (truncated
 * stream or unrecognised encoding).
 */
static size_t
x86InsnLen(const uint8_t *p, size_t avail)
{
    if (avail == 0)
        return 0;

    size_t i = 0;

    // ------------------------------------------------------------------
    // 1. Legacy prefixes (up to 4, any order)
    // ------------------------------------------------------------------
    bool hasOpSzPfx = false;
    bool hasRepPfx  = false;  // 0xF2 / 0xF3 (also mandatory SSE prefix)
    for (int pfxCount = 0; pfxCount < 4; ++pfxCount) {
        if (i >= avail)
            return 0;
        uint8_t b = p[i];
        if (b == 0x66) { hasOpSzPfx = true; ++i; }
        else if (b == 0x67) { ++i; }
        else if (b == 0xF2 || b == 0xF3) { hasRepPfx = true; ++i; }
        else if (b == 0xF0) { ++i; }
        else if (b == 0x2E || b == 0x3E || b == 0x26 ||
                 b == 0x36 || b == 0x64 || b == 0x65) { ++i; }
        else
            break;
    }
    (void)hasRepPfx;

    // ------------------------------------------------------------------
    // 2. REX prefix (0x40–0x4F in 64-bit mode)
    // ------------------------------------------------------------------
    bool hasREX = false;
    bool rexW   = false;
    if (i < avail && (p[i] & 0xF0) == 0x40) {
        hasREX = true;
        rexW   = (p[i] & 0x08) != 0;
        (void)hasREX;
        ++i;
    }

    if (i >= avail)
        return 0;

    // ------------------------------------------------------------------
    // 3. Opcode
    // ------------------------------------------------------------------
    uint8_t op = p[i++];

    // Helper: size of [ModRM] + [SIB] + [disp] in bytes; -1 on truncation.
    auto modRMSize = [&]() -> size_t {
        if (i >= avail)
            return (size_t)-1;
        uint8_t modrm = p[i];
        uint8_t mod   = (modrm >> 6) & 0x3;
        uint8_t rm    = modrm & 0x7;
        size_t  sz    = 1;

        if (mod == 3)
            return sz;

        bool hasSIB = (rm == 4);
        if (hasSIB) {
            ++sz;
            if (i + 1 >= avail)
                return (size_t)-1;
            uint8_t sib    = p[i + 1];
            uint8_t sibBase = sib & 0x7;
            if (mod == 0 && sibBase == 5)
                sz += 4;
        }

        if (mod == 1)
            sz += 1;
        else if (mod == 2)
            sz += 4;
        else if (mod == 0 && rm == 5)
            sz += 4; // RIP-relative

        if (i + sz - 1 >= avail)
            return (size_t)-1;
        return sz;
    };

    // ------------------------------------------------------------------
    // 4. Decode by opcode
    // ------------------------------------------------------------------

    if (op == 0x0F) {
        if (i >= avail)
            return 0;
        uint8_t op2 = p[i++];

        // Near Jcc: 0x0F 0x80–0x8F  rel32
        if (op2 >= 0x80 && op2 <= 0x8F)
            return i + 4;

        // SSE / scalar-FP ops: [pfx] 0F <op2> ModRM
        if (op2 == 0x10 || op2 == 0x11 || op2 == 0x12 || op2 == 0x13 ||
            op2 == 0x14 || op2 == 0x15 || op2 == 0x16 || op2 == 0x17 ||
            op2 == 0x28 || op2 == 0x29 || op2 == 0x2A || op2 == 0x2B ||
            op2 == 0x2C || op2 == 0x2D || op2 == 0x2E || op2 == 0x2F ||
            op2 == 0x51 || op2 == 0x57 || op2 == 0x58 || op2 == 0x59 ||
            op2 == 0x5A || op2 == 0x5B || op2 == 0x5C || op2 == 0x5D ||
            op2 == 0x5E || op2 == 0x5F ||
            op2 == 0x60 || op2 == 0x61 || op2 == 0x62 || op2 == 0x63 ||
            op2 == 0x64 || op2 == 0x65 || op2 == 0x66 || op2 == 0x67 ||
            op2 == 0x68 || op2 == 0x69 || op2 == 0x6A || op2 == 0x6B ||
            op2 == 0x6C || op2 == 0x6D || op2 == 0x6E || op2 == 0x6F ||
            op2 == 0x74 || op2 == 0x75 || op2 == 0x76 ||
            op2 == 0x7E || op2 == 0x7F ||
            op2 == 0xD0 || op2 == 0xD1 || op2 == 0xD2 || op2 == 0xD3 ||
            op2 == 0xD4 || op2 == 0xD5 || op2 == 0xD6 || op2 == 0xD7 ||
            op2 == 0xD8 || op2 == 0xD9 || op2 == 0xDA || op2 == 0xDB ||
            op2 == 0xDC || op2 == 0xDD || op2 == 0xDE || op2 == 0xDF ||
            op2 == 0xE0 || op2 == 0xE1 || op2 == 0xE2 || op2 == 0xE3 ||
            op2 == 0xE4 || op2 == 0xE5 || op2 == 0xE6 || op2 == 0xE7 ||
            op2 == 0xE8 || op2 == 0xE9 || op2 == 0xEA || op2 == 0xEB ||
            op2 == 0xEC || op2 == 0xED || op2 == 0xEE || op2 == 0xEF ||
            op2 == 0xF8 || op2 == 0xF9 || op2 == 0xFA || op2 == 0xFB ||
            op2 == 0xFC || op2 == 0xFD || op2 == 0xFE) {
            size_t ms = modRMSize();
            if (ms == (size_t)-1) return 0;
            return i + ms;
        }

        // SETCC: 0x0F 0x90–0x9F  ModRM
        if (op2 >= 0x90 && op2 <= 0x9F) {
            size_t ms = modRMSize();
            if (ms == (size_t)-1) return 0;
            return i + ms;
        }

        // CMOV: 0x0F 0x40–0x4F  ModRM
        if (op2 >= 0x40 && op2 <= 0x4F) {
            size_t ms = modRMSize();
            if (ms == (size_t)-1) return 0;
            return i + ms;
        }

        // MOVZX/MOVSX/BTx/BSF/BSR/IMUL/POPCNT/TZCNT/LZCNT
        if (op2 == 0xA3 || op2 == 0xAB || op2 == 0xAF ||
            op2 == 0xB3 || op2 == 0xB6 || op2 == 0xB7 ||
            op2 == 0xBB || op2 == 0xBC || op2 == 0xBD ||
            op2 == 0xBE || op2 == 0xBF) {
            size_t ms = modRMSize();
            if (ms == (size_t)-1) return 0;
            return i + ms;
        }

        // PSRL/PSLL/PSRA imm8: 0x0F 0x71/0x72/0x73  ModRM  imm8
        if (op2 == 0x71 || op2 == 0x72 || op2 == 0x73) {
            size_t ms = modRMSize();
            if (ms == (size_t)-1) return 0;
            return i + ms + 1;
        }

        // PINSRW: 0x0F 0xC4  ModRM  imm8
        if (op2 == 0xC4) {
            size_t ms = modRMSize();
            if (ms == (size_t)-1) return 0;
            return i + ms + 1;
        }

        // PEXTRW: 0x0F 0xC5  ModRM  imm8
        if (op2 == 0xC5) {
            size_t ms = modRMSize();
            if (ms == (size_t)-1) return 0;
            return i + ms + 1;
        }

        // SHUFPS/SHUFPD: 0x0F 0xC6  ModRM  imm8
        if (op2 == 0xC6) {
            size_t ms = modRMSize();
            if (ms == (size_t)-1) return 0;
            return i + ms + 1;
        }

        return 0; // unrecognised 0F escape
    }

    // Short Jcc: 0x70–0x7F  rel8
    if (op >= 0x70 && op <= 0x7F)
        return i + 1;

    // JMP short: 0xEB  rel8
    if (op == 0xEB) return i + 1;
    // JMP near: 0xE9  rel32
    if (op == 0xE9) return i + 4;
    // CALL near: 0xE8  rel32
    if (op == 0xE8) return i + 4;

    // RET
    if (op == 0xC3) return i;
    if (op == 0xC2) return i + 2;

    // One-byte ops with no operands
    if (op == 0x90 || op == 0x99 || op == 0x9B || op == 0x9C || op == 0x9D)
        return i;

    // PUSH/POP reg (0x50–0x5F)
    if (op >= 0x50 && op <= 0x5F) return i;

    // MOV reg, imm
    if (op >= 0xB0 && op <= 0xB7) return i + 1;
    if (op >= 0xB8 && op <= 0xBF) return i + (rexW ? 8 : 4);

    // ADD/OR/ADC/SBB/AND/SUB/XOR/CMP r/m, imm
    if (op == 0x80) { size_t ms = modRMSize(); if (ms == (size_t)-1) return 0; return i + ms + 1; }
    if (op == 0x81) { size_t ms = modRMSize(); if (ms == (size_t)-1) return 0; return i + ms + (hasOpSzPfx ? 2 : 4); }
    if (op == 0x83) { size_t ms = modRMSize(); if (ms == (size_t)-1) return 0; return i + ms + 1; }

    // TEST/XCHG/MOV/LEA/POP with ModRM (0x84–0x8F)
    if (op >= 0x84 && op <= 0x8F) { size_t ms = modRMSize(); if (ms == (size_t)-1) return 0; return i + ms; }

    // Common two-operand ALU: 0x00–0x3B
    if ((op & 0xC0) == 0x00 && (op & 0x06) != 0x04 && (op & 0x07) != 0x06) {
        if ((op & 0x07) <= 0x03) { size_t ms = modRMSize(); if (ms == (size_t)-1) return 0; return i + ms; }
    }

    // MOV r/m, imm
    if (op == 0xC6) { size_t ms = modRMSize(); if (ms == (size_t)-1) return 0; return i + ms + 1; }
    if (op == 0xC7) { size_t ms = modRMSize(); if (ms == (size_t)-1) return 0; return i + ms + (hasOpSzPfx ? 2 : 4); }

    // INC/DEC/CALL/JMP/PUSH r/m (0xFF /r)
    if (op == 0xFF) { size_t ms = modRMSize(); if (ms == (size_t)-1) return 0; return i + ms; }

    // CMP/TEST AL/rAX, imm
    if (op == 0x3C) return i + 1;
    if (op == 0x3D) return i + (hasOpSzPfx ? 2 : 4);
    if (op == 0xA8) return i + 1;
    if (op == 0xA9) return i + (hasOpSzPfx ? 2 : 4);

    // ADD/SUB/XOR/OR/AND rAX, imm
    if (op == 0x05 || op == 0x15 || op == 0x25 || op == 0x35)
        return i + (hasOpSzPfx ? 2 : 4);

    // PUSH imm
    if (op == 0x6A) return i + 1;
    if (op == 0x68) return i + (hasOpSzPfx ? 2 : 4);

    // IMUL r, r/m, imm
    if (op == 0x6B) { size_t ms = modRMSize(); if (ms == (size_t)-1) return 0; return i + ms + 1; }
    if (op == 0x69) { size_t ms = modRMSize(); if (ms == (size_t)-1) return 0; return i + ms + (hasOpSzPfx ? 2 : 4); }

    // XCHG rAX, rN
    if (op >= 0x91 && op <= 0x97) return i;

    // String ops
    if (op >= 0xA4 && op <= 0xA7) return i;
    if (op >= 0xAA && op <= 0xAF) return i;

    return 0; // unrecognised
}

} // namespace gem5

#endif // __STOCHSUITE_X86_INSN_LEN_HH__
