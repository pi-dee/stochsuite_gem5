#ifndef __HARDWARE_RANDOM_NUMBER_GENERATOR__
#define __HARDWARE_RANDOM_NUMBER_GENERATOR__

#include "RandomNumberGenerator.hpp"

#include <stdint.h>
#include <cstddef>

class HWRNG: public RNGBase {
    public:
        HWRNG();
        uint32_t _read_random() override;
        void _seed_random(uint32_t new_seed) override;
        std::string name() override;
    private:
        std::string _name;
};
#endif // __HARDWARE_RANDOM_NUMBER_GENERATOR__
