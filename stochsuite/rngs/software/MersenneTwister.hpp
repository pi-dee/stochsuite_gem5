#ifndef __MERSENNE_TWISTER__
#define __MERSENNE_TWISTER__

#include "RandomNumberGenerator.hpp"

#include <stdint.h>
#include <cstddef>

class MersenneTwister: public RNGBase {
    public:
        MersenneTwister();
        MersenneTwister(uint32_t *seeds, size_t numWords);
        uint32_t _read_random() override;
        void _seed_random(uint32_t new_seed) override;
        std::string name() override;
        void reload();
        uint32_t hiBit(uint32_t u ) const { return u & 0x80000000UL; }
        uint32_t loBit(uint32_t u ) const { return u & 0x00000001UL; }
        uint32_t loBits(uint32_t u ) const { return u & 0x7fffffffUL; }
        uint32_t mixBits(uint32_t u, uint32_t v ) const
                { return hiBit(u) | loBits(v); }
        uint32_t twist(uint32_t m, uint32_t s0, uint32_t s1 ) const
                { return m ^ (mixBits(s0,s1)>>1) ^ (-loBit(s1) & 0x9908b0dfUL); }

        static const uint32_t N = 624; // Num words in state vector
        static const uint32_t M = 397; // Period parameter

    private:
        uint32_t nextRandWord;
};
#endif // __MERSENNE_TWISTER__
