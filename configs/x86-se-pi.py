"""
This script utilizes the X86DemoBoard to run a simple Ubuntu boot. The script
will boot the the OS to login before exiting the simulation.

A detailed terminal output can be found in `m5out/board.pc.com_1.device`.

**Warning:** The X86DemoBoard uses the Timing CPU. The boot may take
considerable time to complete execution.
`configs/example/gem5_library/x86-ubuntu-run-with-kvm.py` can be referenced as
an example of booting Ubuntu with a KVM CPU.

Usage
-----

```
scons build/ALL/gem5.opt
./build/ALL/gem5.opt configs/x86-se-stochsuite-workloads.py
```
"""

from m5.objects import (
    TAGE,
    LocalBP,
    TournamentBP,
    X86ISA,
)
import argparse

parser = argparse.ArgumentParser(description="Stochsuite GEM5 X86 SE Workload")
parser.add_argument("--hwrng", type=str, default="Taus88", help="Hardware RNG type for RDRAND")
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


board.set_se_binary_workload(
    binary=BinaryResource(
        local_path="/home/pgupta58/work/stochsuite_gem5/stochsuite/apps/pi.o"
    ),
    #        arguments=["-iters", "1000"])
    arguments=["-rng", "HWRNG", "-iters", "1000"],
)

# Initialize the simulator with the handlers
simulator = Simulator(board=board)

simulator.run()
