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

    size_t niters = 10000;
    uint32_t seed = 12312332;
    std::string rngStr = "Taus88";
    size_t inputs = 1000;
    double dropProb = 0.5;

    if (argc > 1) {
        if (argc > 11) {
            std::cout << "usage: ./" << argv[0] << " -seed <SEED> ";
            std::cout << " -iters <NUM_ITERS> -rng <RNG> -inputs <NUM_INPUTS>";
            std::cout << " -prob <DROPOUT_PROBABILITY>" << std::endl;
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
            if (strcmp("-inputs", argv[i]) == 0) {
                assert(i + 1 < (size_t)argc);
                inputs = (size_t) strtol(argv[i+1], NULL, 10);
                i++;
            }
            if (strcmp("-prob", argv[i]) == 0) {
                assert(i + 1 < (size_t)argc);
                dropProb = (double) strtod(argv[i+1], NULL);
                i++;
            }
        }
    }
    assert(0.0 <= dropProb && dropProb <= 1.0);

    std::unique_ptr<RNGBase> rng = RNGFactory::createRNG(rngStr);
    if (!rng) {
        std::cout << "Failed to instantiate RNG instance!" << std::endl;
        return 1;
    }
    std::cout << "Successfully initialized RNG: " << rng->name() << std::endl;

    rng->seed_random(seed);

    float *layer = new float[inputs];
    for (size_t i = 0; i < inputs; i++) {
        layer[i] = 3.14;
    }

    double rnd, resultantProb;
    size_t count = 0;
    map_m5_mem();
    m5_work_begin_addr(0, 0);
    for (size_t i = 0; i < niters; i++) {
        for (size_t j = 0; j < inputs; j++) {
            rnd = rng->read_random_double();
            if (rnd < dropProb) {
                layer[j] = 0.0;
                count++;
            } else {
                // scaled by 1/(1-p) to maintain the expected value
                layer[j] /= (1.0 - dropProb);
            }
        }
        for (size_t j = 0; j < inputs; j++) {
            layer[j] = 3.14;
        }
    }
    m5_work_end_addr(0, 0);
    resultantProb = ((double)count) / ((double)niters*inputs);
    std::cout << "Dropped Outputs = " << resultantProb << std::endl;
    delete[] layer;
	return 0;
}
