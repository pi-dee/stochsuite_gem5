#include "JKISS32.hpp"

JKISS32::JKISS32() : RNGBase(17) {
    state->set_state_bytes_from_int(123456789, 4, 0);
    state->set_state_bytes_from_int(987654321, 4, 4);
    state->set_state_bytes_from_int(43219876, 4, 8);
    state->set_state_bytes_from_int(6543217, 4, 12);
    state->set_state_byte(0, 16);
}

JKISS32::JKISS32(uint32_t x, uint32_t y, uint32_t z, uint32_t w, uint8_t c) : RNGBase(17) {
    state->set_state_bytes_from_int(x, 4, 0);
    state->set_state_bytes_from_int(y, 4, 4);
    state->set_state_bytes_from_int(z, 4, 8);
    state->set_state_bytes_from_int(w, 4, 12);
    state->set_state_byte(c, 16);
}

void JKISS32::_seed_random(uint32_t new_seed) {
    state->set_state_bytes_from_int(new_seed, 4, 0);
}

std::string JKISS32::name() {
    return "JKISS32";
}

uint32_t JKISS32::_read_random(){
    uint32_t t;
    uint32_t x = state->get_state_bytes_as_int(0);
    uint32_t y = state->get_state_bytes_as_int(4);
    uint32_t z = state->get_state_bytes_as_int(8);
    uint32_t w = state->get_state_bytes_as_int(12);
    uint8_t  c = state->get_state_byte(16);

    y ^= (y<<5);
    y ^= (y>>7);
    y ^= (y<<22);
    t = z+w+c;
    z = w;
    c = t < 0;
    w = t&2147483647;
    x += 1411392427;

    state->set_state_bytes_from_int(x, 4, 0);
    state->set_state_bytes_from_int(y, 4, 4);
    state->set_state_bytes_from_int(z, 4, 8);
    state->set_state_bytes_from_int(w, 4, 12);
    state->set_state_byte(c, 16);
    return (x + y + z);
}
