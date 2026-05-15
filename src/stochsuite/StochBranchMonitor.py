from m5.params import *
from m5.SimObject import SimObject


class StochBranchMonitor(SimObject):
    type = "StochBranchMonitor"
    cxx_header = "stochsuite/stoch_branch_monitor.hh"
    cxx_class = "gem5::StochBranchMonitor"

    binary = Param.String(
        "Path to the workload binary (stripped, retaining __stoch_br_* symbols)"
    )
    # SimObject (rather than BranchPredictor) so the generated params header
    # gives us a complete gem5::SimObject* — calling getProbeManager() on a
    # forward-declared BPredUnit* would require including bpred_unit.hh.
    bpred = Param.SimObject(NULL, "Branch predictor SimObject whose probes to monitor")
