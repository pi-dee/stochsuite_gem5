"""
Usage
-----

```
scons build/X86/gem5.opt -j8
./build/X86/gem5.opt configs/x86-se-stochsuite-workloads.py
```
"""

import argparse
import os
from pathlib import Path

from m5.objects import (
    TAGE,
    X86ISA,
    LocalBP,
    TournamentBP,
)

parser = argparse.ArgumentParser(description="Stochsuite GEM5 X86 SE Workload")
parser.add_argument(
    "--stochsuite-home",
    type=str,
    default=os.getenv(
        "STOCHSUITE_HOME", "/home/selagamsetty/private/stochsuite"
    ),
    help="Top level directory for stochsuite project",
)
parser.add_argument(
    "--harden-prng",
    action="store_true",
    dest="use_hwrng",
    help="Flag to enable using gem5 RDRAND during simulation.",
)
parser.add_argument(
    "--prng-select",
    type=str,
    default="Taus88",
    help="""The PRNG to use. When harden-prng is used, this is the genertor that
          implements the backed for when rdrand is called by the application to
          model a hardwre implementation of the PRNG. When hard-prng is unset,
          the PRNG is modelled entirely in software and considered as part of the
          application. Options are: Taus88, Taus113, JKISS, JKISS32, CONG,
          GLIBC_CRAND, DRAND48, MersenneTwister, KISS11, PCGBasic, XorShift32,
          XorShift128, XorWow, XoShiRo128++.""",
)
parser.add_argument(
    "--hwrng-lat",
    type=int,
    default=1,
    help="Latency for RDRAND operations. Unused if harden-prng is not used.",
)
parser.add_argument(
    "--rdseed-lat", type=int, default=1, help="Latency for RDSEED operations"
)
parser.add_argument(
    "--app",
    type=str,
    default="pi.o",
    help="""Application binary to run. Options are: pi.o, dop.o, dropout.o,
          multinomial.o, photon.o, and tailwag.o""",
)
parser.add_argument(
    "--app-iters",
    type=int,
    default=100,
    help="Number of iterations to run on applicaiton.",
)

# Allow passing unknown args to gem5
args, _ = parser.parse_known_args()

# Update the default parameter for all future X86ISA instances in this run
X86ISA.hwrng_type = args.prng_select

from gem5.coherence_protocol import CoherenceProtocol
from gem5.components.boards.x86_board import X86Board
from gem5.components.cachehierarchies.ruby.mesi_two_level_cache_hierarchy import (
    MESITwoLevelCacheHierarchy,
)
from gem5.components.memory import DualChannelDDR4_2400
from gem5.components.processors.cpu_types import CPUTypes
from gem5.components.processors.simple_processor import SimpleProcessor
from gem5.components.processors.simple_switchable_processor import (
    SimpleSwitchableProcessor,
)
from gem5.isas import ISA
from gem5.prebuilt.demo.x86_demo_board import X86DemoBoard
from gem5.resources.resource import (
    BinaryResource,
    DiskImageResource,
    obtain_resource,
)
from gem5.simulate.exit_event import ExitEvent
from gem5.simulate.exit_handler import (
    ExitHandler,
    WorkBeginExitHandler,
    WorkEndExitHandler,
)
from gem5.simulate.simulator import Simulator
from gem5.utils.override import overrides
from gem5.utils.requires import requires

cache_hierarchy = MESITwoLevelCacheHierarchy(
    l1d_size="32KiB",
    l1d_assoc=8,
    l1i_size="32KiB",
    l1i_assoc=8,
    l2_size="256KiB",
    l2_assoc=16,
    num_l2_banks=2,
)
# Memory: Dual Channel DDR4 2400 DRAM device.
# The X86 board only supports 3 GiB of main memory.

memory = DualChannelDDR4_2400(size="3GiB")

# Here we setup the processor. This is a special switchable processor in which
# a starting core type and a switch core type must be specified. Once a
# configuration is instantiated a user may call `processor.switch()` to switch
# from the starting core types to the switch core types. In this simulation
# we start with KVM cores to simulate the OS boot, then switch to the Timing
# cores for the command we wish to run after boot.

# processor = SimpleSwitchableProcessor(
#    starting_core_type=CPUTypes.KVM,
#    switch_core_type=CPUTypes.TIMING,
#    isa=ISA.X86,
#    num_cores=1,
# )
processor = SimpleProcessor(cpu_type=CPUTypes.O3, num_cores=1, isa=ISA.X86)

# Configure the latency of the HWRNG functional unit
print("Configuring HWRNG latency...")
for core in processor.get_cores():
    # In gem5 library, 'core' is a wrapper. 'core.core' is the SimObject.
    c = core.core
    print(f"Checking core: {c.path() if hasattr(c, 'path') else c}")
    if hasattr(c, "instQueues"):
        for iq in c.instQueues:
            if hasattr(iq, "fuPool"):
                for fu in iq.fuPool.FUList:
                    for op in fu.opList:
                        if str(op.opClass) == "RdRand":
                            print(
                                f"Found RdRand OpClass in {fu.path()}. Setting latency to {args.hwrng_lat}"
                            )
                            op.opLat = args.hwrng_lat
                        elif str(op.opClass) == "RdSeed":
                            print(
                                f"Found RdSeed OpClass in {fu.path()}. Setting latency to {args.rdseed_lat}"
                            )
                            op.opLat = args.rdseed_lat

for core in processor.get_cores():
    # Setting the branch predictor on the SimObject
    core.branchPred = LocalBP(
        numThreads=1, instShiftAmt=2, speculativeHistUpdate=False
    )
# core.branchPred = TAGE(numThreads=1, instShiftAmt=2,
#            speculativeHistUpdate=False,
#            takenOnlyHistory=True,
#            )

# Here we setup the board. The X86Board allows for Full-System X86 simulations

board = X86Board(
    clk_freq="3GHz",
    processor=processor,
    memory=memory,
    cache_hierarchy=cache_hierarchy,
)


app_path = Path(args.stochsuite_home).joinpath("apps", args.app)
app_rng_opt = args.prng_select
if args.use_hwrng:
    app_rng_opt = "HWRNG"

board.set_se_binary_workload(
    binary=BinaryResource(local_path=str(app_path)),
    #        arguments=["-iters", "1000"])
    arguments=["-rng", app_rng_opt, "-iters", str(args.app_iters)],
)

# Initialize the simulator with the handlers
simulator = Simulator(board=board)

simulator.run()
