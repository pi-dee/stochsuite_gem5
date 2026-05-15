"""
Syscall-emulation gem5 driver for Stochsuite workloads.

Runs a stochsuite SE binary on an O3 CPU with a StochBranchMonitor attached
to the branch predictor so per-stochastic-branch predict/mispredict statistics
appear in stats.txt.

Usage
-----
```
scons build/ALL/gem5.opt
./build/ALL/gem5.opt \
    configs/x86-se-pi.py \
    --benchmark pi --iters 1000 --rng HWRNG
```
"""

import argparse
import os

from m5.objects import (
    TAGE,
    X86ISA,
    BranchPredictor,
    LocalBP,
    TournamentBP,
)
from m5.objects.StochBranchMonitor import StochBranchMonitor

WORKLOADS = ["pi", "dop", "dropout", "multinomial", "photon", "tailwag"]

parser = argparse.ArgumentParser(
    description="Syscall-emulation simulation of a stochsuite workload."
)
parser.add_argument(
    "--benchmark",
    required=True,
    choices=WORKLOADS,
    help="Stochsuite workload binary to run (apps/<name>_se).",
)
parser.add_argument(
    "--iters",
    type=int,
    default=100000,
    help="Number of iterations passed to the workload's `-iters` flag.",
)
parser.add_argument(
    "--rng",
    default="Taus88",
    help="RNG class name passed to the workload's `-rng` flag.",
)
parser.add_argument(
    "--seed",
    type=int,
    default=12312332,
    help="Seed passed to the workload's `-seed` flag.",
)
parser.add_argument(
    "--hwrng", type=str, default="Taus88", help="Hardware RNG type for RDRAND"
)
parser.add_argument(
    "--hwrng-lat", type=int, default=1, help="Latency for RDRAND operations"
)
parser.add_argument(
    "--rdseed-lat", type=int, default=1, help="Latency for RDSEED operations"
)
# Allow passing unknown args to gem5
args, _ = parser.parse_known_args()

# Update the default parameter for all future X86ISA instances in this run
X86ISA.hwrng_type = args.hwrng

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

_stochsuite_home = os.environ.get(
    "STOCHSUITE_HOME",
    os.path.join(os.path.dirname(__file__), "../stochsuite"),
)
_se_binary = os.path.join(
    _stochsuite_home, "apps", f"{args.benchmark}_se"
)
_stripped_binary = os.path.join(
    _stochsuite_home, "apps", f"{args.benchmark}_se.stripped"
)

for core in processor.get_cores():
    # O3 branchPred must be BranchPredictor (BPredUnit), not a raw ConditionalPredictor.
    bp = BranchPredictor(
        conditionalBranchPred=LocalBP(numThreads=1),
        instShiftAmt=0,
        speculativeHistUpdate=False,
    )
    # StochBranchMonitor must be a sibling of branchPred on the CPU SimObject
    # (not a probe_listeners child of the BP) to avoid a config-hierarchy cycle.
    core.core.branchPred = bp
    core.core.stoch_branch_monitor = StochBranchMonitor(
        binary=_stripped_binary, bpred=bp
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


board.set_se_binary_workload(
    binary=BinaryResource(local_path=_se_binary),
    arguments=[
        "-seed",
        str(args.seed),
        "-iters",
        str(args.iters),
        "-rng",
        args.rng,
    ],
)

simulator = Simulator(board=board)

print(
    f"Running stochsuite/{args.benchmark}_se with "
    f"iters={args.iters} rng={args.rng} seed={args.seed}"
)
simulator.run()
