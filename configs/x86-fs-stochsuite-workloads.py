"""
Full-system gem5 driver for Stochsuite workloads.

Boots Ubuntu 24.04 with a KVM CPU, then switches to the O3 CPU at the
start of the ROI (m5_work_begin_addr), resets stats, runs the benchmark,
dumps stats at the end of the ROI (m5_work_end_addr), and exits cleanly.

Usage
-----
```
scons build/ALL/gem5.opt
./build/ALL/gem5.opt \
    configs/x86-fs-stochsuite-workloads.py \
    --benchmark pi --iters 1000 \
    --disk-image /path/to/stochsuite-base.img
```
"""

import argparse

import m5

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
from gem5.resources.resource import (
    DiskImageResource,
    obtain_resource,
)
from gem5.simulate.exit_event import ExitEvent
from gem5.simulate.simulator import Simulator
from gem5.utils.requires import requires

requires(
    coherence_protocol_required=CoherenceProtocol.MESI_TWO_LEVEL,
    kvm_required=True,
)

WORKLOADS = ["pi", "dop", "dropout", "multinomial", "photon", "tailwag"]
DEFAULT_KERNEL = "x86-linux-kernel-6.8.0-52-generic"

parser = argparse.ArgumentParser(
    description="Full-system simulation of a stochsuite workload."
)
parser.add_argument(
    "--benchmark",
    required=True,
    choices=WORKLOADS,
    help="Stochsuite workload binary to run inside the guest.",
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
    "--disk-image",
    required=True,
    help="Path to the FS disk image containing /home/gem5/stochsuite/*_gem5.",
)
parser.add_argument(
    "--root-partition",
    default="2",
    help=(
        "Disk partition holding the Linux rootfs. The canned "
        "x86-ubuntu-24.04-img is GPT-formatted with a tiny BIOS boot "
        "partition as p1 and the ext4 rootfs as p2, so '2' is the right "
        "default. Set to '1' for legacy MBR-style images."
    ),
)
parser.add_argument(
    "--disk-device",
    default="/dev/sda",
    help=(
        "Guest device node for the disk. X86Board attaches the disk as "
        "an IdeDisk and defaults the kernel cmdline to /dev/hda, but the "
        "kernel 6.x libata stack exposes IDE disks via /dev/sd*, so the "
        "default here is /dev/sda. Combined with --root-partition the "
        "guest kernel sees root=/dev/sda2 by default."
    ),
)
parser.add_argument(
    "--kernel",
    default=DEFAULT_KERNEL,
    help="gem5 resource id (or local path) for the x86 Linux kernel.",
)
args = parser.parse_args()

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

# Boot on KVM for speed, then switch to the Timing CPU at the start
# of the ROI for cycle-accurate stats.
processor = SimpleSwitchableProcessor(
    starting_core_type=CPUTypes.KVM,
    switch_core_type=CPUTypes.TIMING,
    isa=ISA.X86,
    num_cores=1,
)

# KVM CPU's default mode opens a Linux perf_event fd, which fails with
# EACCES on hosts where /proc/sys/kernel/perf_event_paranoid is >= 2 (the
# kernel 4.6+ default). We don't need host perf counters during the KVM
# boot phase because the actual measurement happens on the Timing CPU
# after WORKBEGIN, so disable usePerf on the starting (KVM) cores.
for proc in processor.start:
    proc.core.usePerf = False

# Here we setup the board. The X86Board allows for Full-System X86 simulations

board = X86Board(
    clk_freq="3GHz",
    processor=processor,
    memory=memory,
    cache_hierarchy=cache_hierarchy,
)
# Here we set up the board. The prebuilt X86DemoBoard allows for for FS mode
# (full system) or SE mode (syscall emulation) X86 simulation.

# Under KVM, GNU libc may pick AVX/VEX IFUNC paths for libc helpers (and for
# some libm entry points). Gem5 Timing frequently treats VEX-encoded insns as
# UD → SIGILL. Ask glibc for conservative hwcaps where supported (comma list;
# tunables docs: Hardware Capability Tunables).
_guest_glibc_hwcaps = (
    "export GLIBC_TUNABLES=glibc.cpu.hwcaps="
    "-AVX512F,-AVX512VL,-AVX512DQ,-AVX512BW,-AVX512IFMA,-AVX512VBMI,"
    "-AVX2,-AVX,-FMA;"
)

guest_cmd = _guest_glibc_hwcaps + (
    "echo TEST_START; "
    "sleep 2; "
    f"/home/gem5/stochsuite/{args.benchmark}_gem5 "
    f"-seed {args.seed} -iters {args.iters} -rng {args.rng}; "
    "sleep 2; "
    "echo TEST_END; "
    "sleep 2; "
    "m5 exit"
)

test_cmd = (
    "set -x; "
    "echo TEST_START; "
    "ls -la /home/gem5/stochsuite; "
    "file /home/gem5/stochsuite/pi_gem5 2>&1 || true; "
    "sync; "
    "echo TEST_END; "
    "sleep 1; "
    "m5 exit; "
)

board.set_kernel_disk_workload(
    kernel=obtain_resource(args.kernel),
    disk_image=DiskImageResource(
        local_path=args.disk_image,
        root_partition=args.root_partition,
    ),
    disk_device=args.disk_device,
    readfile_contents=guest_cmd,
    exit_on_work_items=True,
)


# Switch from KVM to O3 at WORKBEGIN, reset stats so the stats dump
# at WORKEND reflects only the ROI.
def workbegin_handler():
    print("WorkBegin: entering ROI, switching to Timing CPU")
    board.get_processor().switch()
    m5.stats.reset()
    yield False


# Dump stats at WORKEND and keep the simulation running so the binary
# can finish its trailing prints and call `m5 exit` cleanly.
def workend_handler():
    m5.stats.dump()
    print("WorkEnd: stats dumped")
    yield False


simulator = Simulator(
    board=board,
    on_exit_event={
        ExitEvent.WORKBEGIN: workbegin_handler(),
        ExitEvent.WORKEND: workend_handler(),
    },
)

print(f"Running stochsuite/{args.benchmark} with iters={args.iters}")
simulator.run()
