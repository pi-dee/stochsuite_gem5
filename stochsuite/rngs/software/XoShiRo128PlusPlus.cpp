#include "XoShiRo128PlusPlus.hpp"

#include <cassert>

XoShiRo128PlusPlus::XoShiRo128PlusPlus() : RNGBase(16) {
    state->set_state_bytes_from_int(123456789, 4, 0);
    state->set_state_bytes_from_int(362436069, 4, 4);
    state->set_state_bytes_from_int(521288629, 4, 8);
    state->set_state_bytes_from_int(88675123, 4, 12);
}

XoShiRo128PlusPlus::XoShiRo128PlusPlus(uint32_t S0, uint32_t S1,
    uint32_t S2, uint32_t S3) : RNGBase(16) {
    // Seeding rule:
    // "The state must be seeded so that it is not everywhere zero."
    assert(S0 != 0);
    assert(S1 != 0);
    assert(S2 != 0);
    assert(S3 != 0);
    state->set_state_bytes_from_int(S0, 4, 0);
    state->set_state_bytes_from_int(S1, 4, 4);
    state->set_state_bytes_from_int(S2, 4, 8);
    state->set_state_bytes_from_int(S3, 4, 12);
}

void XoShiRo128PlusPlus::_seed_random(uint32_t new_seed) {
    // Seeding rules prohibit a seed of 0
    assert(new_seed != 0);
    state->set_state_bytes_from_int(new_seed, 4, 0);
}

std::string XoShiRo128PlusPlus::name() {
    return "XoShiRo128++";
}

uint32_t XoShiRo128PlusPlus::_read_random(){
    uint32_t s0 = state->get_state_bytes_as_int(0);
    uint32_t s1 = state->get_state_bytes_as_int(4);
    uint32_t s2 = state->get_state_bytes_as_int(8);
    uint32_t s3 = state->get_state_bytes_as_int(12);

    const uint32_t result = rotl(s0 + s3, 7) + s0;

	const uint32_t t = s1 << 9;

	s2 ^= s0;
	s3 ^= s1;
	s1 ^= s2;
	s0 ^= s3;

	s2 ^= t;

	s3 = rotl(s3, 11);

    state->set_state_bytes_from_int(s0, 4, 0);
    state->set_state_bytes_from_int(s1, 4, 4);
    state->set_state_bytes_from_int(s2, 4, 8);
    state->set_state_bytes_from_int(s3, 4, 12);
	return result;
}
