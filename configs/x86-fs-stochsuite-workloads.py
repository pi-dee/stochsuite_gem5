"""
Full-system gem5 driver for Stochsuite workloads.

Boots Ubuntu 24.04 with a KVM CPU, switches to O3 at hypercall 2 (before
the benchmark), resets stats in the HC2 handler (avoids sim_quantum delay on
m5_work_begin), runs the benchmark on O3, dumps stats at m5_work_end_addr,
and exits cleanly. m5_work_begin_addr is a log marker only.

A StochBranchMonitor is attached to the O3 branch predictor so that
per-stochastic-branch predict/mispredict statistics appear in stats.txt
under the branch predictor group (probe_listeners0.<symbol>).

Usage
-----
```
scons build/ALL/gem5.opt
./build/ALL/gem5.opt \
    configs/x86-fs-stochsuite-workloads.py \
    --benchmark pi --iters 1000 --harden-prng --prng-select Taus88 \
    --disk-image /path/to/stochsuite-base.img

./build/ALL/gem5.opt \
    configs/x86-fs-stochsuite-workloads.py --list-bp-types

# Classic caches + StridePrefetcher on L1D (build/X86/gem5.opt is enough):
./build/ALL/gem5.opt \
    configs/x86-fs-stochsuite-workloads.py \
    --mem-system classic --l1d-hwp-type StridePrefetcher \
    --benchmark pi --iters 1000 \
    --disk-image /path/to/stochsuite-base.img
```
"""

import argparse
import os
import sys

import m5
import m5.objects

from gem5.coherence_protocol import CoherenceProtocol
from gem5.components.boards.x86_board import X86Board
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
from gem5.simulate.exit_handler import ExitHandler
from gem5.simulate.simulator import Simulator
from gem5.utils.override import overrides
from gem5.utils.requires import requires
from common.ObjectList import ObjectList
from m5.objects import BranchPredictor, NULL, X86ISA
from m5.objects.StochBranchMonitor import StochBranchMonitor

conditional_bp_list = ObjectList(
    getattr(m5.objects, "ConditionalPredictor", None)
)
hwp_list = ObjectList(getattr(m5.objects, "BasePrefetcher", None))
ruby_prefetcher_list = ObjectList(
    getattr(m5.objects, "RubyPrefetcher", None)
)

PREFETCHER_NONE = "none"
HWP_NONE = "none"
# "Prefetcher" is a deprecated alias for RubyPrefetcher.
_RUBY_PREFETCHER_TYPES = [
    n for n in ruby_prefetcher_list.get_names() if n != "Prefetcher"
]
_HWP_CHOICES = [HWP_NONE] + hwp_list.get_names()

_L1D_SIZE = "32KiB"
_L1I_SIZE = "32KiB"
_L2_SIZE = "256KiB"
_L1_ASSOC = 8
_L2_ASSOC = 16
_NUM_L2_BANKS = 2


class ListBp(argparse.Action):
    def __call__(self, parser, namespace, values, option_string=None):
        conditional_bp_list.print()
        sys.exit(0)


class ListHwp(argparse.Action):
    def __call__(self, parser, namespace, values, option_string=None):
        print("Available classic hardware prefetcher types (--mem-system classic):")
        hwp_list.print()
        print(f"\t{HWP_NONE} (disable prefetch on that cache)")
        sys.exit(0)


class ListRubyPrefetcher(argparse.Action):
    def __call__(self, parser, namespace, values, option_string=None):
        print("Available Ruby L1 prefetcher types (--mem-system ruby):")
        for name in _RUBY_PREFETCHER_TYPES:
            print(f"\t{name}")
        print(
            f"\nUse --prefetcher-type {_RUBY_PREFETCHER_TYPES[0]} to enable, "
            f"or --prefetcher-type {PREFETCHER_NONE} (default) to disable."
        )
        sys.exit(0)


WORKLOADS = ["pi", "dop", "dropout", "multinomial", "photon", "tailwag"]
DEFAULT_KERNEL = "x86-linux-kernel-6.8.0-52-generic"

