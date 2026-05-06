#ifndef __TAUSWORTHE_113__
#define __TAUSWORTHE_113__

#include "RandomNumberGenerator.hpp"

#include <stdint.h>
#include <cstddef>

class Taus113: public RNGBase {
    public:
        Taus113();
        Taus113(uint32_t S1, uint32_t S2, uint32_t S3, uint32_t S4);
        uint32_t _read_random() override;
        void _seed_random(uint32_t new_seed) override;
        std::string name() override;

    private:
        // Constants
        static const uint32_t C1 = 4294967294U;
        static const uint32_t C2 = 4294967288U;
        static const uint32_t C3 = 4294967280U;
        static const uint32_t C4 = 4294967168U;
};
#endif // __TAUSWORTHE_113__
