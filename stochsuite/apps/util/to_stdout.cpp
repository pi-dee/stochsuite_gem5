#include "RNGFactory.hpp"
#include "RandomNumberGenerator.hpp"
#include <cassert>
#include <cstring>
#include <iostream>

int main(int argc, char *argv[]) {
    size_t niters = 100000000000;
    uint32_t seed = 1634404289;
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
                seed = (size_t) strtol(argv[i+1], NULL, 10);
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
    std::unique_ptr<RNGBase> rng = RNGFactory::createRNG(rngStr);
    if (!rng) {
        std::cout << "Failed to instantiate RNG instance!" << std::endl;
        return 1;
    }

    rng->seed_random(seed);
    uint32_t rnd;
    FILE *bstdout = freopen(NULL, "wb", stdout);
    for (size_t i = 0; i < niters; i++) {
        rnd = rng->read_random();
        fwrite(&rnd, sizeof(rnd), 1, bstdout);
    }
    // Can ignore the output here
    std::ignore = freopen(NULL, "wb", stdout);
	return 0;
}
