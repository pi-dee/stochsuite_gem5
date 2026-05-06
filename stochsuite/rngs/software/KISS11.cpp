#include "KISS11.hpp"

#include <cassert>

KISS11::KISS11() : RNGBase(4*KISS11::N + 4 + 4 + 4 + 4) {
    // Set CNG
    state->set_state_bytes_from_int(123456789, 4, 4*KISS11::N);
    // Set xs
    state->set_state_bytes_from_int(362436069, 4, 4*KISS11::N + 4);
    // Set j
    state->set_state_bytes_from_int(4194303, 4, 4*KISS11::N + 8);
    // Set carry
    state->set_state_bytes_from_int(0, 4, 4*KISS11::N + 12);
    for (size_t i = 0; i < KISS11::N; i++) {
        state->set_state_bytes_from_int(43219876, 4, 4*i);
    }
}

KISS11::KISS11(uint32_t *seeds, size_t numWords)
    : RNGBase(4*KISS11::N + 4 + 4 + 4 + 4) {
    assert(numWords <= KISS11::N + 4);
    for (size_t i = 0; i < numWords; i++) {
        state->set_state_bytes_from_int(seeds[i], 4, i*4);
    }

    // If user provide values for entire state, simply return
    if (numWords == KISS11::N + 3) {
        return;
    }

    for (size_t i = numWords; i < KISS11::N + 4; i++) {
        state->set_state_bytes_from_int(43219876, 4, i*4);
    }
    // Set carry explicitly to 0
    state->set_state_byte(0, 4*KISS11::N + 12);
}

void KISS11::_seed_random(uint32_t new_seed) {
    state->set_state_bytes_from_int(new_seed, 4, 0);
}

std::string KISS11::name() {
    return "KISS11";
}

uint32_t KISS11::_read_random(){
    uint32_t cng   = state->get_state_bytes_as_int(4*KISS11::N);
    uint32_t xs    = state->get_state_bytes_as_int(4*KISS11::N + 4);
    uint32_t j     = state->get_state_bytes_as_int(4*KISS11::N + 8);
    uint32_t carry = state->get_state_byte(4*KISS11::N + 12);
    uint32_t x, t, result;

    j = (j + 1) & KISS11::N;
    x = state->get_state_bytes_as_int(4*j);
    t = (x << 28) + carry;
    result = t - x + cng + xs;

    // State updates
    carry = (x >> 4) - (t < x);
    cng  = 69069 * cng + 13579;
    xs ^= (xs << 13);
    xs ^= (xs >> 17);
    xs ^= (xs <<  5);
    state->set_state_bytes_from_int(cng, 4, 4*KISS11::N);
    state->set_state_bytes_from_int(xs, 4, 4*KISS11::N + 4);
    state->set_state_bytes_from_int(j, 4, 4*KISS11::N + 8);
    state->set_state_bytes_from_int(carry, 4, 4*KISS11::N + 12);
    state->set_state_bytes_from_int(t - x, 4, 4*j); // Q[j] = t - x
    return result;
}
