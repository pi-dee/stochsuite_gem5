#ifndef __TAUSWORTHE_88__
#define __TAUSWORTHE_88__

#include "RandomNumberGenerator.hpp"

#include <stdint.h>
#include <cstddef>

class Taus88: public RNGBase {
    public:
        Taus88();
        Taus88(uint32_t S1, uint32_t S2, uint32_t S3);
        uint32_t _read_random() override;
        void _seed_random(uint32_t new_seed) override;
        std::string name() override;

    private:
        // Constants
        static const uint32_t C1 = 0xFFFFFFFE;
        static const uint32_t C2 = 0xFFFFFFF8;
        static const uint32_t C3 = 0xFFFFFFF0;
};
#endif // __TAUSWORTHE_88__
