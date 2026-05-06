#ifndef __XOR_SHIFT_32__
#define __XOR_SHIFT_32__

#include "RandomNumberGenerator.hpp"

#include <stdint.h>
#include <cstddef>
#include <set>
#include <tuple>
#include <memory>

using ABCSet = std::set<std::tuple<uint32_t, uint32_t, uint32_t>>;
using ABCLLLSet = std::set<std::tuple<uint32_t, uint32_t, uint32_t, bool, bool, bool>>;

class XorShift32: public RNGBase {
    public:
        XorShift32();
        XorShift32(uint32_t set_a, uint32_t set_b, uint32_t set_c, bool shift_a_left, bool shift_b_left, bool shift_c_left);
        XorShift32(uint32_t seed);
        XorShift32(uint32_t seed, uint32_t set_a, uint32_t set_b, uint32_t set_c, bool shift_a_left, bool shift_b_left, bool shift_c_left);
        uint32_t _read_random() override;
        void _seed_random(uint32_t new_seed) override;
        std::string name() override;

    private:
        void _init_valid_configs();
        void _cleanup_valid_configs();
        bool _test_config_valid(uint32_t test_a, uint32_t test_b, uint32_t test_c, bool test_left_a, bool test_left_b, bool test_left_c);

        uint32_t a, b, c;
        ABCSet* ABCSetPtr;
        bool left_a, left_b, left_c;
        ABCLLLSet* ABCLLLSetPtr;
};
#endif // __XOR_SHIFT_32__
