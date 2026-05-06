#ifndef __J_KEEP_IT_SIMPLE_STUPID__
#define __J_KEEP_IT_SIMPLE_STUPID__

#include "RandomNumberGenerator.hpp"

#include <stdint.h>
#include <cstddef>

// Implementation based on
// http://www0.cs.ucl.ac.uk/staff/d.jones/GoodPracticeRNG.pdf

class JKISS: public RNGBase {
    public:
        JKISS();
        JKISS(uint32_t x, uint32_t y, uint32_t z, uint32_t c);
        uint32_t _read_random() override;
        void _seed_random(uint32_t new_seed) override;
        std::string name() override;
};
#endif // __J_KEEP_IT_SIMPLE_STUPID__
