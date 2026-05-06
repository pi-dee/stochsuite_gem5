include $(STOCHSUITE_HOME)/stochsuite.mk
include $(STOCHSUITE_HOME)/rngs/hardware/vlsi_technologies_synopsys.mk

DUT ?= taus88
XILINX_CELLS_DIR ?= XilinxUnisimLibrary/verilog/src
IVERILOG ?= /usr/bin/iverilog
YOSYS ?= /usr/bin/yosys

# Which standard cell library to use
SCL ?= vsclib013

# IVerilog Simulation
sim: ${DUT}-sim
	./$< +VCDFILE=sim.vcd +VCDLEVEL=0 | tee ${DUT}-sim.log

${DUT}-sim: ${DUT}_tb.v ${DUT}.v
	${IVERILOG} -g2005-sv -s ${DUT}_tb -y ${XILINX_CELLS_DIR} -o $@ $^

# Yosys Synthesis
synth-fpga: ${DUT}.netlist.v

${DUT}.netlist.v: ${DUT}.v
	${YOSYS} -p 'synth_xilinx -top ${DUT}' -p stat $< -L ${DUT}-synth-fpga.log -o $@

gatesim: ./${DUT}-gatesim
	./$< +VCDFILE=gatesim.vcd +VCDLEVEL=1 | tee ${DUT}-gatesim.log

${DUT}-gatesim: ${DUT}_tb.v ${DUT}.netlist.v
	${IVERILOG} -g2005-sv -s ${DUT}_tb -DGATESIM -y ${XILINX_CELLS_DIR} -y ${XILINX_CELLS_DIR}/unisims -o $@ $^

ifneq ($(filter synth-asic,$(MAKECMDGOALS)),)
    ifeq ($(filter $(SCL),$(VALID_TECH_NODES)),)
        $(info Please choose from valid tech nodes: $(VALID_TECH_NODES))
        $(error Invalid Standard Cell Library selection: "$(SCL)".)
    endif
endif

${SCL}.lib:
	@echo "Downloading ${SC}.lib ..."
	wget http://www.vlsitechnology.org/synopsys/${SCL}.lib

synth-asic: ${DUT}.v ${SCL}.lib
	export MODULE=${DUT}; \
	export TECH_NODE=${SCL}; \
    ${YOSYS} -c area_and_path_length.tcl | tee ${DUT}-synth-asic.log

clean:
	@rm -f *-sim
	@rm -f *-gatesim
	@rm -f *-sim.log
	@rm -f *-synth-asic.log
	@rm -f *-synth-fpga.log
	@rm -f *-gatesim.log
	@rm -f *.netlist.v
