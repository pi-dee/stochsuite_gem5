#include "XorWow.hpp"

#include <cassert>

XorWow::XorWow() : RNGBase(24) {
    state->set_state_bytes_from_int(123456789, 4, 0);
    state->set_state_bytes_from_int(362436069, 4, 4);
    state->set_state_bytes_from_int(521288629, 4, 8);
    state->set_state_bytes_from_int(88675123, 4, 12);
    state->set_state_bytes_from_int(5783321, 4, 16);
    state->set_state_bytes_from_int(6615241, 4, 20);
}

XorWow::XorWow(uint32_t S1, uint32_t S2,
    uint32_t S3, uint32_t S4, uint32_t S5,
    uint32_t S6) : RNGBase(24) {
    // Seeding rules prohibit a seed of 0
    assert(S1 != 0);
    assert(S2 != 0);
    assert(S3 != 0);
    assert(S4 != 0);
    assert(S5 != 0);
    assert(S6 != 0);
    state->set_state_bytes_from_int(S1, 4, 0);
    state->set_state_bytes_from_int(S2, 4, 4);
    state->set_state_bytes_from_int(S3, 4, 8);
    state->set_state_bytes_from_int(S4, 4, 12);
    state->set_state_bytes_from_int(S5, 4, 16);
    state->set_state_bytes_from_int(S6, 4, 20);
}

void XorWow::_seed_random(uint32_t new_seed) {
    // Seeding rules prohibit a seed of 0
    assert(new_seed != 0);
    state->set_state_bytes_from_int(new_seed, 4, 0);
}

std::string XorWow::name() {
    return "XorWow";
}

uint32_t XorWow::_read_random(){
    uint32_t x = state->get_state_bytes_as_int(0);
    uint32_t y = state->get_state_bytes_as_int(4);
    uint32_t z = state->get_state_bytes_as_int(8);
    uint32_t w = state->get_state_bytes_as_int(12);
    uint32_t v = state->get_state_bytes_as_int(16);
    uint32_t d = state->get_state_bytes_as_int(20);
    uint32_t t;
    t = (x^(x>>2));
    x=y;
    y=z;
    z=w;
    w=v;
    v=(v^(v<<4))^(t^(t<<1));
    d += 362437;

    state->set_state_bytes_from_int(x, 4, 0);
    state->set_state_bytes_from_int(y, 4, 4);
    state->set_state_bytes_from_int(z, 4, 8);
    state->set_state_bytes_from_int(w, 4, 12);
    state->set_state_bytes_from_int(v, 4, 16);
    state->set_state_bytes_from_int(d, 4, 20);
    return d+v;
}
