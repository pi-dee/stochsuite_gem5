#include "RNGFactory.hpp"
#include "RandomNumberGenerator.hpp"
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>
#include <gem5/m5ops.h>
#include "m5_mmap.h"

void normalize(double *layer, bool *used, size_t len) {
    double sum = 0.0;
    for (size_t i = 0; i < len; i++) {
        if (!used[i]) {
            sum += layer[i];
        }
    }
    for (size_t i = 0; i < len; i++) {
        if (!used[i]) {
            layer[i] /= sum;
        }
    }
}

void softmax_reset(double *layer, bool *used, size_t len,
    const std::unique_ptr<RNGBase>& rng) {

    for (size_t i = 0; i < len; i++) {
        layer[i] = rng->read_random_double();
        used[i] = false;
    }
    normalize(layer, used, len);

}

int main(int argc, char *argv[]) {

    size_t niters = 10000;
    uint32_t seed = 12312332;
    std::string rngStr = "Taus88";
    size_t inputs = 1000;
    size_t outputs = 1000;
    bool without_replacement = false;

    if (argc > 1) {
        if (argc > 11) {
            std::cout << "usage: ./" << argv[0] << " -seed <SEED> ";
            std::cout << " -iters <NUM_ITERS> -rng <RNG>";
            std::cout << " -inputs <NUM_INPUTS>";
            std::cout << " -outputs <NUM_INPUTS>" << std::endl;
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
            if (strcmp("-outputs", argv[i]) == 0) {
                assert(i + 1 < (size_t)argc);
                outputs = (size_t) strtol(argv[i+1], NULL, 10);
                i++;
            }
            if (strcmp("-without_replacement", argv[i]) == 0) {
                without_replacement = true;
            }
        }
    }
    if (without_replacement) {
        assert(outputs <= inputs);
    }

    std::unique_ptr<RNGBase> rng = RNGFactory::createRNG(rngStr);
    if (!rng) {
        std::cout << "Failed to instantiate RNG instance!" << std::endl;
        return 1;
    }
    std::cout << "Successfully initialized RNG: " << rng->name() << std::endl;

    rng->seed_random(seed);

    // Fake output from a softmax layer
    double *layer = new double[inputs];
    // Keep track of used indicies if sampling without replacement
    bool *used = new bool[inputs];
    // Storage for results
    double *results = new double[outputs];

    softmax_reset(layer, used, inputs, rng);

    double rnd;
    double cdf = 0.0;
    map_m5_mem();
    if (!without_replacement) {
        m5_work_begin_addr(0, 0);
        for (size_t i = 0; i < niters; i++) {
            for (size_t j = 0; j < outputs; j++) {
                rnd = rng->read_random_double();
                for (size_t k = 0; k < inputs; k++) {
                    if (rnd < cdf) {
                        cdf += layer[i];
                    } else {
                        results[j] = k;
                        break;
                    }
                }
            }
        }
        m5_work_end_addr(0, 0);
    } else {
        // When sampling without replacement, we must renormalize after
        // every draw to re-comput the cdf
        m5_work_begin_addr(0, 0);
        for (size_t i = 0; i < niters; i++) {
            for (size_t j = 0; j < outputs; j++) {
                rnd = rng->read_random_double();
                for (size_t k = 0; k < inputs; k++) {
                    if (used[k]) {
                        // If we have already drawn a sample for this
                        // index, we can just skip to the next
                        continue;
                    }
                    if (rnd < cdf) {
                        cdf += layer[i];
                    } else {
                        results[j] = k;
                        used[k] = true;
                        normalize(layer, used, inputs);
                        break;
                    }
                }
            }
            softmax_reset(layer, used, inputs, rng);
        }
        m5_work_end_addr(0, 0);
    }
    delete[] layer;
    delete[] used;
    delete[] results;
	return 0;
}
