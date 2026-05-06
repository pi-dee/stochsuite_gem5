#include "XorShift128.hpp"

#include <cassert>

XorShift128::XorShift128() : RNGBase(16) {
    state->set_state_bytes_from_int(123456789, 4, 0);
    state->set_state_bytes_from_int(362436069, 4, 4);
    state->set_state_bytes_from_int(521288629, 4, 8);
    state->set_state_bytes_from_int(88675123, 4, 12);
}

XorShift128::XorShift128(uint32_t S1, uint32_t S2,
    uint32_t S3, uint32_t S4) : RNGBase(16) {
    // Seeding rules prohibit a seed of 0
    assert(S1 != 0);
    assert(S2 != 0);
    assert(S3 != 0);
    assert(S4 != 0);
    state->set_state_bytes_from_int(S1, 4, 0);
    state->set_state_bytes_from_int(S2, 4, 4);
    state->set_state_bytes_from_int(S3, 4, 8);
    state->set_state_bytes_from_int(S4, 4, 12);
}

void XorShift128::_seed_random(uint32_t new_seed) {
    // Seeding rules prohibit a seed of 0
    assert(new_seed != 0);
    state->set_state_bytes_from_int(new_seed, 4, 0);
}

std::string XorShift128::name() {
    return "XorShift128";
}

uint32_t XorShift128::_read_random(){
    uint32_t x = state->get_state_bytes_as_int(0);
    uint32_t y = state->get_state_bytes_as_int(4);
    uint32_t z = state->get_state_bytes_as_int(8);
    uint32_t w = state->get_state_bytes_as_int(12);
    uint32_t t;
    t = (x ^ (x<<11));
    t ^= t >> 8;
    x=y;
    y=z;
    z=w;
    w = t ^ w ^ (w >> 19);
    state->set_state_bytes_from_int(x, 4, 0);
    state->set_state_bytes_from_int(y, 4, 4);
    state->set_state_bytes_from_int(z, 4, 8);
    state->set_state_bytes_from_int(w, 4, 12);
    return w;
}
