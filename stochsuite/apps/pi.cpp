#include "RNGFactory.hpp"
#include "RandomNumberGenerator.hpp"
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <gem5/m5ops.h>
#include "m5_mmap.h"

int main(int argc, char *argv[]) {

    size_t niters = 100;
    uint32_t seed = 12312332;
    std::string rngStr = "Taus88";

    if (argc > 1) {
        if (argc > 7) {
            std::cout << "usage: ./" << argv[0] << " -seed <SEED> ";
            std::cout << " -iters <NUM_ITERS> -rng <RNG>" << std::endl;
            return 1;
        }
        for (size_t i = 0; i < (size_t)argc; i++) {
            if (strcmp("-seed", argv[i]) == 0) {
                assert(i + 1 < (size_t)argc);
                seed = (uint32_t) strtol(argv[i+1], NULL, 10);
                i++;
            }
            if (strcmp("-iters", argv[i]) == 0) {
                assert(i + 1 < (size_t)argc);
                niters = (size_t) strtol(argv[i+1], NULL, 10);
                i++;
            }
            if (strcmp("-rng", argv[i]) == 0) {
                assert(i + 1 < (size_t)argc);
                rngStr = (std::string) argv[i+1];
                i++;
            }
        }
    }
    double x, y, z, pi;
    size_t count = 0;

    std::unique_ptr<RNGBase> rng = RNGFactory::createRNG(rngStr);
    if (!rng) {
        std::cout << "Failed to instantiate RNG instance!" << std::endl;
        return 1;
    }
    std::cout << "Successfully initialized RNG: " << rng->name() << std::endl;

    rng->seed_random(seed);
    map_m5_mem();
    m5_work_begin_addr(0, 0);
    for (size_t i = 0; i < niters; i++) {
        x = rng->read_random_double();
        y = rng->read_random_double();
        z = sqrt((x*x) + (y*y));
        if (z <= 1.0) {
            count++;
        }
    }
    pi = 4.0 * ((double)count) / ((double)niters);
    m5_work_end_addr(0, 0);

    std::cout << "pi = " << pi << std::endl;
	return 0;
}
