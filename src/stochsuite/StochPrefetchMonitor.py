from m5.params import *
from m5.SimObject import SimObject


class StochPrefetchMonitor(SimObject):
    type = "StochPrefetchMonitor"
    cxx_header = "stochsuite/stoch_prefetch_monitor.hh"
    cxx_class = "gem5::StochPrefetchMonitor"

    binary = Param.String(
        "Path to the workload binary (stripped, retaining __stoch_mem_* symbols)"
    )
    # SimObject* rather than a concrete Cache type so the generated params
    # header gives us a complete gem5::SimObject* — calling getProbeManager()
    # only requires SimObject, not the full Cache class.
    dcache = Param.SimObject(
        NULL, "L1D cache SimObject whose Hit/Miss probes to monitor"
    )
