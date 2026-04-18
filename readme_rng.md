# Random Number Generator (RNG) Integration Guide

This guide explains how to add a new Random Number Generator to the `stochsuite` software library and how to integrate it into the gem5 simulator to power architectural instructions like `RDRAND`.

---

## Part 1: Integrating the RNG into gem5

To make your software RNG available inside the gem5 simulator (e.g., as the backend for the x86 `RDRAND` instruction), follow these steps:

### 1. Update gem5 Build Dependencies
Modify `gem5_sim/gem5/src/arch/x86/SConscript` to include your new source files in the gem5 build process:

```python
# Add your new RNG file
Source('#/../../rngs/software/MyRNG.cpp', tags=['x86 isa'])
```

### 2. Add Global Includes
Modify `gem5_sim/gem5/src/arch/x86/isa/includes.isa` to include your header in the global ISA namespace:

```cpp
#include "MyRNG.hpp"
```

### 3. Update the Micro-op Implementation
Modify `gem5_sim/gem5/src/arch/x86/isa/microops/regop.isa`. Locate the `RdRandOp` class and update it to use your new RNG class:

```python
class RdRandOp(BasicRegOp):
    code = '''
        static ::MyRNG rng;  // Use the global namespace scope
        DestReg = merge(DestReg, dest, (uint64_t)rng._read_random(), dataSize);
    '''
```

---

## Part 2: Rebuilding

After making these changes, you must rebuild both the software suite and the simulator.

```bash
./gem5_sim/gem5/build/X86/gem5.opt \
  ./gem5_sim/gem5/configs/deprecated/example/se.py \
  -c ./apps/pi.o \
  --options="-rng HWRNG -iters 1000"
```
