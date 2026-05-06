# Environment Setup

## Requirements

Docker is a platform that uses operating-system-level virtualization to
deliver software in packages called containers. This generators and
applications in this suite have been functionally verified on the
following versions:

| Application | Version | Notes |
|---|---|---|
| Docker Engine| 27.3.1 | build ce12230 |
| Docker Desktop | 4.35.0 | (172550) |
| Docker Compose | v2.29.7-desktop.1 | |

## Building Docker Image Locally

One option to develop and run the code in this repo is to build a docker image
locally. You can inspect and modify the provided [Dockerfile](Dockerfile) to
add any additional packages you may need.

To build an image, rung the following in the same directory as the:

```
docker buildx create --use --name multi-platform-builder
docker buildx inspect multi-platform-builder --bootstrap
docker buildx build --platform linux/arm64,linux/amd64 \
  -t rselagam/stochsuite:v2_ubuntu_24.04 \
  --push .
```

## Pulling Exisitng Image from DockerHub

If you do not need any additional packages, you can pull the image directly from
dockerhub via:

```
docker pull rselagam/stochsuite:v2_ubuntu_24.04
```

## Starting an Interactive Container

Once the image is either built of pulled down to your local machine, it is
ready to be used for running a container. E.g. you can do this via:

```
docker run -it --rm -w /root -e "STOCHSUITE_HOME=/root/stochsuite"  rselagam/stochsuite:v2_ubuntu_24.04 /bin/bash -c "git clone https://github.com/BujSet/stochsuite.git && cd stochsuite/ && /bin/bash"
```

The command above creates a docker container with the necessary environment
vairables to use the Makefiles in the repo. It also clones the repo to the
correct path location to match the environment variable before yielding control
back to the user. After entering the project directory, run:

```
git submodule update --init --recursive
```

To ensure that all submodules (e.g., gem5, Xilinx lib, etc.) are pulled and available
to use.

# Building and Running RNG Applications

## Building RNG Library

The generator library must be built before the applications. Run the following

```
cd $STOCHSUITE_HOME/rngs/software/
make
```

The command above will compile all the C++ implementations of the random number
generators.

## Building the Application Library

After building the RNG library, we can now build the applications:

```
cd $STOCHSUITE_HOME/apps/
make
```

This will generate object files that can be run in the `apps` directory.

# Running an example application

## The Correctness Application

Running the command below from the `apps/` directory will run the correctness
application for the JKISS generator. See the [RNGFactory.cpp](rngs/software/RNGFactory.cpp)
file for a list of generator options.

```
./util/correctness.o -iters 5 -seed 0xdeadbeef -rng JKISS
```

You should see the following output:

```
Successfully initialized RNG: JKISS
Rand Value = 2778845915
Rand Value = 2959504851
Rand Value = 54303350
Rand Value = 4063145910
Rand Value = 255296776
```

# gem5 Full-System Simulation

The six workloads (`pi`, `dop`, `dropout`, `multinomial`, `photon`,
`tailwag`) are instrumented with `m5_work_begin_addr(0,0)` /
`m5_work_end_addr(0,0)` ROI markers around their RNG-consuming kernel loops.
A driver script in the gem5 tree boots Ubuntu on KVM, switches to the
O3 CPU at the start of the ROI, resets stats, and dumps stats at the
end of the ROI.

## One-time setup

The Makefiles all include `$(STOCHSUITE_HOME)/stochsuite.mk`, and the
build flags resolve `$(GEM5_HOME)` to find `libm5`. **Both variables must
be exported in the shell before any `make` invocation** (the Docker
workflow above exports `STOCHSUITE_HOME` automatically; outside Docker
you set them yourself):

```
export STOCHSUITE_HOME=/absolute/path/to/stochsuite
export GEM5_HOME=/absolute/path/to/gem5
```

For this checkout that is:

```
export GEM5_HOME=$HOME/work/stochsuite_gem5/gem5
export STOCHSUITE_HOME=$GEM5_HOME/stochsuite
```