parser = argparse.ArgumentParser(
    description="Full-system simulation of a stochsuite workload."
)
parser.add_argument(
    "--mem-system",
    choices=("ruby", "classic"),
    default="ruby",
    help=(
        "Memory/cache model: 'ruby' (MESI two-level, default) or 'classic' "
        "(private L1, shared L2, MMU walk caches; supports --l1d-hwp-type)."
    ),
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
    "--harden-prng",
    action="store_true",
    dest="use_hwrng",
    help="Enable gem5 RDRAND: app uses HWRNG; gem5 implements it via --prng-select.",
)
parser.add_argument(
    "--prng-select",
    type=str,
    default="Taus88",
    help=(
        "PRNG to use. With --harden-prng, selects the gem5 backend for RDRAND "
        "when the app calls HWRNG. Without --harden-prng, passed to the app as "
        "-rng (software PRNG). Options include: Taus88, Taus113, JKISS, "
        "JKISS32, CONG, GLIBC_CRAND, DRAND48, MersenneTwister, KISS11, "
        "PCGBasic, XorShift32, XorShift128, XorWow, XoShiRo128++."
    ),
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
parser.add_argument(
    "--list-bp-types",
    action=ListBp,
    nargs=0,
    help="List available conditional branch predictor types and exit.",
)
parser.add_argument(
    "--bp-type",
    default="LocalBP",
    choices=conditional_bp_list.get_names(),
    help=(
        "Conditional branch predictor for the O3 CPU (wrapped in "
        "BranchPredictor). Default: LocalBP."
    ),
)
parser.add_argument(
    "--list-hwp-types",
    action=ListHwp,
    nargs=0,
    help="List classic hardware prefetcher types (--mem-system classic) and exit.",
)
parser.add_argument(
    "--list-prefetcher-types",
    action=ListRubyPrefetcher,
    nargs=0,
    help="List Ruby L1 prefetcher types (--mem-system ruby) and exit.",
)
parser.add_argument(
    "--prefetcher-type",
    default=PREFETCHER_NONE,
    choices=[PREFETCHER_NONE] + _RUBY_PREFETCHER_TYPES,
    help=(
        "Ruby only: L1 data prefetcher when --mem-system ruby. "
        f"'{PREFETCHER_NONE}' disables (default). "
        "'RubyPrefetcher' enables the Ruby stream prefetcher."
    ),
)
parser.add_argument(
    "--l1d-hwp-type",
    default=None,
    choices=_HWP_CHOICES,
    help=(
        "Classic only: L1 data-cache prefetcher "
        f"({HWP_NONE} disables; unset keeps the cache default)."
    ),
)
parser.add_argument(
    "--l1i-hwp-type",
    default=None,
    choices=_HWP_CHOICES,
    help="Classic only: L1 instruction-cache prefetcher.",
)
parser.add_argument(
    "--l2-hwp-type",
    default=None,
    choices=_HWP_CHOICES,
    help="Classic only: L2 prefetcher.",
)
parser.add_argument(
    "--hwrng-lat",
    type=int,
    default=1,
    help="Latency for RDRAND operations. Unused if --harden-prng is not set.",
)
parser.add_argument(
    "--rdseed-lat", type=int, default=1, help="Latency for RDSEED operations"
)
args = parser.parse_args()

X86ISA.hwrng_type = args.prng_select

app_rng_opt = args.prng_select
if args.use_hwrng:
    app_rng_opt = "HWRNG"

if args.mem_system == "classic":
    if args.prefetcher_type != PREFETCHER_NONE:
        parser.error(
            "--prefetcher-type is only valid with --mem-system ruby; "
            "use --l1d-hwp-type / --l1i-hwp-type / --l2-hwp-type with classic"
        )
else:
    for opt_name in ("l1d_hwp_type", "l1i_hwp_type", "l2_hwp_type"):
        if getattr(args, opt_name) is not None:
            parser.error(
                f"--{opt_name.replace('_', '-')} is only valid with "
                "--mem-system classic; use --prefetcher-type with ruby"
            )

coherence_protocol_required = None
if args.mem_system == "ruby":
    coherence_protocol_required = CoherenceProtocol.MESI_TWO_LEVEL

requires(
    coherence_protocol_required=coherence_protocol_required,
    kvm_required=True,
)


def _apply_classic_hwp(caches, hwp_type):
    if hwp_type is None:
        return
    if hwp_type == HWP_NONE:
        for cache in caches:
            cache.prefetcher = NULL
    else:
        hwp_class = hwp_list.get(hwp_type)
        for cache in caches:
            cache.prefetcher = hwp_class()


if args.mem_system == "classic":
    from gem5.components.boards.abstract_board import AbstractBoard
    from gem5.components.cachehierarchies.classic.private_l1_shared_l2_walk_cache_hierarchy import (
        PrivateL1SharedL2WalkCacheHierarchy,
    )

    class StochsuiteClassicCacheHierarchy(PrivateL1SharedL2WalkCacheHierarchy):
        def __init__(
            self,
            l1d_hwp_type,
            l1i_hwp_type,
            l2_hwp_type,
            **kwargs,
        ):
            self._l1d_hwp_type = l1d_hwp_type
            self._l1i_hwp_type = l1i_hwp_type
            self._l2_hwp_type = l2_hwp_type
            super().__init__(**kwargs)

        @overrides(PrivateL1SharedL2WalkCacheHierarchy)
        def incorporate_cache(self, board: AbstractBoard) -> None:
            super().incorporate_cache(board)
            _apply_classic_hwp(self.l1dcaches, self._l1d_hwp_type)
            _apply_classic_hwp(self.l1icaches, self._l1i_hwp_type)
            _apply_classic_hwp([self.l2cache], self._l2_hwp_type)

    cache_hierarchy = StochsuiteClassicCacheHierarchy(
        l1d_hwp_type=args.l1d_hwp_type,
        l1i_hwp_type=args.l1i_hwp_type,
        l2_hwp_type=args.l2_hwp_type,
        l1d_size=_L1D_SIZE,
        l1i_size=_L1I_SIZE,
        l2_size=_L2_SIZE,
        l1d_assoc=_L1_ASSOC,
        l1i_assoc=_L1_ASSOC,
        l2_assoc=_L2_ASSOC,
    )
else:
    from gem5.components.boards.abstract_board import AbstractBoard
    from gem5.components.cachehierarchies.ruby.mesi_two_level_cache_hierarchy import (
        MESITwoLevelCacheHierarchy,
    )

    class StochsuiteRubyCacheHierarchy(MESITwoLevelCacheHierarchy):
        def __init__(self, prefetcher_type, **kwargs):
            self._prefetcher_type = prefetcher_type
            super().__init__(**kwargs)

        @overrides(MESITwoLevelCacheHierarchy)
        def incorporate_cache(self, board: AbstractBoard) -> None:
            super().incorporate_cache(board)
            if self._prefetcher_type == PREFETCHER_NONE:
                return
            pf_class = ruby_prefetcher_list.get(self._prefetcher_type)
            cache_line_size = board.get_cache_line_size()
            for l1 in self._l1_controllers:
                l1.prefetcher = pf_class(block_size=cache_line_size)
                l1.enable_prefetch = True

    cache_hierarchy = StochsuiteRubyCacheHierarchy(
        prefetcher_type=args.prefetcher_type,
        l1d_size=_L1D_SIZE,
        l1d_assoc=_L1_ASSOC,
        l1i_size=_L1I_SIZE,
        l1i_assoc=_L1_ASSOC,
        l2_size=_L2_SIZE,
        l2_assoc=_L2_ASSOC,
        num_l2_banks=_NUM_L2_BANKS,
    )

memory = DualChannelDDR4_2400(size="3GiB")

processor = SimpleSwitchableProcessor(
    starting_core_type=CPUTypes.KVM,
    switch_core_type=CPUTypes.O3,
    isa=ISA.X86,
    num_cores=1,
)

for proc in processor.start:
    proc.core.usePerf = False

_stochsuite_home = os.environ.get(
    "STOCHSUITE_HOME",
    os.path.join(os.path.dirname(__file__), "../stochsuite"),
)
_stripped_binary = os.path.join(
    _stochsuite_home, "apps", f"{args.benchmark}.stripped"
)

cond_bp_class = conditional_bp_list.get(args.bp_type)
print("Configuring HWRNG latency...")
for switch_core in processor._switchable_cores[processor._switch_key]:
    bp = BranchPredictor(
        conditionalBranchPred=cond_bp_class(numThreads=1),
        instShiftAmt=0,
        speculativeHistUpdate=False,
    )
    switch_core.core.branchPred = bp
    switch_core.core.stoch_branch_monitor = StochBranchMonitor(
        binary=_stripped_binary, bpred=bp
    )
    c = switch_core.core
    if hasattr(c, "instQueues"):
        for iq in c.instQueues:
            if hasattr(iq, "fuPool"):
                for fu in iq.fuPool.FUList:
                    for op in fu.opList:
                        if str(op.opClass) == "RdRand":
                            print(
                                f"Found RdRand OpClass in {fu.path()}. "
                                f"Setting latency to {args.hwrng_lat}"
                            )
                            op.opLat = args.hwrng_lat
                        elif str(op.opClass) == "RdSeed":
                            print(
                                f"Found RdSeed OpClass in {fu.path()}. "
                                f"Setting latency to {args.rdseed_lat}"
                            )
                            op.opLat = args.rdseed_lat

board = X86Board(
    clk_freq="3GHz",
    processor=processor,
    memory=memory,
    cache_hierarchy=cache_hierarchy,
)

_guest_glibc_hwcaps = (
    "export GLIBC_TUNABLES=glibc.cpu.hwcaps="
    "-AVX512F,-AVX512VL,-AVX512DQ,-AVX512BW,-AVX512IFMA,-AVX512VBMI,"
    "-AVX2,-AVX,-FMA;"
)

# Pattern A: hypercall 2 immediately before the benchmark binary. Ubuntu 24.04
# images may also issue HC2 from after_boot.sh (early switch); this readfile
# HC2 resets stats right before the workload. Handler is idempotent on switch.
_hypercall2_cmd = (
    "if command -v gem5-bridge >/dev/null 2>&1; then gem5-bridge hypercall 2; "
    "elif command -v m5 >/dev/null 2>&1; then m5 hypercall 2; "
    "elif [ -x /sbin/m5 ]; then /sbin/m5 hypercall 2; "
    "else echo WARNING: no gem5-bridge or m5; cannot trigger hypercall 2; fi; "
)

guest_cmd = _guest_glibc_hwcaps + (
    "echo TEST_START; "
    "sleep 2; "
    + _hypercall2_cmd
    + f"/home/gem5/stochsuite/{args.benchmark}_gem5 "
    f"-seed {args.seed} -iters {args.iters} -rng {app_rng_opt}; "
    "sleep 2; "
    "echo TEST_END; "
    "sleep 2; "
    "m5 exit"
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


class SwitchToO3AfterBootExitHandler(ExitHandler, hypercall_num=2):
    """Switch KVM -> O3 and reset stats before the benchmark (Pattern A)."""

    @overrides(ExitHandler)
    def _process(self, simulator: "Simulator") -> None:
        proc = simulator._board.get_processor()
        if getattr(proc, "_current_is_start", True):
            print("Hypercall 2: switching KVM -> O3 before benchmark")
            simulator.switch_processor()
        else:
            print("Hypercall 2: already on O3, skipping switch")
        print("Hypercall 2: resetting stats before benchmark")
        m5.stats.reset()

    @overrides(ExitHandler)
    def _exit_simulation(self) -> bool:
        return False


def workbegin_handler():
    print("WorkBegin: ROI marker (stats reset at hypercall 2)")
    yield False


def workend_handler():
    m5.stats.dump()
    print("WorkEnd: stats dumped, exiting")
    yield True


simulator = Simulator(
    board=board,
    on_exit_event={
        ExitEvent.WORKBEGIN: workbegin_handler(),
        ExitEvent.WORKEND: workend_handler(),
    },
)

_prefetch_summary = args.prefetcher_type
if args.mem_system == "classic":
    _prefetch_summary = (
        f"l1d={args.l1d_hwp_type or 'default'}, "
        f"l1i={args.l1i_hwp_type or 'default'}, "
        f"l2={args.l2_hwp_type or 'default'}"
    )

print(
    f"Running stochsuite/{args.benchmark} with iters={args.iters}, "
    f"app-rng={app_rng_opt}, prng-select={args.prng_select}, "
    f"harden-prng={args.use_hwrng}, mem-system={args.mem_system}, "
    f"bp-type={args.bp_type}, prefetch={_prefetch_summary}"
)
simulator.run()
