#!/bin/bash

./build/X86/gem5.opt --outdir=pi_sw_taus88 configs/x86-se-pi.py                --prng-select=Taus88 --app-iters=1000
./build/X86/gem5.opt --outdir=pi_hw_taus88 configs/x86-se-pi.py --harden-prng  --prng-select=Taus88 --hwrng-lat=1 --rdseed-lat=200 --app-iters=1000
./build/X86/gem5.opt --outdir=pi_sw_taus113 configs/x86-se-pi.py               --prng-select=Taus113 --app-iters=1000
./build/X86/gem5.opt --outdir=pi_hw_taus113 configs/x86-se-pi.py --harden-prng --prng-select=Taus113 --hwrng-lat=1 --rdseed-lat=200 --app-iters=1000
./build/X86/gem5.opt --outdir=pi_sw_jkiss configs/x86-se-pi.py               --prng-select=JKISS --app-iters=1000
./build/X86/gem5.opt --outdir=pi_hw_jkiss configs/x86-se-pi.py --harden-prng --prng-select=JKISS --hwrng-lat=1 --rdseed-lat=200 --app-iters=1000
./build/X86/gem5.opt --outdir=pi_sw_jkiss32 configs/x86-se-pi.py               --prng-select=JKISS32 --app-iters=1000
./build/X86/gem5.opt --outdir=pi_hw_jkiss32 configs/x86-se-pi.py --harden-prng --prng-select=JKISS32 --hwrng-lat=1 --rdseed-lat=200 --app-iters=1000
./build/X86/gem5.opt --outdir=pi_sw_cong configs/x86-se-pi.py               --prng-select=CONG --app-iters=1000
./build/X86/gem5.opt --outdir=pi_hw_cong configs/x86-se-pi.py --harden-prng --prng-select=CONG --hwrng-lat=1 --rdseed-lat=200 --app-iters=1000
./build/X86/gem5.opt --outdir=pi_sw_glibc_crand configs/x86-se-pi.py               --prng-select=GLIBC_CRAND --app-iters=1000
./build/X86/gem5.opt --outdir=pi_hw_glibc_crand configs/x86-se-pi.py --harden-prng --prng-select=GLIBC_CRAND --hwrng-lat=1 --rdseed-lat=200 --app-iters=1000
./build/X86/gem5.opt --outdir=pi_sw_drand48 configs/x86-se-pi.py               --prng-select=DRAND48 --app-iters=1000
./build/X86/gem5.opt --outdir=pi_hw_drand48 configs/x86-se-pi.py --harden-prng --prng-select=DRAND48 --hwrng-lat=1 --rdseed-lat=200 --app-iters=1000
./build/X86/gem5.opt --outdir=pi_sw_mt19937 configs/x86-se-pi.py               --prng-select=MersenneTwister --app-iters=1000
./build/X86/gem5.opt --outdir=pi_hw_mt19937 configs/x86-se-pi.py --harden-prng --prng-select=MersenneTwister --hwrng-lat=1 --rdseed-lat=200 --app-iters=1000
./build/X86/gem5.opt --outdir=pi_sw_kiss11 configs/x86-se-pi.py               --prng-select=KISS11 --app-iters=1000
./build/X86/gem5.opt --outdir=pi_hw_kiss11 configs/x86-se-pi.py --harden-prng --prng-select=KISS11 --hwrng-lat=1 --rdseed-lat=200 --app-iters=1000
./build/X86/gem5.opt --outdir=pi_sw_pcgbasic configs/x86-se-pi.py               --prng-select=PCGBasic --app-iters=1000
./build/X86/gem5.opt --outdir=pi_hw_pcgbasic configs/x86-se-pi.py --harden-prng --prng-select=PCGBasic --hwrng-lat=1 --rdseed-lat=200 --app-iters=1000
./build/X86/gem5.opt --outdir=pi_sw_xorshift32 configs/x86-se-pi.py               --prng-select=XorShift32 --app-iters=1000
./build/X86/gem5.opt --outdir=pi_hw_xorshift32 configs/x86-se-pi.py --harden-prng --prng-select=XorShift32 --hwrng-lat=1 --rdseed-lat=200 --app-iters=1000
./build/X86/gem5.opt --outdir=pi_sw_xorshift128 configs/x86-se-pi.py               --prng-select=XorShift128 --app-iters=1000
./build/X86/gem5.opt --outdir=pi_hw_xorshift128 configs/x86-se-pi.py --harden-prng --prng-select=XorShift128 --hwrng-lat=1 --rdseed-lat=200 --app-iters=1000
./build/X86/gem5.opt --outdir=pi_sw_xorwow configs/x86-se-pi.py               --prng-select=XorWow --app-iters=1000
./build/X86/gem5.opt --outdir=pi_hw_xorwow configs/x86-se-pi.py --harden-prng --prng-select=XorWow --hwrng-lat=1 --rdseed-lat=200 --app-iters=1000
./build/X86/gem5.opt --outdir=pi_sw_xoshiro128pp configs/x86-se-pi.py               --prng-select=XoShiRo128++ --app-iters=1000
./build/X86/gem5.opt --outdir=pi_hw_xoshiro128pp configs/x86-se-pi.py --harden-prng --prng-select=XoShiRo128++ --hwrng-lat=1 --rdseed-lat=200 --app-iters=1000
./build/X86/gem5.opt --outdir=pi_hw_intel_rdrand configs/x86-se-pi.py --harden-prng --prng-select=MersenneTwister --hwrng-lat=65 --rdseed-lat=200 --app-iters=1000
