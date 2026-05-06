# RTL Implementations of Pseudo-Random Number Generators

## Simulating Taus88 Testbench

The testbench can be compiled and run with the following command
```
make sim DUT=taus88
```

Note, the argument to the `DUT` parameter must match the name of the
source verilog file, the prefix of the testbench file name, as well
as the prefix for the testbench module name.

The hardware testbench checks against values produced from the software implementation, i.e. output from:

```
./util/correctness.o -iters 10 -seed 0xCAFEBABE -rng Taus88
```

### Troubleshooting

<details>

If you get an error like:

```
taus88.netlist.v:1731: error: Unknown module type: FDRE
taus88.netlist.v:1742: error: Unknown module type: FDRE
389 error(s) during elaboration.
*** These modules were missing:
        BUFG referenced 1 times.
        FDRE referenced 96 times.
        IBUF referenced 35 times.
        INV referenced 96 times.
        LUT2 referenced 36 times.
        LUT3 referenced 79 times.
        LUT4 referenced 13 times.
        OBUF referenced 32 times.
***
```

It's likely that the `XilinxUnisimLibrary` was not pulled. Run the following and then retry
the command:

```
cd $STOCHSUITE_HOME
git submodule update --init --recursive
cd $STOCHSUITE_HOME/rngs/hardware/
```

</details>

## FPGA Synthesis Flow

### Synthesizing Taus88 to Netlist for Xilinx FPGA

The following command will attempt to use Xilinx primitives to create a netlist to implement design

```
make synth-fpga DUT=taus88
```

You should see output like:

```
=== design hierarchy ===

   taus88                            1
     $paramod$e07d4e29b3300cbec84577cedf54b2f8181665cb\Register      1

   Number of wires:                183
   Number of wire bits:            887
   Number of public wires:          19
   Number of public wire bits:     645
   Number of memories:               0
   Number of memory bits:            0
   Number of processes:              0
   Number of cells:                388
     BUFG                            1
     FDRE                           96
     IBUF                           35
     INV                            96
     LUT2                           36
     LUT3                           79
     LUT4                           13
     OBUF                           32
```

The command prints output to stdout, but is also captured
in a log file named off of the provide argument `DUT`, in this case,
`taus88-synth-fpga.log`.

### Functionality of Synthesizable Desgin

If the design were to be routed onto a real FPGA, we would like to verify that
the design functionally completes from it's netlist implementation. The following
commands rerun the simulation using the netlist output as the DUT:

```
make gatesim DUT=taus88
```

## ASIC Synthesis Flow

### Synthesizing Xorwow to Netlist from  Synopsys VLSI 13nm Standard Cell Library

The following command will attempt to download a supported open source
standard cell library liberty file, if not already present. The command then
runs the Tcl script `area_and_path_length.tcl` to estimate the area of PRNG
design, and the number of gate on the topologically longest path. The example
in the following command examines the `xorwow` PRNG using the VLSI Technologies
Synopsys liberty file for a 13nm technology node:


```
make synth-asic DUT=xorwow SCL=vsclib013
```

The command prints output to stdout, but also capture the synthesis report in
a file named off of the provide argument `DUT`, in this case,
`xorwow-synth-asic.log`. The bottom of the output should look something like:

```
     ...
     xooi21v0x05                     5
     xoon21v0x05                     3
     xor2v0x05                      80
     xor3v1x05                       7

   Chip area for top module '\xorwow': 7519.000000

7. Executing LTP pass (find longest path).

Longest topological path in $paramod$5abd54821b56aca50aadf8f6ee000d4bf520e9b2\Register (length=0):
    0: \write_data [191]

Longest topological path in xorwow (length=1):
    0: \z_wd [31]
    1: \x_cur [0] (via state_regs)
```

The output above indicates the design has a chip area of `7519` nm<sup>2</sup>, and
has a longest topological path length of `1`.
