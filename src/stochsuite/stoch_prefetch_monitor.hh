#ifndef __STOCHSUITE_STOCH_PREFETCH_MONITOR_HH__
#define __STOCHSUITE_STOCH_PREFETCH_MONITOR_HH__

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/statistics.hh"
#include "base/types.hh"
#include "mem/cache/cache_probe_arg.hh"
#include "params/StochPrefetchMonitor.hh"
#include "sim/probe/probe.hh"
#include "sim/sim_object.hh"

namespace gem5
{

/**
 * StochPrefetchMonitor — tracks per-site L1D cache hit/miss statistics for
 * stochastic memory access sites marked with inline assembly labels
 * (__stoch_mem_*) in the stochsuite workloads.
 *
 * At startup() the binary specified by the `binary` parameter is parsed to
 * locate every __stoch_mem_* ELF symbol and, from there, the first
 * memory-read instruction (load) that follows in the text segment.  Those
 * addresses are stored in a hash map keyed by instruction virtual address.
 *
 * During simulation the object listens to the L1D cache's "Hit" and "Miss"
 * ProbePointArg<CacheAccessProbeArg> probe points.  Each firing is filtered
 * to demand accesses (requests that carry a valid instruction PC) and looked
 * up in the map.  On a hit the demand_hits counter is incremented; if the
 * cache line was brought in by a hardware prefetch, prefetch_hits is also
 * incremented.  On a miss demand_misses is incremented.
 *
 * Inherits SimObject (NOT ProbeListenerObject) for the same reason as
 * StochBranchMonitor: manual probe attachment via the dcache param avoids
 * initialisation-order cycles.
 */
class StochPrefetchMonitor : public SimObject
{
  public:
    PARAMS(StochPrefetchMonitor);

    explicit StochPrefetchMonitor(const Params &p);

    /**
     * Parse the binary, then connect listeners to the L1D cache's "Hit" and
     * "Miss" probe points.
     */
    void startup() override;

    /** Called on every L1D cache hit; arg carries the triggering packet. */
    void notifyHit(const CacheAccessProbeArg &arg);

    /** Called on every L1D cache miss; arg carries the triggering packet. */
    void notifyMiss(const CacheAccessProbeArg &arg);

  private:
    /** Path to the stripped workload binary retaining __stoch_mem_* symbols. */
    const std::string binaryPath;

    /**
     * Scan `len` bytes starting at `bytes` for the first x86 instruction
     * that reads from memory (i.e. has a ModRM byte with mod != 3 on a
     * load-type opcode).  Returns the byte offset of that instruction, or
     * `len` if none was found within the search window.
     */
    size_t findLoadOffset(const uint8_t *bytes, size_t len) const;

    /**
     * Read the ELF symbol table of `binaryPath`, identify every symbol whose
     * name begins with "__stoch_mem_", resolve the address of the first load
     * instruction that follows (using findLoadOffset()), and populate
     * `loadStats`.
     */
    void parseElf();

    /** Per-stochastic-load statistics, named after the ELF symbol. */
    struct LoadSymbolStats : public statistics::Group
    {
        LoadSymbolStats(statistics::Group *parent,
                        const std::string &symbolName);

        statistics::Scalar demand_hits;
        statistics::Scalar demand_misses;
        statistics::Scalar prefetch_hits;
    };

    /** Map from load-instruction VA → per-symbol stats. */
    std::unordered_map<Addr, std::unique_ptr<LoadSymbolStats>> loadStats;

    /**
     * Live probe listeners — held here so they are automatically deregistered
     * when this object is destroyed (same pattern as StochBranchMonitor).
     */
    std::vector<ProbeListenerPtr<>> listeners_;
};

} // namespace gem5

#endif // __STOCHSUITE_STOCH_PREFETCH_MONITOR_HH__
