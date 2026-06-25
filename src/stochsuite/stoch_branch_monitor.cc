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
#include "stochsuite/x86_insn_len.hh"

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

// Instruction-length decoding: stochsuite/x86_insn_len.hh (x86InsnLen).

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
