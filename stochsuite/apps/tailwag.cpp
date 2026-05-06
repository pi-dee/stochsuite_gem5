#include "RNGFactory.hpp"
#include "RandomNumberGenerator.hpp"

#include <iostream>
#include <cstring>
#include <cassert>
#include <cmath>
#include <gem5/m5ops.h>
#include "m5_mmap.h"

int main(int argc, char* argv[]) {
    size_t niters = 100;
    uint32_t seed = 12312332;
    std::string rngStr = "Taus88";
    uint32_t dist2P = 0;
    uint32_t dist3P = 0;

    if (argc > 1) {
        if (argc > 7) {
            std::cout << "usage: ./" << argv[0] << " -seed <SEED> ";
            std::cout << " -iters <ITERS>";
            std::cout << " -rng <RNG>";
            std::cout << " -dist2P <dist2P>";
            std::cout << " -dist3P <dist3P>" << std::endl;
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
            if (strcmp("-dist2P", argv[i]) == 0) {
                assert(i + 1 < (size_t)argc);
                dist2P = (uint32_t) strtol(argv[i+1], NULL, 10);
                i++;
            }
            if (strcmp("-dist3P", argv[i]) == 0) {
                assert(i + 1 < (size_t)argc);
                dist3P = (uint32_t) strtol(argv[i+1], NULL, 10);
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

    size_t block1Cnt = 0;
    size_t freeCnt = 0;
    size_t block2Cnt = 0;
    uint64_t virt_freq = 4091986000000;
    double dist1T = 10.0;
    double dist1V = 1.0;
    double dist2T = 1000.0;
    double dist2V = 1.00;
    double dist3T = 1000.0;
    double dist3V = 1.0;
    double block1T = 10.00;
    double block2T = 10.00;
    double noiseT = 10.00;
    double noiseP = 1.00;

    double dist1Ave = virt_freq*dist1T/1e+6;
    double dist2Ave = virt_freq*dist2T/1e+6;
    double dist3Ave = virt_freq*dist3T/1e+6;

    double block1Ave = virt_freq*block1T/1e+6;
    double block2Ave = virt_freq*block2T/1e+6;

    double noiseAve = virt_freq*noiseT/1e+6;

    uint64_t block1Cycle, block2Cycle, noiseCycle, distCycle;

    noiseCycle =  std::llround(noiseAve);
    block1Cycle = std::llround(block1Ave);
    block2Cycle = std::llround(block2Ave);

    uint32_t distNum, noiseNum;

    map_m5_mem();
    m5_work_begin_addr(0, 0);
    for (size_t i = 0; i < niters; i++) {
        distNum = rng->read_random_range(0, 999999, 10000000);
        if (block1Ave) {
            for (size_t b1 = 0; b1 < block1Cycle; b1++) block1Cnt++;
        }

        //randomized change to add noise disturbance
        if(distNum < dist3P){
            distCycle = std::llround(rng->lognormal_distribution(std::log(dist3Ave),dist3V));
        } else if(distNum < (dist3P + dist2P) ){
            distCycle = std::llround(rng->lognormal_distribution(std::log(dist2Ave),dist2V));
        } else {
            distCycle = std::llround(rng->lognormal_distribution(std::log(dist1Ave),dist1V));
        }
        for (size_t dist = 0; dist < distCycle; dist++) freeCnt++;

        noiseNum = rng->read_random_range(0, 999999, 10000000);
        if(noiseNum < noiseP){
            for (size_t noise = 0; noise < noiseCycle; noise++) freeCnt++;
        }
        if (block2Ave) {
            for (size_t b2 = 0; b2 < block2Cycle; b2++) block2Cnt++;
        }
    }
    m5_work_end_addr(0, 0);

    size_t blocked = block1Cnt + block2Cnt;
    double blockPercentage = 100.0 * blocked / (blocked + freeCnt);
    std::cout << "Spent " << blockPercentage << "\% of server time blocked on I/O" << std::endl;
    return 0;
}
