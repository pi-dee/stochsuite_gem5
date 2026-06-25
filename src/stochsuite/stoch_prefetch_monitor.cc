#include "stochsuite/stoch_prefetch_monitor.hh"

#include <elf.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <string>

#include "base/logging.hh"
#include "base/trace.hh"
#include "debug/StochPrefetchMonitor.hh"
#include "sim/probe/probe.hh"
#include "stochsuite/x86_insn_len.hh"

namespace gem5
{

// ---------------------------------------------------------------------------
// LoadSymbolStats
// ---------------------------------------------------------------------------

StochPrefetchMonitor::LoadSymbolStats::LoadSymbolStats(
        statistics::Group *parent, const std::string &symbolName)
    : statistics::Group(parent, symbolName.c_str()),
      demand_hits(this, "demand_hits",
                  "L1D demand hits at this stochastic load site"),
      demand_misses(this, "demand_misses",
                    "L1D demand misses at this stochastic load site"),
      prefetch_hits(this, "prefetch_hits",
                    "Demand hits on a hardware-prefetched line at this site")
{
}

// ---------------------------------------------------------------------------
// StochPrefetchMonitor
// ---------------------------------------------------------------------------

StochPrefetchMonitor::StochPrefetchMonitor(const Params &p)
    : SimObject(p),
      binaryPath(p.binary)
{
}

void
StochPrefetchMonitor::startup()
{
    parseElf();

    if (!params().dcache) {
        warn("StochPrefetchMonitor: dcache not set — probe listeners not "
             "connected, no prefetch statistics will be collected");
        return;
    }

    ProbeManager *pm = params().dcache->getProbeManager();

    warn_if(!pm->getFirstProbePoint("Hit"),
            "StochPrefetchMonitor: 'Hit' probe not found on %s — "
            "check that BaseCache::regProbePoints() added it",
            params().dcache->name());
    warn_if(!pm->getFirstProbePoint("Miss"),
            "StochPrefetchMonitor: 'Miss' probe not found on %s",
            params().dcache->name());

    listeners_.push_back(
        pm->connect<ProbeListenerArg<StochPrefetchMonitor,
                                     CacheAccessProbeArg>>(
            this, "Hit", &StochPrefetchMonitor::notifyHit));

    listeners_.push_back(
        pm->connect<ProbeListenerArg<StochPrefetchMonitor,
                                     CacheAccessProbeArg>>(
            this, "Miss", &StochPrefetchMonitor::notifyMiss));

    inform("StochPrefetchMonitor: connected to %s, tracking %d site(s) "
           "from %s",
           params().dcache->name(), (int)loadStats.size(), binaryPath);

    DPRINTF(StochPrefetchMonitor, "Tracking %d stochastic load site(s) "
            "from %s\n", (int)loadStats.size(), binaryPath);
}

void
StochPrefetchMonitor::notifyHit(const CacheAccessProbeArg &arg)
{
    // Filter to demand accesses only: hardware prefetch packets issued by
    // the prefetcher engine do not carry a valid instruction PC.
    if (!arg.pkt->req->hasPC())
        return;

    Addr pc = arg.pkt->req->getPC();
    DPRINTF(StochPrefetchMonitor, "notifyHit: pc=%#x\n", pc);

    auto it = loadStats.find(pc);
    if (it == loadStats.end())
        return;

    it->second->demand_hits++;

    // Check whether the cache line was brought in by a hardware prefetch.
    if (arg.cache.hasBeenPrefetched(arg.pkt->getAddr(), arg.pkt->isSecure()))
        it->second->prefetch_hits++;
}

void
StochPrefetchMonitor::notifyMiss(const CacheAccessProbeArg &arg)
{
    if (!arg.pkt->req->hasPC())
        return;

    Addr pc = arg.pkt->req->getPC();
    auto it = loadStats.find(pc);
    if (it != loadStats.end())
        it->second->demand_misses++;
}

// ---------------------------------------------------------------------------
// findLoadOffset: walk x86 instructions from `bytes` (instruction boundary),
// stop at the first instruction that reads from memory, and return its byte
// offset from `bytes`.
//
// "Reads from memory" means the instruction has a ModRM byte with mod != 3
// (i.e. a memory operand, not register-register) AND the opcode is a load
// (not a pure store or a LEA address computation).
//
// Uses x86InsnLen() from x86_insn_len.hh to advance instruction-by-
// instruction, preventing false positives from displacement bytes.
// The scan is capped at kMaxScan bytes.
//
// Load opcode sets recognised:
//   One-byte: MOV r,r/m (0x8A/0x8B); ALU r,r/m (0x02–0x03, 0x0A–0x0B,
//             0x12–0x13, 0x1A–0x1B, 0x22–0x23, 0x2A–0x2B, 0x32–0x33,
//             0x3A–0x3B); TEST r/m,r (0x84–0x85)
//   Two-byte: SSE/AVX loads (0F 10, 0F 12, 0F 16, 0F 28, 0F 6E, 0F 6F …);
//             CMOV (0F 40–4F); integer reads (0F 2A, 0F 2C, 0F 2F, 0F 51,
//             0F 58–5F, 0F AF, 0F B6/B7, 0F BE/BF, 0F BC/BD)
//
// Excluded (stores or no-mem-read): LEA (0x8D); MOV r/m,r (0x88/0x89);
//   MOV r/m,imm (0xC6/0xC7); SSE store variants (0F 11, 0F 13, 0F 17,
//   0F 29, 0F 2B, 0F 7E on non-F3, 0F 7F).
// ---------------------------------------------------------------------------

size_t
StochPrefetchMonitor::findLoadOffset(const uint8_t *bytes, size_t len) const
{
    const size_t kMaxScan = std::min(len, static_cast<size_t>(128));
    size_t i = 0;

    while (i < kMaxScan) {
        // Peek through prefixes and REX to find the canonical opcode.
        size_t j = i;

        // Legacy prefixes
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

        uint8_t op = bytes[j++];
        bool isLoad = false;

        if (op == 0x0F) {
            if (j >= kMaxScan)
                break;
            uint8_t op2 = bytes[j]; // don't advance — just peek
            // ModRM is at j+1
            if (j + 1 < kMaxScan) {
                uint8_t modrm = bytes[j + 1];
                uint8_t mod   = (modrm >> 6) & 0x3;
                if (mod != 3) {
                    // Load-typed 0F opcodes (reads memory, not stores)
                    isLoad =
                        op2 == 0x10 || // MOVUPS/MOVSD/MOVSS load
                        op2 == 0x12 || // MOVLPS/MOVHLPS
                        op2 == 0x16 || // MOVHPS/MOVLHPS
                        op2 == 0x28 || // MOVAPS/MOVAPD load
                        op2 == 0x2A || // CVTPI2PS / CVTSI2SD
                        op2 == 0x2C || // CVTTPS2PI / CVTTSD2SI
                        op2 == 0x2D || // CVTPS2PI / CVTSD2SI
                        op2 == 0x2E || // UCOMISS/UCOMISD
                        op2 == 0x2F || // COMISS/COMISD
                        op2 == 0x51 || // SQRTPS/SS/PD/SD
                        op2 == 0x52 || // RSQRTPS/SS
                        op2 == 0x53 || // RCPPS/SS
                        op2 == 0x54 || // ANDPS/PD
                        op2 == 0x55 || // ANDNPS/PD
                        op2 == 0x56 || // ORPS/PD
                        op2 == 0x57 || // XORPS/PD
                        op2 == 0x58 || // ADDPS/SS/PD/SD
                        op2 == 0x59 || // MULPS/SS/PD/SD
                        op2 == 0x5A || // CVTPS2PD / CVTPD2PS / CVTSS2SD / CVTSD2SS
                        op2 == 0x5B || // CVTDQ2PS / CVTPS2DQ / CVTTPS2DQ
                        op2 == 0x5C || // SUBPS/SS/PD/SD
                        op2 == 0x5D || // MINPS/SS/PD/SD
                        op2 == 0x5E || // DIVPS/SS/PD/SD
                        op2 == 0x5F || // MAXPS/SS/PD/SD
                        op2 == 0x60 || op2 == 0x61 || op2 == 0x62 || // PUNPCKLBW/WD/DQ
                        op2 == 0x63 || op2 == 0x64 || op2 == 0x65 || // PACKSSWB, PCMPGTB/W
                        op2 == 0x66 || op2 == 0x67 ||                 // PCMPGTD, PACKUSWB
                        op2 == 0x68 || op2 == 0x69 || op2 == 0x6A || // PUNPCKHBW/WD/DQ
                        op2 == 0x6B || op2 == 0x6C || op2 == 0x6D || // PACKSSDW, PUNPCKLQDQ/HQDQ
                        op2 == 0x6E || // MOVD xmm, r/m
                        op2 == 0x6F || // MOVDQA/MOVDQU load (F3 0F 6F or 66 0F 6F)
                        // 0x7E: MOVD/MOVQ store variant — only MOVQ xmm,xmm/m is
                        //       a load when using F3 prefix (MOVQ xmm,r/m64); we
                        //       include it since F3 0F 7E reads memory.
                        op2 == 0x7E ||
                        // SSE integer arithmetic (all read from memory when mod!=3)
                        op2 == 0xD0 || op2 == 0xD1 || op2 == 0xD2 || op2 == 0xD3 ||
                        op2 == 0xD4 || op2 == 0xD5 ||                 op2 == 0xD7 ||
                        op2 == 0xD8 || op2 == 0xD9 || op2 == 0xDA || op2 == 0xDB ||
                        op2 == 0xDC || op2 == 0xDD || op2 == 0xDE || op2 == 0xDF ||
                        op2 == 0xE0 || op2 == 0xE1 || op2 == 0xE2 || op2 == 0xE3 ||
                        op2 == 0xE4 || op2 == 0xE5 ||                 op2 == 0xE7 ||
                        op2 == 0xE8 || op2 == 0xE9 || op2 == 0xEA || op2 == 0xEB ||
                        op2 == 0xEC || op2 == 0xED || op2 == 0xEE || op2 == 0xEF ||
                        op2 == 0xF1 || op2 == 0xF2 || op2 == 0xF3 ||
                        op2 == 0xF4 || op2 == 0xF5 || op2 == 0xF6 ||
                        op2 == 0xF8 || op2 == 0xF9 || op2 == 0xFA || op2 == 0xFB ||
                        op2 == 0xFC || op2 == 0xFD || op2 == 0xFE ||
                        // Scalar integer reads
                        op2 == 0xAF || // IMUL r, r/m
                        op2 == 0xB6 || // MOVZX r, r/m8
                        op2 == 0xB7 || // MOVZX r, r/m16
                        op2 == 0xBC || // BSF r, r/m
                        op2 == 0xBD || // BSR r, r/m
                        op2 == 0xBE || // MOVSX r, r/m8
                        op2 == 0xBF || // MOVSX r, r/m16
                        // CMOV (conditional move from memory)
                        (op2 >= 0x40 && op2 <= 0x4F);
                }
            }
        } else {
            // One-byte opcodes: only reads-from-memory variants
            if (j < kMaxScan) {
                uint8_t modrm = bytes[j];
                uint8_t mod   = (modrm >> 6) & 0x3;
                if (mod != 3) {
                    isLoad =
                        op == 0x8A || // MOV r8, r/m8
                        op == 0x8B || // MOV r, r/m  (most common load)
                        // TEST r/m, r — reads memory
                        op == 0x84 || op == 0x85 ||
                        // ALU r, r/m (reg ← reg op mem):
                        op == 0x02 || op == 0x03 || // ADD r,r/m
                        op == 0x0A || op == 0x0B || // OR  r,r/m
                        op == 0x12 || op == 0x13 || // ADC r,r/m
                        op == 0x1A || op == 0x1B || // SBB r,r/m
                        op == 0x22 || op == 0x23 || // AND r,r/m
                        op == 0x2A || op == 0x2B || // SUB r,r/m
                        op == 0x32 || op == 0x33 || // XOR r,r/m
                        op == 0x3A || op == 0x3B;   // CMP r,r/m
                    // Deliberately excluded:
                    //   0x88/0x89 MOV r/m, r  (store)
                    //   0x8C       MOV r/m, Sreg (store)
                    //   0x8D       LEA r, m  (address calc, no memory read)
                    //   0x8E       MOV Sreg, r/m (segment reg load, not data)
                    //   0xC6/0xC7  MOV r/m, imm (store)
                }
            }
        }

        if (isLoad) {
            DPRINTF(StochPrefetchMonitor,
                    "findLoadOffset: found load at offset +%zu "
                    "(op 0x%02x)\n", i, op);
            return i;
        }

        size_t insnLen = x86InsnLen(bytes + i, kMaxScan - i);
        if (insnLen == 0) {
            DPRINTF(StochPrefetchMonitor,
                    "findLoadOffset: unrecognised x86 encoding at offset "
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
// __stoch_mem_* symbol, locate the actual load instruction that follows it,
// and create a LoadSymbolStats entry keyed by the load's virtual address.
// (Closely mirrors StochBranchMonitor::parseElf.)
// ---------------------------------------------------------------------------

void
StochPrefetchMonitor::parseElf()
{
    int fd = open(binaryPath.c_str(), O_RDONLY);
    fatal_if(fd < 0, "StochPrefetchMonitor: cannot open binary '%s': %s",
             binaryPath.c_str(), strerror(errno));

    struct stat st;
    fstat(fd, &st);
    size_t fileSize = static_cast<size_t>(st.st_size);

    void *mapping = mmap(nullptr, fileSize, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    fatal_if(mapping == MAP_FAILED,
             "StochPrefetchMonitor: mmap failed for '%s': %s",
             binaryPath.c_str(), strerror(errno));

    const uint8_t *base = static_cast<const uint8_t *>(mapping);

    const Elf64_Ehdr *ehdr = reinterpret_cast<const Elf64_Ehdr *>(base);
    fatal_if(memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0,
             "StochPrefetchMonitor: '%s' is not a valid ELF file",
             binaryPath.c_str());
    fatal_if(ehdr->e_ident[EI_CLASS] != ELFCLASS64,
             "StochPrefetchMonitor: only ELF64 binaries are supported");

    const Elf64_Shdr *shdrs =
        reinterpret_cast<const Elf64_Shdr *>(base + ehdr->e_shoff);
    const char *shstrtab = reinterpret_cast<const char *>(
        base + shdrs[ehdr->e_shstrndx].sh_offset);

    const Elf64_Shdr *symtabHdr = nullptr;
    const Elf64_Shdr *strtabHdr = nullptr;
    for (int i = 0; i < ehdr->e_shnum; ++i) {
        const char *sname = shstrtab + shdrs[i].sh_name;
        if (shdrs[i].sh_type == SHT_SYMTAB && strcmp(sname, ".symtab") == 0) {
            symtabHdr = &shdrs[i];
            strtabHdr = &shdrs[shdrs[i].sh_link];
        }
    }

    if (!symtabHdr) {
        for (int i = 0; i < ehdr->e_shnum; ++i) {
            if (shdrs[i].sh_type == SHT_DYNSYM) {
                symtabHdr = &shdrs[i];
                strtabHdr = &shdrs[shdrs[i].sh_link];
                break;
            }
        }
    }

    fatal_if(!symtabHdr,
             "StochPrefetchMonitor: no symbol table found in '%s'. "
             "Rebuild with --keep-symbol=__stoch_mem_* or use the .o binary.",
             binaryPath.c_str());

    const Elf64_Sym *syms =
        reinterpret_cast<const Elf64_Sym *>(base + symtabHdr->sh_offset);
    size_t nsyms = symtabHdr->sh_size / sizeof(Elf64_Sym);
    const char *strtab =
        reinterpret_cast<const char *>(base + strtabHdr->sh_offset);

    const Elf64_Phdr *phdrs =
        reinterpret_cast<const Elf64_Phdr *>(base + ehdr->e_phoff);

    for (size_t si = 0; si < nsyms; ++si) {
        const Elf64_Sym &sym = syms[si];
        if (sym.st_name == 0)
            continue;

        const char *sname = strtab + sym.st_name;
        if (strncmp(sname, "__stoch_mem_", 12) != 0)
            continue;

        Addr labelVA = static_cast<Addr>(sym.st_value);

        const uint8_t *segBytes = nullptr;
        size_t segBytesLen = 0;
        Addr segVA = 0;
        for (int pi = 0; pi < ehdr->e_phnum; ++pi) {
            const Elf64_Phdr &ph = phdrs[pi];
            if (ph.p_type != PT_LOAD)
                continue;
            if (!(ph.p_flags & PF_X))
                continue;
            if (labelVA >= ph.p_vaddr &&
                labelVA < ph.p_vaddr + ph.p_filesz) {
                segVA       = static_cast<Addr>(ph.p_vaddr);
                segBytes    = base + ph.p_offset;
                segBytesLen = static_cast<size_t>(ph.p_filesz);
                break;
            }
        }

        if (!segBytes) {
            warn("StochPrefetchMonitor: symbol '%s' VA %#x not in any "
                 "executable LOAD segment; skipping.", sname, labelVA);
            continue;
        }

        size_t labelOff  = static_cast<size_t>(labelVA - segVA);
        size_t remaining = segBytesLen - labelOff;
        size_t loadOff   = findLoadOffset(segBytes + labelOff, remaining);

        if (loadOff >= remaining) {
            warn("StochPrefetchMonitor: could not find a load instruction "
                 "within 128 bytes of symbol '%s' (VA %#x); skipping.",
                 sname, labelVA);
            continue;
        }

        Addr loadPC = labelVA + static_cast<Addr>(loadOff);

        DPRINTF(StochPrefetchMonitor,
                "Symbol %-40s  label VA %#x  load VA %#x  (offset +%zu)\n",
                sname, labelVA, loadPC, loadOff);

        std::string symName(sname);
        loadStats.emplace(
            loadPC,
            std::make_unique<LoadSymbolStats>(this, symName));
    }

    munmap(mapping, fileSize);

    inform("StochPrefetchMonitor: found %d stochastic load site(s) in '%s'",
           (int)loadStats.size(), binaryPath.c_str());
}

} // namespace gem5