1. Build `libm5` for the gem5 utility (the workloads link against this):

   ```
   cd $GEM5_HOME/util/m5
   scons build/x86/out/m5
   ```

2. Build the RNG library (existing step):

   ```
   cd $STOCHSUITE_HOME/rngs/software/
   make
   ```

3. Build the apps. The six FS workloads use `LFLAGS_FS` (same flags as
   `stochsuite.mk` but **without `-static`**) plus `libm5`; utilities such as
   `util/correctness` may still use `-static`. All are built with `-g`:

   ```
   cd $STOCHSUITE_HOME/apps/
   make
   ```

4. Obtain a base FS disk image. The cleanest option is to use gem5's
   `obtain-resource.py` to download a fresh copy of the canned
   `x86-ubuntu-24.04-img` to a stochsuite-specific path:

   ```
   mkdir -p $STOCHSUITE_HOME/disk_images
   cd $GEM5_HOME
   build/ALL/gem5.opt util/obtain-resource.py \
       x86-ubuntu-24.04-img \
       -p $STOCHSUITE_HOME/disk_images/stochsuite-base.img
   ```

   The download is a few GiB and ends up as a raw image at
   `$STOCHSUITE_HOME/disk_images/stochsuite-base.img`.

   If you already have a base image elsewhere on disk that you do not
   mind mutating, copy it instead:

   ```
   mkdir -p $STOCHSUITE_HOME/disk_images
   cp /path/to/some-x86-ubuntu-24.04.img \
      $STOCHSUITE_HOME/disk_images/stochsuite-base.img
   ```

   Either way, **do not point the next step at a shared image** — the
   helper script writes the workload binaries into the image in place.

5. Install the binaries into the FS disk image. The helper script
   loop-mounts the image, copies each `*.o` into
   `/home/gem5/stochsuite/<name>_gem5`, and unmounts:

   ```
   sudo $STOCHSUITE_HOME/tools/populate_disk_image.sh \
       $STOCHSUITE_HOME/disk_images/stochsuite-base.img \
       $GEM5_HOME \
       $STOCHSUITE_HOME
   ```

If you see `Makefile:1: /stochsuite.mk: No such file or directory`, it
means `$STOCHSUITE_HOME` was empty when `make` ran — re-export it in the
current shell.

## Running a workload

Build the gem5 binary once, then drive a benchmark:

```
cd $GEM5_HOME
scons build/ALL/gem5.opt -j$(nproc)
./build/ALL/gem5.opt \
    configs/x86-fs-stochsuite-workloads.py \
    --benchmark pi --iters 1000 \
    --disk-image $STOCHSUITE_HOME/disk_images/stochsuite-base.img
```

Useful flags:

- `--rng <name>` and `--seed <int>` are forwarded to the workload.
- `--disk-image <path>` overrides the default disk image location.

Stats for the ROI are written to `m5out/stats.txt` at WORKEND.

# TODO

## Random Number Generator Suite

* TODO: implement other RNGS:
    * Yarrow: https://www.schneier.com/academic/yarrow/
    * Fortuna: https://www.schneier.com/wp-content/uploads/2015/12/fortuna.pdf
    * PCGBasic: Seems to be broken... Need to fix
    * Parallel Random Number Generation: https://www.thesalmons.org/john/random123/papers/random123sc11.pdf
    * HWRNG: should add check for x86_64 support for DRNG for graceful error message rather than program crash
    * Middle-squared method: https://www.scratchapixel.com/lessons/mathematics-physics-for-computer-graphics/monte-carlo-methods-in-practice/generating-random-numbers.html

## Applications:

* TODO: add more apps
    * Monte Carlo McBeth Color Chart Approximator via Integration Approximation: https://www.scratchapixel.com/lessons/mathematics-physics-for-computer-graphics/monte-carlo-methods-in-practice/monte-carlo-rendering-practical-example.html, https://github.com/scratchapixel/scratchapixel-code/blob/main/monte-carlo-methods-in-practice/mcintegration.cpp, https://www.scratchapixel.com/lessons/mathematics-physics-for-computer-graphics/monte-carlo-methods-in-practice/monte-carlo-integration.html
