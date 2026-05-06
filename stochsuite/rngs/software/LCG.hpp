#ifndef __LINEAR_CONGRUENTIAL_GENERATOR__
#define __LINEAR_CONGRUENTIAL_GENERATOR__

#include "RandomNumberGenerator.hpp"

#include <stdint.h>
#include <cstddef>

class LCG: public RNGBase {
    public:
        LCG();
        LCG(uint32_t seed, uint64_t multiplier, uint32_t additive,
            uint32_t modulus, std::string name);
        uint32_t _read_random() override;
        void _seed_random(uint32_t new_seed) override;
        std::string name() override;
        uint32_t MAX() override;
    private:
        uint64_t _mul;
        uint32_t _add;
        uint32_t _mod;
        std::string _name;
};
#endif // __LINEAR_CONGRUENTIAL_GENERATOR__
