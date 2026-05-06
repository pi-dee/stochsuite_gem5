#ifndef __PERMUTED_CONGRUENTIAL_GENERATOR_BASIC__
#define __PERMUTED_CONGRUENTIAL_GENERATOR_BASIC__

#include "RandomNumberGenerator.hpp"

#include <stdint.h>
#include <cstddef>

class PCGBasic: public RNGBase {
    public:
        PCGBasic();
        PCGBasic(uint64_t new_seed, uint64_t new_inc);
        uint32_t _read_random() override;
        void _seed_random(uint32_t new_seed) override;
        std::string name() override;
};
#endif // __PERMUTED_CONGRUENTIAL_GENERATOR_BASIC__
