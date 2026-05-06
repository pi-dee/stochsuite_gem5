#ifndef __XO_SHI_RO_128_PLUS_PLUS__
#define __XO_SHI_RO_128_PLUS_PLUS__

#include "RandomNumberGenerator.hpp"

#include <stdint.h>
#include <cstddef>

// Implemented from https://prng.di.unimi.it/xoshiro128plusplus.c
class XoShiRo128PlusPlus: public RNGBase {
    private:
        static inline uint32_t rotl(const uint32_t x, int k) {
            return (x << k) | (x >> (32 - k));
        }
    public:
        XoShiRo128PlusPlus();
        XoShiRo128PlusPlus(uint32_t S0, uint32_t S1,
                uint32_t S2, uint32_t S3);
        uint32_t _read_random() override;
        void _seed_random(uint32_t new_seed) override;
        std::string name() override;
};
#endif // __XO_SHI_RO_128_PLUS_PLUS__
