#include "JKISS.hpp"

JKISS::JKISS() : RNGBase(16) {
    state->set_state_bytes_from_int(123456789, 4, 0);
    state->set_state_bytes_from_int(987654321, 4, 4);
    state->set_state_bytes_from_int(43219876, 4, 8);
    state->set_state_bytes_from_int(6543217, 4, 12);
}

JKISS::JKISS(uint32_t x, uint32_t y, uint32_t z, uint32_t c) : RNGBase(16) {
    state->set_state_bytes_from_int(x, 4, 0);
    state->set_state_bytes_from_int(y, 4, 4);
    state->set_state_bytes_from_int(z, 4, 8);
    state->set_state_bytes_from_int(c, 4, 12);
}

void JKISS::_seed_random(uint32_t new_seed) {
    state->set_state_bytes_from_int(new_seed, 4, 0);
}

std::string JKISS::name() {
    return "JKISS";
}

uint32_t JKISS::_read_random(){
    // Though t is a 64 bit value, it is not considered part
    // of the internal state, as it's merely an intermediate
    // value produced when producing a stochastic value.
    uint64_t t;
    uint32_t x = state->get_state_bytes_as_int(0);
    uint32_t y = state->get_state_bytes_as_int(4);
    uint32_t z = state->get_state_bytes_as_int(8);
    uint32_t c = state->get_state_bytes_as_int(12);
    x = 314527869 * x + 1234567;
    y ^= y << 5;
    y ^= y >> 7;
    y ^= y << 22;
    t = 4294584393ULL * z + c;
    c = t >> 32;
    z = t;
    state->set_state_bytes_from_int(x, 4, 0);
    state->set_state_bytes_from_int(y, 4, 4);
    state->set_state_bytes_from_int(z, 4, 8);
    state->set_state_bytes_from_int(c, 4, 12);
    return (x + y + z);
}
