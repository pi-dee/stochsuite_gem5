#ifndef __XOR_SHIFT_128__
#define __XOR_SHIFT_128__

#include "RandomNumberGenerator.hpp"

#include <stdint.h>
#include <cstddef>

class XorShift128: public RNGBase {
    public:
        XorShift128();
        XorShift128(uint32_t S1, uint32_t S2,
            uint32_t S3, uint32_t S4);
        uint32_t _read_random() override;
        void _seed_random(uint32_t new_seed) override;
        std::string name() override;
};
#endif // __XOR_SHIFT_128__
