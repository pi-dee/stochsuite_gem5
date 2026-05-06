#include "RNGFactory.hpp"
#include "RandomNumberGenerator.hpp"
#include <cassert>
#include <cerrno>
#include <climits>
#include <cmath>
#include <cstring>
#include <iostream>

int main(int argc, char *argv[]) {

    size_t niters = 10;
    uint32_t seed = 1634404289;
    std::string rngStr = "Taus88";
    bool dumpState = false;
    std::string dumpStateFilename = "Taus88_initial_state.hex";
    int saved_errno;
    char* endptr;

    if (argc > 1) {
        if (argc > 7) {
            std::cout << "usage: ./" << argv[0] << " -seed <SEED> ";
            std::cout << " -iters <NUM_ITERS> -rng <RNG> -dump_state <FILENAME>" << std::endl;
            return 1;
        }
        for (size_t i = 0; i < (size_t)argc; i++) {
            if (strcmp("-seed", argv[i]) == 0) {
                assert(i + 1 < (size_t)argc);
                saved_errno = errno;
                errno = 0;
                seed = (uint32_t) strtol(argv[i+1], &endptr, 0);
                if (errno == ERANGE || errno == EINVAL || endptr == argv[i+1]) {
                    std::cout << "Error, unable to parse " << argv[i+1] << " as new seed" << std::endl;
                    errno = saved_errno;
                    return 1;
                }
                i++;
            }
            if (strcmp("-iters", argv[i]) == 0) {
                assert(i + 1 < (size_t)argc);
                saved_errno = errno;
                errno = 0;
                niters = (size_t) strtol(argv[i+1], &endptr, 0);
                if (errno == ERANGE || errno == EINVAL || endptr == argv[i+1]) {
                    std::cout << "Error, unable to parse " << argv[i+1] << " as number of iterations" << std::endl;
                    errno = saved_errno;
                    return 1;
                }
                i++;
            }
            if (strcmp("-rng", argv[i]) == 0) {
                assert(i + 1 < (size_t)argc);
                rngStr = (std::string) argv[i+1];
                i++;
            }
            if (strcmp("-dump_state", argv[i]) == 0) {
                assert(i + 1 < (size_t)argc);
                dumpStateFilename = (std::string) argv[i+1];
                dumpState = true;
                i++;
            }
        }
    }
    std::unique_ptr<RNGBase> rng = RNGFactory::createRNG(rngStr);
    if (!rng) {
        std::cout << "Failed to instantiate RNG instance!" << std::endl;
        return 1;
    }
    std::cout << "Successfully initialized RNG: " << rng->name() << std::endl;

    rng->seed_random(seed);

    if (dumpState) {
        rng->dump_state(dumpStateFilename);
    }

    uint32_t rnd;
    for (size_t i = 0; i < niters; i++) {
        rnd = rng->read_random();
        std::cout << "Rand Value = " << rnd << std::endl;
    }
	return 0;
}
