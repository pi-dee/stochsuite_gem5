#!/bin/bash

./build/X86/gem5.opt --outdir=pi_hw_taus88 configs/x86-se-pi.py --prng-select=Taus88
./build/X86/gem5.opt --outdir=pi_hw_taus88 configs/x86-se-pi.py --harden-prng --prng-select=Taus88 --hwrng-lat=1 --rdseed-lat=200
