include $(STOCHSUITE_HOME)/stochsuite.mk
RNG_OBJS := $(wildcard $(RNG_LIB)/*.o)

ifeq ($(OS), Linux)
    ifeq ($(ARCH), x86_64)
        PLATFORM_CFLAGS += -msse2
    endif
    ifeq ($(ARCH), aarch64)
        PLATFORM_CFLAGS += -march=armv8.5-a+rng
    endif
endif

all:
	@$(CPP) -I$(RNG_LIB) $(M5OPS_INC) $(RNG_OBJS) $(LFLAGS_FS) pi.cpp $(M5OPS_LIBS) -o pi.o
	@$(CPP) -I$(RNG_LIB) $(RNG_OBJS) $(LFLAGS) util/correctness.cpp -o util/correctness.o
	@$(CPP) -I$(RNG_LIB) $(M5OPS_INC) $(RNG_OBJS) $(LFLAGS_FS) dropout.cpp $(M5OPS_LIBS) -o dropout.o
	@$(CPP) -I$(RNG_LIB) $(M5OPS_INC) $(RNG_OBJS) $(LFLAGS_FS) multinomial.cpp $(M5OPS_LIBS) -o multinomial.o
	@$(CPP) -I$(RNG_LIB) $(M5OPS_INC) $(RNG_OBJS) $(LFLAGS_FS) dop.cpp $(M5OPS_LIBS) -o dop.o
	@$(CPP) -I$(RNG_LIB) $(M5OPS_INC) $(RNG_OBJS) $(LFLAGS_FS) photon.cpp $(M5OPS_LIBS) -o photon.o
	@$(CPP) -I$(RNG_LIB) $(M5OPS_INC) $(RNG_OBJS) $(LFLAGS_FS) tailwag.cpp $(M5OPS_LIBS) -o tailwag.o
	@$(CPP) -I$(RNG_LIB) $(RNG_OBJS) $(LFLAGS) util/to_stdout.cpp -o util/to_stdout.o
	@if [ -f  /.dockerenv ]; then \
		echo "[WARN] Detected docker environment, skipping build of latency.o..."; \
	else \
	    $(CPP) -I$(RNG_LIB) $(RNG_OBJS) $(LFLAGS) $(PLATFORM_CFLAGS) util/latency.cpp -o util/latency.o; \
	fi
clean:
	@rm -f *.o *.ppm *.bin *.hex
	@rm -f util/*.o
