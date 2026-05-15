#ifndef __STOCHSUITE_STOCH_BRANCH_MONITOR_HH__
#define __STOCHSUITE_STOCH_BRANCH_MONITOR_HH__

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/statistics.hh"
#include "base/types.hh"
#include "params/StochBranchMonitor.hh"
#include "sim/probe/probe.hh"
#include "sim/sim_object.hh"

namespace gem5
{

/**
 * StochBranchMonitor — tracks branch prediction statistics (lookups and
 * mispredictions) for the stochastic branch sites marked with inline
 * assembly labels (__stoch_br_*) in the stochsuite workloads.
 *
 * At startup() the binary specified by the `binary` parameter is parsed to
 * locate every __stoch_br_* ELF symbol and, from there, the first
 * conditional branch (Jcc) instruction that follows in the text segment.
 * Those addresses are loaded into a hash map keyed by virtual address.
 *
 * During simulation the object listens to the BranchPredictor's "BranchPC"
 * and "MissPC" ProbePointArg<Addr> probe points (added alongside the
 * existing PMU probes).  Every hit against the map increments the
 * corresponding per-symbol gem5 statistics, which appear in stats.txt
 * under the StochBranchMonitor group.
 *
 * Inherits SimObject (NOT ProbeListenerObject) to avoid a cycle:
 * ProbeListenerObject uses manager=Parent.any which would resolve back to
 * BranchPredictor while it is still being instantiated.  Instead, this
 * object lives as a sibling of BranchPredictor (direct child of the O3 CPU)
 * and connects its probe listeners manually in startup() via the bpred param.
 */
class StochBranchMonitor : public SimObject
{
  public:
    PARAMS(StochBranchMonitor);

    explicit StochBranchMonitor(const Params &p);

    /**
     * Parse the binary, then connect listeners to the branch predictor's
     * "BranchPC" and "MissPC" probe points.
     */
    void startup() override;

    /** Called on every branch prediction; pc is the branch instruction VA. */
    void notifyBranchPC(const Addr &pc);

    /** Called on every committed mispredict; pc is the branch instruction VA. */
    void notifyMissBranchPC(const Addr &pc);

  private:
    /** Path to the stripped workload binary retaining __stoch_br_* symbols. */
    const std::string binaryPath;

    /**
     * Scan `len` bytes starting at `bytes` for the first x86 conditional
     * branch (Jcc) opcode.  Returns the byte offset of that instruction, or
     * `len` if none was found within the search window.
     */
    size_t findJccOffset(const uint8_t *bytes, size_t len) const;

    /**
     * Read the ELF symbol table of `binaryPath`, identify every symbol whose
     * name begins with "__stoch_br_", resolve the address of the first Jcc
     * that follows (using findJccOffset()), and populate `branchStats`.
     */
    void parseElf();

    /** Per-stochastic-branch statistics, named after the ELF symbol. */
    struct BranchSymbolStats : public statistics::Group
    {
        BranchSymbolStats(statistics::Group *parent,
                          const std::string &symbolName);

        statistics::Scalar predictions;
        statistics::Scalar mispredictions;
    };

    /** Map from branch-instruction VA → per-symbol stats. */
    std::unordered_map<Addr, std::unique_ptr<BranchSymbolStats>> branchStats;

    /**
     * Live probe listeners — held here so they are automatically deregistered
     * when this object is destroyed (same pattern as ProbeListenerObject).
     */
    std::vector<ProbeListenerPtr<>> listeners_;
};

} // namespace gem5

#endif // __STOCHSUITE_STOCH_BRANCH_MONITOR_HH__
