# 1. Read and Elaborate
yosys read -sv "$::env(MODULE).v"
yosys hierarchy -top $::env(MODULE)

# 2. Generic Synthesis (Fixes path/logic issues)
yosys "synth -top $::env(MODULE)"

# 3. Map to your specific Library (Fixes Area)
yosys "dfflibmap -liberty $::env(TECH_NODE).lib"
yosys "abc -liberty $::env(TECH_NODE).lib"

# 4. Clean and Report
yosys clean
yosys "stat -liberty $::env(TECH_NODE).lib"
yosys "ltp -noff"
