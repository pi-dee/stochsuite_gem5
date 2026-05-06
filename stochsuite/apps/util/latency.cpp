#include "RNGFactory.hpp"
#include "RandomNumberGenerator.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <cmath>
#include <iostream>
#include <numeric>

#if defined(__x86_64__) || defined(__amd64__)
#include <x86intrin.h>
#endif

#if defined(__aarch64__)
#include <time.h>
#endif

#include <vector>

int main(int argc, char *argv[]) {
    size_t warmup_iters = 100;
    size_t niters = 100000000;
    size_t batch = 100; // should be a small factor of niters
    uint32_t seed = 1634404289;
    std::string rngStr = "Taus88";
    bool printHeader = false;

    if (argc > 1) {
        if (argc > 8) {
            std::cout << "usage: ./" << argv[0] << " ";
            std::cout << " -rng <RNG> -iters <ITERS>";
            std::cout << " -batch <BATCH_SIZE> -header" << std::endl;
            return 1;
        }
        for (size_t i = 0; i < (size_t)argc; i++) {
            if (strcmp("-rng", argv[i]) == 0) {
                assert(i + 1 < (size_t)argc);
                rngStr = (std::string) argv[i+1];
                i++;
            }
            if (strcmp("-iters", argv[i]) == 0) {
                assert(i + 1 < (size_t)argc);
                niters = (size_t) strtol(argv[i+1], NULL, 10);
                i++;
            }
            if (strcmp("-batch", argv[i]) == 0) {
                assert(i + 1 < (size_t)argc);
                batch = (size_t) strtol(argv[i+1], NULL, 10);
                i++;
            }
            if (strcmp("-header", argv[i]) == 0) {
                printHeader = true;
            }
        }
    }
    assert(niters % batch == 0);
    std::unique_ptr<RNGBase> rng = RNGFactory::createRNG(rngStr);
    if (!rng) {
        std::cout << "Failed to instantiate RNG instance!" << std::endl;
        return 1;
    }
#if defined(__x86_64__) || defined(__amd64__)
    uint64_t start_ticks, end_ticks;
#endif
#if defined(__aarch64__)
    struct timespec start_ts, end_ts;
#endif
    uint64_t elapsed;


    rng->seed_random(seed);
    uint32_t rnd;
    for (size_t i = 0; i < warmup_iters; i++) {
        rnd = rng->read_random();
    }
    std::vector<uint64_t> delays;
    delays.reserve(niters / batch);

    for (size_t i = 0; i < niters/batch; i++) {
#if defined(__x86_64__) || defined(__amd64__)
        start_ticks = __rdtsc();
#endif
#if defined(__aarch64__)
	// TODO save and restore errno
        if (clock_gettime(CLOCK_MONOTONIC, &start_ts) ) {
            std::cout << "Failed to read clocktime at start of ROI!" << std::endl;
            return 1;
	}
#endif
        for (size_t j = 0; j < batch; j++) {
            rnd = rng->read_random();
        }
#if defined(__x86_64__) || defined(__amd64__)
        end_ticks = __rdtsc();
	elapsed = end_ticks - start_ticks;
#endif
#if defined(__aarch64__)
	// TODO save and restore errno
        if (clock_gettime(CLOCK_MONOTONIC, &end_ts)) {
            std::cout << "Failed to read clocktime at end of ROI!" << std::endl;
            return 1;
	}
	// For ease of comparison, we should convert to
	// same time unit (e.g. seconds):
	elapsed = (end_ts.tv_sec - start_ts.tv_sec) * 1000000000ULL +
	                (end_ts.tv_nsec - start_ts.tv_nsec);
	// TODO need to do this for x86_64 machines as well
#endif
        delays.push_back(elapsed);
    }
    uint64_t sum = 0;
    for (size_t i = 0; i < delays.size(); i++) {
        sum += delays[i];
    }

    double mean = (((double)sum) / ((double)delays.size())) / batch;
    std::sort(delays.begin(), delays.end());
    double min = ((double)delays[0])/batch;
    double max = ((double)delays[delays.size() - 1])/batch;
    double median = delays[delays.size() / 2];
    if (delays.size() % 2 == 0) {
        // average of middle two elements
        median = (delays[delays.size() / 2 - 1] + median) / 2;
    }
    median = median / batch;
    double firstQuartile = delays[delays.size() / 4];
    if (delays.size() % 4 == 0) {
        // average of middle two elements
        firstQuartile = (delays[delays.size() / 4 - 1] + firstQuartile) / 2;
    }
    firstQuartile /= batch;
    double thirdQuartile = delays[3* delays.size() / 4];
    if (delays.size() % 4 == 0) {
        // average of middle two elements
        thirdQuartile = (delays[3*delays.size() / 4 - 1] + thirdQuartile) / 2;
    }
    thirdQuartile /= batch;

    double P99_9 = ((double)delays[floor(99.9 * delays.size() / 100.0)])/batch;
    double P99_0 = ((double)delays[floor(99.0 * delays.size() / 100.0)])/batch;
    double P95_0 = ((double)delays[floor(95.0 * delays.size() / 100.0)])/batch;
    double P90_0 = ((double)delays[floor(90.0 * delays.size() / 100.0)])/batch;
    double stddev = 0.0;
    for (size_t i = 0; i < delays.size(); i++) {
        stddev += std::pow((((double)delays[i])/batch) - mean, 2);
    }
    stddev = std::sqrt(stddev / delays.size());

    if (printHeader) {
        std::cout << "RNG,Mean,Median,Stddev,Min,Max,FirstQuartile,";
        std::cout << "ThirdQuartile,P99_9,P99_0,P95_0,P90_0,LastRand" << std::endl;
    }
    std::cout << rngStr << "," << mean << "," << median << "," << stddev << ",";
    std::cout << min << "," << max << "," << firstQuartile << ",";
    std::cout << thirdQuartile << "," << P99_9 << "," << P99_0 << ",";
    std::cout << P95_0 << "," << P90_0 << "," << rnd << std::endl;
	return 0;
}
