#include "stochsuite/stoch_branch_monitor.hh"

#include <elf.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>
#include <stdexcept>
#include <string>

#include "base/logging.hh"
#include "base/trace.hh"
#include "debug/StochBranchMonitor.hh"
#include "sim/probe/probe.hh"

namespace gem5
{

// ---------------------------------------------------------------------------
// BranchSymbolStats
// ---------------------------------------------------------------------------

StochBranchMonitor::BranchSymbolStats::BranchSymbolStats(
        statistics::Group *parent, const std::string &symbolName)
    : statistics::Group(parent, symbolName.c_str()),
      predictions(this, "predictions",
                  "Branch prediction lookups at this stochastic site"),
      mispredictions(this, "mispredictions",
                     "Committed mispredictions at this stochastic site")
{
}

// ---------------------------------------------------------------------------
// StochBranchMonitor
// ---------------------------------------------------------------------------

StochBranchMonitor::StochBranchMonitor(const Params &p)
    : SimObject(p),
      binaryPath(p.binary)
{
}

void
StochBranchMonitor::startup()
{
    parseElf();

    if (!params().bpred) {
        warn("StochBranchMonitor: bpred not set — probe listeners not connected, "
             "no branch statistics will be collected");
        return;
    }

    // params().bpred is a SimObject* (the BPredUnit); grab its ProbeManager
    // so we can attach listeners to the "BranchPC" and "MissPC" probe points
    // that were added to BPredUnit::regProbePoints().
    ProbeManager *pm = params().bpred->getProbeManager();

    // Verify the probe points exist before connecting; if they are missing
    // (e.g. regProbePoints() was never called on this BPredUnit) the connect
    // call silently fails and we would always see 0 predictions.
    warn_if(!pm->getFirstProbePoint("BranchPC"),
            "StochBranchMonitor: 'BranchPC' probe not found on %s — "
            "check that BPredUnit::regProbePoints() added it",
            params().bpred->name());
    warn_if(!pm->getFirstProbePoint("MissPC"),
            "StochBranchMonitor: 'MissPC' probe not found on %s",
            params().bpred->name());

    // ProbeManager::connect<T>(args...) constructs the listener, registers it
    // with the named probe point, and returns a ProbeListenerPtr that
    // auto-deregisters on destruction.
    listeners_.push_back(
        pm->connect<ProbeListenerArg<StochBranchMonitor, Addr>>(
            this, "BranchPC", &StochBranchMonitor::notifyBranchPC));

    listeners_.push_back(
        pm->connect<ProbeListenerArg<StochBranchMonitor, Addr>>(
            this, "MissPC", &StochBranchMonitor::notifyMissBranchPC));

    inform("StochBranchMonitor: connected to %s, tracking %d site(s) from %s",
           params().bpred->name(), (int)branchStats.size(), binaryPath);

    DPRINTF(StochBranchMonitor, "Tracking %d stochastic branch site(s) "
            "from %s\n", (int)branchStats.size(), binaryPath);
}

void
StochBranchMonitor::notifyBranchPC(const Addr &pc)
{
    DPRINTF(StochBranchMonitor, "notifyBranchPC: pc=%#x\n", pc);
    auto it = branchStats.find(pc);
    if (it != branchStats.end()) {
        it->second->predictions++;
    }
}

void
StochBranchMonitor::notifyMissBranchPC(const Addr &pc)
{
    auto it = branchStats.find(pc);
    if (it != branchStats.end()) {
        it->second->mispredictions++;
    }
}

// ---------------------------------------------------------------------------
// x86InsnLen: return the byte length of one x86-64 instruction starting at
// `p` with `avail` bytes available.  Returns 0 on decode error (truncated
// instruction or unrecognised encoding); the caller should bail out.
//
// This is a minimal decoder covering the encodings that appear in the
// straight-line code stochsuite compilers emit after the __stoch_br_* label
// (SSE/scalar FP ops, integer compares, short branches).  It is NOT a full
// general-purpose disassembler; its only contract is:
//   • never identify a Jcc at a non-instruction boundary
//   • return the correct length for any instruction it does recognise
//
// The general x86-64 instruction format:
//   [legacy prefixes]  [REX]  opcode  [ModRM [SIB] [disp]]  [imm]
// ---------------------------------------------------------------------------
static size_t
x86InsnLen(const uint8_t *p, size_t avail)
{
    if (avail == 0)
        return 0;

    size_t i = 0; // walking index through the instruction bytes

    // -----------------------------------------------------------------------
    // 1. Legacy prefixes (can appear in any order, up to 4 distinct groups)
    // -----------------------------------------------------------------------
    bool hasOpSzPfx = false; // 0x66 operand-size override
    bool hasRepPfx  = false; // 0xF2 / 0xF3 (REP / REPNE / mandatory SSE pfx)
    for (int pfxCount = 0; pfxCount < 4; ++pfxCount) {
        if (i >= avail)
            return 0;
        uint8_t b = p[i];
        if (b == 0x66) { hasOpSzPfx = true; ++i; }
        else if (b == 0x67) { ++i; } // address-size override
        else if (b == 0xF2 || b == 0xF3) { hasRepPfx = true; ++i; }
        else if (b == 0xF0) { ++i; } // LOCK prefix
        else if (b == 0x2E || b == 0x3E || b == 0x26 ||
                 b == 0x36 || b == 0x64 || b == 0x65) { ++i; } // segment overrides
        else
            break;
    }

    // -----------------------------------------------------------------------
    // 2. REX prefix (0x40 – 0x4F in 64-bit mode)
    // -----------------------------------------------------------------------
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

    // -----------------------------------------------------------------------
    // 3. Opcode byte(s)
    // -----------------------------------------------------------------------
    uint8_t op = p[i++];

    // Helper: compute ModRM extension size (disp bytes only; SIB handled too).
    // Returns the total size in bytes of [ModRM] + [SIB] + [disp].
    // Returns (size_t)-1 on truncation.
    auto modRMSize = [&]() -> size_t {
        if (i >= avail)
            return (size_t)-1;
        uint8_t modrm = p[i];
        uint8_t mod   = (modrm >> 6) & 0x3;
        uint8_t rm    = modrm & 0x7;
        size_t  sz    = 1; // ModRM byte itself

        if (mod == 3) {
            // register–register, no disp/SIB
            return sz;
        }

        // SIB present when rm == 4 and mod != 3
        bool hasSIB = (rm == 4);
        if (hasSIB) {
            ++sz; // SIB byte
            if (i + 1 >= avail)
                return (size_t)-1;
            uint8_t sib = p[i + 1];
            uint8_t sibBase = sib & 0x7;
            // disp32 forced when sib.base == 5 and mod == 0
            if (mod == 0 && sibBase == 5)
                sz += 4;
        }

        // Displacement
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

    // -----------------------------------------------------------------------
    // 4. Decode by opcode
    // -----------------------------------------------------------------------

    // --- Two-byte escape (0x0F ...) ---
    if (op == 0x0F) {
        if (i >= avail)
            return 0;
        uint8_t op2 = p[i++];

        // Near Jcc: 0x0F 0x80–0x8F  rel32  (6 bytes total)
        if (op2 >= 0x80 && op2 <= 0x8F)
            return i + 4; // opcode(2) + rel32(4)

        // SSE / scalar-FP ops that appear in stochsuite loops
        // These all have the form  [pfx] 0F <op2> ModRM [SIB] [disp]
        // and no immediate beyond ModRM.
        //   10 MOVUPS/MOVSD/MOVSS   11 MOVUPS/MOVSD/MOVSS
        //   2F COMISD/COMISS        51 SQRTSD/SQRTSS
        //   58 ADDSD/ADDPS          59 MULSD/MULPS
        //   5C SUBSD                5E DIVSD
        //   57 XORPS                EF PXOR
        //   6E MOVD (ModRM only)    7E MOVD (ModRM only)
        //   2A CVTSI2SD             2C CVTTSD2SI
        //   28 MOVAPS               29 MOVAPS
        if (op2 == 0x10 || op2 == 0x11 || op2 == 0x2F ||
            op2 == 0x51 || op2 == 0x58 || op2 == 0x59 ||
            op2 == 0x5C || op2 == 0x5E || op2 == 0x57 ||
            op2 == 0xEF || op2 == 0x6E || op2 == 0x7E ||
            op2 == 0x2A || op2 == 0x2C || op2 == 0x28 ||
            op2 == 0x29 || op2 == 0x5A) {
            size_t ms = modRMSize();
            if (ms == (size_t)-1)
                return 0;
            return i + ms;
        }

        // SETCC: 0x0F 0x90–0x9F  ModRM (no imm)
        if (op2 >= 0x90 && op2 <= 0x9F) {
            size_t ms = modRMSize();
            if (ms == (size_t)-1)
                return 0;
            return i + ms;
        }

        // CMOV: 0x0F 0x40–0x4F  ModRM
        if (op2 >= 0x40 && op2 <= 0x4F) {
            size_t ms = modRMSize();
            if (ms == (size_t)-1)
                return 0;
            return i + ms;
        }

        // Short BTx, MOVZX, MOVSX, etc. — ModRM only, no imm
        if (op2 == 0xB6 || op2 == 0xB7 || op2 == 0xBE || op2 == 0xBF ||
            op2 == 0xA3 || op2 == 0xAB || op2 == 0xB3 || op2 == 0xBB ||
            op2 == 0xBC || op2 == 0xBD || op2 == 0xAF) {
            size_t ms = modRMSize();
            if (ms == (size_t)-1)
                return 0;
            return i + ms;
        }

        // Unrecognised 0F escape — return 0 to abort the scan
        return 0;
    }

    // --- Short Jcc: 0x70–0x7F  rel8  (2 bytes total) ---
    if (op >= 0x70 && op <= 0x7F)
        return i + 1; // opcode(1) + rel8(1)

    // --- JMP short: 0xEB  rel8 ---
    if (op == 0xEB)
        return i + 1;

    // --- JMP near: 0xE9  rel32 ---
    if (op == 0xE9)
        return i + 4;

    // --- CALL near: 0xE8  rel32 ---
    if (op == 0xE8)
        return i + 4;

    // --- RET: 0xC3 / 0xC2 imm16 ---
    if (op == 0xC3)
        return i;
    if (op == 0xC2)
        return i + 2;

    // --- One-byte opcodes with no operands (NOP, ...) ---
    if (op == 0x90 || op == 0x99 || op == 0x9B || op == 0x9C || op == 0x9D)
        return i;

    // --- PUSH/POP reg (0x50–0x5F) ---
    if (op >= 0x50 && op <= 0x5F)
        return i;

    // --- MOV reg, imm64 (0xB8–0xBF + REX.W for 64-bit) ---
    if (op >= 0xB0 && op <= 0xB7)
        return i + 1; // 8-bit imm
    if (op >= 0xB8 && op <= 0xBF)
        return i + (rexW ? 8 : 4);

    // --- ADD/OR/ADC/SBB/AND/SUB/XOR/CMP reg, imm (0x80/0x81/0x83) ---
    if (op == 0x80) { // /r imm8
        size_t ms = modRMSize();
        if (ms == (size_t)-1) return 0;
        return i + ms + 1;
    }
    if (op == 0x81) { // /r imm16/32
        size_t ms = modRMSize();
        if (ms == (size_t)-1) return 0;
        return i + ms + (hasOpSzPfx ? 2 : 4);
    }
    if (op == 0x83) { // /r imm8 (sign-extended)
        size_t ms = modRMSize();
        if (ms == (size_t)-1) return 0;
        return i + ms + 1;
    }

    // --- TEST/XCHG/MOV/LEA/POP with ModRM (0x84–0x8F) ---
    if (op >= 0x84 && op <= 0x8F) {
        size_t ms = modRMSize();
        if (ms == (size_t)-1) return 0;
        return i + ms;
    }

    // --- Common two-operand ops: ADD/OR/ADC/SBB/AND/SUB/XOR/CMP r/m,r or r,r/m ---
    // Opcodes 0x00–0x3F in the "reg/mem op reg" groups (step 8)
    if ((op & 0xC0) == 0x00 && (op & 0x06) != 0x04 && (op & 0x07) != 0x06) {
        // Most opcodes 0x00–0x3B have ModRM
        if ((op & 0x07) <= 0x03) {
            size_t ms = modRMSize();
            if (ms == (size_t)-1) return 0;
            return i + ms;
        }
    }

    // --- MOV r/m, r  or  r, r/m  (0x88–0x8B) already covered above ---

    // --- MOV r/m, imm  (0xC6/0xC7) ---
    if (op == 0xC6) {
        size_t ms = modRMSize();
        if (ms == (size_t)-1) return 0;
        return i + ms + 1;
    }
    if (op == 0xC7) {
        size_t ms = modRMSize();
        if (ms == (size_t)-1) return 0;
        return i + ms + (hasOpSzPfx ? 2 : 4);
    }

    // --- INC/DEC/CALL/JMP/PUSH r/m (0xFF /r) ---
    if (op == 0xFF) {
        size_t ms = modRMSize();
        if (ms == (size_t)-1) return 0;
        return i + ms;
    }

    // --- CMP AL,imm8 (0x3C) / CMP rAX,imm32 (0x3D) ---
    if (op == 0x3C) return i + 1;
    if (op == 0x3D) return i + (hasOpSzPfx ? 2 : 4);

    // --- TEST AL,imm8 (0xA8) / TEST rAX,imm32 (0xA9) ---
    if (op == 0xA8) return i + 1;
    if (op == 0xA9) return i + (hasOpSzPfx ? 2 : 4);

    // --- ADD/SUB/XOR/OR/AND/CMP rAX,imm  (0x05/0x15/…/0x35/0x3D done above) ---
    if (op == 0x05 || op == 0x15 || op == 0x25 || op == 0x35)
        return i + (hasOpSzPfx ? 2 : 4);

    // --- PUSH imm8 (0x6A) / PUSH imm32 (0x68) ---
    if (op == 0x6A) return i + 1;
    if (op == 0x68) return i + (hasOpSzPfx ? 2 : 4);

    // --- IMUL r, r/m, imm8 (0x6B) / imm32 (0x69) ---
    if (op == 0x6B) {
        size_t ms = modRMSize();
        if (ms == (size_t)-1) return 0;
        return i + ms + 1;
    }
    if (op == 0x69) {
        size_t ms = modRMSize();
        if (ms == (size_t)-1) return 0;
        return i + ms + (hasOpSzPfx ? 2 : 4);
    }

    // --- XCHG rAX, rN (0x91–0x97) ---
    if (op >= 0x91 && op <= 0x97) return i;

    // --- MOVSB/MOVSQ/CMPSB/… string ops (0xA4–0xA7, 0xAA–0xAF) ---
    if (op >= 0xA4 && op <= 0xA7) return i;
    if (op >= 0xAA && op <= 0xAF) return i;

    // --- MOV r/m8, r8 (0x88) / MOV r/m, r (0x89) / MOV r, r/m8 (0x8A) /
    //     MOV r, r/m (0x8B) — already covered by 0x84–0x8F block above ---

    // --- Unrecognised — return 0 to stop the scan ---
    return 0;
}

// ---------------------------------------------------------------------------
// findJccOffset: walk x86 instructions from `bytes` (instruction boundary),
// stop at the first Jcc opcode, and return its byte offset from `bytes`.
//
// Uses the minimal decoder above to advance instruction-by-instruction rather
// than scanning byte-by-byte.  This prevents false positives when an opcode-
// range byte (e.g. 0x7E in a RIP-relative displacement) is misidentified as
// a short Jcc.  The scan is capped at kMaxScan bytes.
//
// x86 Jcc encodings:
//   Short (rel8)  : 0x70 – 0x7F             (2 bytes total)
//   Near  (rel32) : 0x0F 0x80 – 0x0F 0x8F   (6 bytes total)
// ---------------------------------------------------------------------------

size_t
StochBranchMonitor::findJccOffset(const uint8_t *bytes, size_t len) const
{
    const size_t kMaxScan = std::min(len, static_cast<size_t>(128));
    size_t i = 0;

    while (i < kMaxScan) {
        // Skip legacy prefixes before opcode test so we don't count them as
        // a separate "instruction".  We need to find the canonical opcode byte
        // to detect Jcc, but we still need the full instruction length to
        // advance correctly.

        // Peek at prefixes to find the actual first opcode byte.
        size_t j = i;
        while (j < kMaxScan) {
            uint8_t b = bytes[j];
            if (b == 0x66 || b == 0x67 || b == 0xF0 || b == 0xF2 ||
                b == 0xF3 || b == 0x2E || b == 0x3E || b == 0x26 ||
                b == 0x36 || b == 0x64 || b == 0x65)
                ++j;
            else if ((b & 0xF0) == 0x40) // REX
                ++j;
            else
                break;
        }
        if (j >= kMaxScan)
            break;

        // Check for Jcc at the opcode boundary.
        uint8_t op = bytes[j];
        if (op >= 0x70 && op <= 0x7F)
            return i; // short Jcc at instruction start
        if (op == 0x0F && j + 1 < kMaxScan) {
            uint8_t op2 = bytes[j + 1];
            if (op2 >= 0x80 && op2 <= 0x8F)
                return i; // near Jcc at instruction start
        }

        // Decode full instruction length and advance.
        size_t insnLen = x86InsnLen(bytes + i, kMaxScan - i);
        if (insnLen == 0) {
            // Unrecognised encoding; abort to avoid infinite loop.
            DPRINTF(StochBranchMonitor,
                    "findJccOffset: unrecognised x86 encoding at offset "
                    "+%zu (byte 0x%02x); aborting scan\n",
                    i, bytes[i]);
            break;
        }
        i += insnLen;
    }

    return len; // not found
}

// ---------------------------------------------------------------------------
// parseElf: mmap the binary, walk the ELF symbol table, find every
// __stoch_br_* symbol, locate the actual Jcc instruction that follows it,
// and create a BranchSymbolStats entry keyed by the Jcc's virtual address.
// ---------------------------------------------------------------------------

void
StochBranchMonitor::parseElf()
{
    // Open and mmap the file.
    int fd = open(binaryPath.c_str(), O_RDONLY);
    fatal_if(fd < 0, "StochBranchMonitor: cannot open binary '%s': %s",
             binaryPath.c_str(), strerror(errno));

    struct stat st;
    fstat(fd, &st);
    size_t fileSize = static_cast<size_t>(st.st_size);

    void *mapping = mmap(nullptr, fileSize, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    fatal_if(mapping == MAP_FAILED,
             "StochBranchMonitor: mmap failed for '%s': %s",
             binaryPath.c_str(), strerror(errno));

    const uint8_t *base = static_cast<const uint8_t *>(mapping);

    // Validate ELF magic.
    const Elf64_Ehdr *ehdr = reinterpret_cast<const Elf64_Ehdr *>(base);
    fatal_if(memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0,
             "StochBranchMonitor: '%s' is not a valid ELF file",
             binaryPath.c_str());
    fatal_if(ehdr->e_ident[EI_CLASS] != ELFCLASS64,
             "StochBranchMonitor: only ELF64 binaries are supported");

    // Locate the section header table.
    const Elf64_Shdr *shdrs =
        reinterpret_cast<const Elf64_Shdr *>(base + ehdr->e_shoff);

    // Section name string table.
    const char *shstrtab = reinterpret_cast<const char *>(
        base + shdrs[ehdr->e_shstrndx].sh_offset);

    // Find .symtab and .strtab sections.
    const Elf64_Shdr *symtabHdr = nullptr;
    const Elf64_Shdr *strtabHdr = nullptr;
    for (int i = 0; i < ehdr->e_shnum; ++i) {
        const char *sname = shstrtab + shdrs[i].sh_name;
        if (shdrs[i].sh_type == SHT_SYMTAB && strcmp(sname, ".symtab") == 0) {
            symtabHdr = &shdrs[i];
            // The associated string table is sh_link.
            strtabHdr = &shdrs[shdrs[i].sh_link];
        }
    }

    if (!symtabHdr) {
        // Fall back to .dynsym if .symtab is absent (stripped binary).
        for (int i = 0; i < ehdr->e_shnum; ++i) {
            if (shdrs[i].sh_type == SHT_DYNSYM) {
                symtabHdr = &shdrs[i];
                strtabHdr = &shdrs[shdrs[i].sh_link];
                break;
            }
        }
    }

    fatal_if(!symtabHdr,
             "StochBranchMonitor: no symbol table found in '%s'. "
             "Rebuild with --keep-symbol=__stoch_br_* or use the .o binary.",
             binaryPath.c_str());

    const Elf64_Sym *syms =
        reinterpret_cast<const Elf64_Sym *>(base + symtabHdr->sh_offset);
    size_t nsyms = symtabHdr->sh_size / sizeof(Elf64_Sym);
    const char *strtab =
        reinterpret_cast<const char *>(base + strtabHdr->sh_offset);

    // Find the executable LOAD segment so we can map VA → file offset.
    // (needed to read binary bytes at a given VA to scan for Jcc)
    const Elf64_Phdr *phdrs =
        reinterpret_cast<const Elf64_Phdr *>(base + ehdr->e_phoff);

    for (size_t si = 0; si < nsyms; ++si) {
        const Elf64_Sym &sym = syms[si];
        if (sym.st_name == 0)
            continue;

        const char *sname = strtab + sym.st_name;
        if (strncmp(sname, "__stoch_br_", 11) != 0)
            continue;

        Addr labelVA = static_cast<Addr>(sym.st_value);

        // Find the LOAD segment that contains labelVA.
        const uint8_t *segBytes = nullptr;
        size_t segBytesLen = 0;
        Addr segVA = 0;
        for (int pi = 0; pi < ehdr->e_phnum; ++pi) {
            const Elf64_Phdr &ph = phdrs[pi];
            if (ph.p_type != PT_LOAD)
                continue;
            if (!(ph.p_flags & PF_X))
                continue; // must be executable
            if (labelVA >= ph.p_vaddr &&
                labelVA < ph.p_vaddr + ph.p_filesz) {
                segVA    = static_cast<Addr>(ph.p_vaddr);
                segBytes = base + ph.p_offset;
                segBytesLen = static_cast<size_t>(ph.p_filesz);
                break;
            }
        }

        if (!segBytes) {
            warn("StochBranchMonitor: symbol '%s' VA %#x not in any "
                 "executable LOAD segment; skipping.", sname, labelVA);
            continue;
        }

        // Byte offset of the label within the segment.
        size_t labelOff = static_cast<size_t>(labelVA - segVA);
        size_t remaining = segBytesLen - labelOff;
        size_t jccOff = findJccOffset(segBytes + labelOff, remaining);

        if (jccOff >= remaining) {
            warn("StochBranchMonitor: could not find a Jcc within 64 bytes "
                 "of symbol '%s' (VA %#x); skipping.", sname, labelVA);
            continue;
        }

        Addr branchPC = labelVA + static_cast<Addr>(jccOff);

        // Sanity-check: the bytes at branchPC must actually be a Jcc opcode.
        // If not, the decoder encountered an unrecognised instruction and
        // stopped early, likely leaving us on the wrong boundary.
        {
            size_t boff = labelOff + jccOff;
            uint8_t op = segBytes[boff];
            // Skip legacy prefixes before opcode
            size_t k = boff;
            while (k < segBytesLen) {
                uint8_t b = segBytes[k];
                if (b == 0x66 || b == 0x67 || b == 0xF0 || b == 0xF2 ||
                    b == 0xF3 || b == 0x2E || b == 0x3E || b == 0x26 ||
                    b == 0x36 || b == 0x64 || b == 0x65 ||
                    (b & 0xF0) == 0x40)
                    ++k;
                else { op = b; break; }
            }
            bool isJcc = (op >= 0x70 && op <= 0x7F) ||
                         (op == 0x0F && k + 1 < segBytesLen &&
                          segBytes[k + 1] >= 0x80 && segBytes[k + 1] <= 0x8F);
            warn_if(!isJcc,
                    "StochBranchMonitor: resolved VA %#x for symbol '%s' "
                    "does not appear to be a Jcc instruction (opcode 0x%02x)."
                    " The label placement or decoder may need adjustment.",
                    branchPC, sname, op);
        }

        DPRINTF(StochBranchMonitor,
                "Symbol %-40s  label VA %#x  Jcc VA %#x  (offset +%zu)\n",
                sname, labelVA, branchPC, jccOff);

        // Each symbol gets its own stats sub-group.
        std::string symName(sname);
        branchStats.emplace(
            branchPC,
            std::make_unique<BranchSymbolStats>(this, symName));
    }

    munmap(mapping, fileSize);

    inform("StochBranchMonitor: found %d stochastic branch site(s) in '%s'",
           (int)branchStats.size(), binaryPath.c_str());
}

} // namespace gem5
