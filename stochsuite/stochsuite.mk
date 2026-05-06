RNG_LIB := $(STOCHSUITE_HOME)/rngs/software
RNG_SRCS := $(wildcard $(RNG_LIB)/*.cpp)
CPP := g++
COMMON_CFLAGS := -c -std=c++11 -Wall -O3 -lm
PLATFORM_CFLAGS :=
LFLAGS := -std=c++11 -Wall -O3 -g -static
UNAME := $(shell uname)
OS := $(shell uname -s)
ARCH := $(shell uname -m)

# Full-system runs switch from KVM to gem5 Timing/O3 at m5_work_begin. The
# detailed core does not execute the full host ISA; -O3 codegen (and some
# static-lib paths) can emit AVX/BMI/etc. and fault with SIGILL. Pin x86-64
# Linux builds to a conservative subset. Override with GEM5_X86_ISA= if needed.
ifeq ($(OS), Linux)
ifeq ($(ARCH), x86_64)
GEM5_X86_ISA ?= -march=x86-64 -mtune=generic -mprefer-vector-width=128 -mno-avx -mno-avx2 -mno-bmi -mno-bmi2 -fcf-protection=none
COMMON_CFLAGS += $(GEM5_X86_ISA)
LFLAGS += $(GEM5_X86_ISA)
endif
endif

# gem5 m5ops integration. Override GEM5_HOME if your gem5 checkout lives
# elsewhere. Before building the apps for gem5 simulation you must build
# libm5 once: `cd $(GEM5_HOME)/util/m5 && scons build/x86/out/m5`.
GEM5_HOME ?= $(STOCHSUITE_HOME)/../gem5
M5OPS_INC := -I$(GEM5_HOME)/include -I$(GEM5_HOME)/util/m5/src
M5OPS_LIBS := -L$(GEM5_HOME)/util/m5/build/x86/out -lm5

# Full-system gem5 workloads: link without -static so they use the guest’s
# glibc/libm. Together with GLIBC_TUNABLES in the FS run script, IFUNCs can
# avoid AVX/VEX paths that fault on gem5 Timing. Other app targets keep LFLAGS
# (often -static) unchanged.
LFLAGS_FS := $(filter-out -static,$(LFLAGS)) -no-pie
