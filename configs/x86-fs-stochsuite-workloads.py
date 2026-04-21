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

from m5.objects import TournamentBP

from gem5.coherence_protocol import CoherenceProtocol
from gem5.components.boards.x86_board import X86Board
from gem5.components.cachehierarchies.ruby.mesi_two_level_cache_hierarchy import (
    MESITwoLevelCacheHierarchy,
)
from gem5.components.memory import DualChannelDDR4_2400
from gem5.components.processors.cpu_types import CPUTypes
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

processor = SimpleSwitchableProcessor(
    starting_core_type=CPUTypes.KVM,
    switch_core_type=CPUTypes.TIMING,
    isa=ISA.X86,
    num_cores=1,
)

# Here we setup the board. The X86Board allows for Full-System X86 simulations

board = X86Board(
    clk_freq="3GHz",
    processor=processor,
    memory=memory,
    cache_hierarchy=cache_hierarchy,
)
# Here we set up the board. The prebuilt X86DemoBoard allows for for FS mode
# (full system) or SE mode (syscall emulation) X86 simulation.

# board = X86DemoBoard()

# for core in board.get_processor().get_cores():
#    core.core.branchPred = TournamentBP()

# board.set_se_binary_workload(BinaryResource(local_path="/home/selagamsetty/private/stochsuite/apps/pi.o"))
board.set_kernel_disk_workload(
    kernel=obtain_resource("x86-linux-kernel-6.8.0-52-generic"),
    disk_image=DiskImageResource(
        local_path="/home/selagamsetty/private/stochsuite/gem5_sim/gem5/x86-ubuntu-24.04-pi_gem5-img"
    ),  #  "x86-ubuntu-24.04-img"),
    readfile_contents="/home/gem5/pi_gem5.o; m5 exit",  # Run binary then exit,
    exit_on_work_items=True,  # Ensure work items trigger an exit event
)


class CustomKernelBootedExitHandler(ExitHandler, hypercall_num=1):
    @overrides(ExitHandler)
    def _process(self, simulator: "Simulator") -> None:
        print("First exit: kernel booted")

    @overrides(ExitHandler)
    def _exit_simulation(self) -> bool:
        return False


class CustomAfterBootExitHandler(ExitHandler, hypercall_num=2):
    @overrides(ExitHandler)
    def _process(self, simulator: "Simulator") -> None:
        simulator.switch_processor()

    @overrides(ExitHandler)
    def _exit_simulation(self) -> bool:
        return False


class AfterBootScriptExitHandler(ExitHandler, hypercall_num=3):
    @overrides(ExitHandler)
    def _process(self, simulator: "Simulator") -> None:
        print(f"Third exit: {self.get_handler_description()}")

    @overrides(ExitHandler)
    def _exit_simulation(self) -> bool:
        return True


# Define what to do for each event
def handle_work_begin():
    print("Work Begin reached! Resetting stats...")
    processor.switch()
    m5.stats.reset()
    yield False  # Yield False to continue simulation after the event


def handle_work_end():
    print("Work End reached! Dumping stats...")
    m5.stats.dump()
    yield False  # Continue to finish the application


def exit_event_handler():
    print("First exit: kernel booted")
    yield False  # gem5 is now executing systemd startup
    print("Second exit: Started `after_boot.sh` script")
    # The after_boot.sh script is executed after the kernel and systemd have
    # booted.
    # Here we switch the CPU type to Timing.
    print("Switching to Timing CPU")
    processor.switch()
    yield False  # gem5 is now executing the `after_boot.sh` script
    print("Third exit: Finished `after_boot.sh` script")
    # The after_boot.sh script will run a script if it is passed via
    # readfile_contents. This is the last exit event before the simulation exits.
    yield True


# Initialize the simulator with the handlers
simulator = Simulator(
    board=board,
    on_exit_event={
        ExitEvent.WORKBEGIN: handle_work_begin(),
        ExitEvent.WORKEND: handle_work_end(),
        ExitEvent.EXIT: exit_event_handler(),
    },
)

simulator.run()
