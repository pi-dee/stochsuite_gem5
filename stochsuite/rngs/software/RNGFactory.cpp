#include "RNGFactory.hpp"

#include "HWRNG.hpp"
#include "JKISS.hpp"
#include "JKISS32.hpp"
#include "KISS11.hpp"
#include "LCG.hpp"
#include "MersenneTwister.hpp"
#include "PCGBasic.hpp"
#include "Taus88.hpp"
#include "Taus113.hpp"
#include "XorShift128.hpp"
#include "XorShift32.hpp"
#include "XorWow.hpp"
#include "XoShiRo128PlusPlus.hpp"

#include <string>
#include <memory>

RNGFactory::RNGFactory() {}

RNGFactory::~RNGFactory() {}

std::unique_ptr<RNGBase> RNGFactory::createRNG(const std::string type) {
    if (type == "Taus88") {
        return std::unique_ptr<RNGBase>(new Taus88());
    } else if (type == "Taus113") {
        return std::unique_ptr<RNGBase>(new Taus113());
    } else if (type == "JKISS") {
        return std::unique_ptr<RNGBase>(new JKISS());
    } else if (type == "JKISS32") {
        return std::unique_ptr<RNGBase>(new JKISS32());
    } else if (type == "CONG") {
        // Based on KISS paper
        // https://eprint.iacr.org/2011/007.pdf
        return std::unique_ptr<RNGBase>(new LCG(123132, 69069, 1234567, 0xFFFFFFFF, "LCG_CONG"));
    } else if (type == "GLIBC_CRAND") {
        // Based on C implementation of random_r.c
        // https://sourceware.org/git/?p=glibc.git;a=blob;f=stdlib/random_r.c;hb=glibc-2.28#l353
        return std::unique_ptr<RNGBase>(new LCG(123132, 1103515245U, 12345U, 0x7FFFFFFF, "LCG_GLIBC_C_RAND"));
    } else if (type == "DRAND48") {
        // Based on C implementation of drand48-iter.c
        // https://codebrowser.dev/glibc/glibc/stdlib/drand48-iter.c.html#__drand48_iterate
        return std::unique_ptr<RNGBase>(new LCG(1, 0x5deece66dull, 0xb, 0xFFFFFFFF, "LCG_DRAND48_C_RAND"));
    } else if (type == "MersenneTwister") {
        // Reference
        // M. Matsumoto and T. Nishimura, "Mersenne Twister: A 623-Dimensionally
        // Equidistributed Uniform Pseudo-Random Number Generator", ACM Transactions on
        // Modeling and Computer Simulation, Vol. 8, No. 1, January 1998, pp 3-30.
        return std::unique_ptr<RNGBase>(new MersenneTwister());
    } else if (type == "KISS11") {
        // Based on original KISS paper
        // https://eprint.iacr.org/2011/007.pdf
        return std::unique_ptr<RNGBase>(new KISS11());
    } else if (type == "PCGBasic") {
        // Based on original PCG paper
        // https://www.pcg-random.org/
        // https://www.pcg-random.org/pdf/hmc-cs-2014-0905.pdf
        // TODO: seems to be broken
        return std::unique_ptr<RNGBase>(new PCGBasic());
    } else if (type == "HWRNG") {
        return std::unique_ptr<RNGBase>(new HWRNG());
    } else if (type == "XorShift32") {
        // Implemented from pg4 of Marsaglia, "Xorshift RNGs"
        // https://www.jstatsoft.org/article/view/v008i14
        return std::unique_ptr<RNGBase>(new XorShift32());
    } else if (type == "XorShift128") {
        // Implemented from pg5 of Marsaglia, "Xorshift RNGs"
        // https://www.jstatsoft.org/article/view/v008i14
        return std::unique_ptr<RNGBase>(new XorShift128());
    } else if (type == "XorWow") {
        // Implemented from pg5 of Marsaglia, "Xorshift RNGs"
        // https://www.jstatsoft.org/article/view/v008i14
        // Default generator for NVIDIA CUDA toolkit: https://docs.nvidia.com/cuda/curand/testing.html
        return std::unique_ptr<RNGBase>(new XorWow());
    } else if (type == "XoShiRo128++") {
        // Implemented from https://prng.di.unimi.it/xoshiro128plusplus.c
        return std::unique_ptr<RNGBase>(new XoShiRo128PlusPlus());
    } else {
        return nullptr;
    }
}
