#ifndef __KEEP_IT_SIMPLE_STUPID_11__
#define __KEEP_IT_SIMPLE_STUPID_11__

#include "RandomNumberGenerator.hpp"

#include <stdint.h>
#include <cstddef>

// Implementation based on
// KISS: A Bit Too Simple
// https://eprint.iacr.org/2011/007.pdf

class KISS11: public RNGBase {
    public:
        KISS11();
        KISS11(uint32_t *seeds, size_t numWords);
        uint32_t _read_random() override;
        void _seed_random(uint32_t new_seed) override;
        std::string name() override;
        static const uint32_t N = 4194304; // Num words in anynonymous state vector
};
#endif // __J_KEEP_IT_SIMPLE_STUPID__
