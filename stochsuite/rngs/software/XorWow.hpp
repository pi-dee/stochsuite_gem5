#ifndef __XOR_WOW__
#define __XOR_WOW__

#include "RandomNumberGenerator.hpp"

#include <stdint.h>
#include <cstddef>

class XorWow: public RNGBase {
    public:
        XorWow();
        XorWow(uint32_t S1, uint32_t S2,
            uint32_t S3, uint32_t S4, uint32_t S5,
            uint32_t S6);
        uint32_t _read_random() override;
        void _seed_random(uint32_t new_seed) override;
        std::string name() override;
};
#endif // __XOR_WOW__
