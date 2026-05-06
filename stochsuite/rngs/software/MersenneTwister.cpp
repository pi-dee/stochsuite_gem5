#include "MersenneTwister.hpp"

#include <cassert>

MersenneTwister::MersenneTwister() : RNGBase(4*MersenneTwister::N) {
    state->set_state_bytes_from_int(19650218, 4, 0);
    uint32_t prev = state->get_state_bytes_as_int(0);
    uint32_t curr;
    for (size_t i = 1; i < MersenneTwister::N; i++) {
        curr = 1812433253UL * ( prev ^ (prev >> 30) ) + i;
        state->set_state_bytes_from_int(curr, 4, i*4);
        prev = curr;
    }
    nextRandWord = 0;
}

MersenneTwister::MersenneTwister(uint32_t *seeds, size_t numWords)
    : RNGBase(4*MersenneTwister::N) {

    assert(numWords <= MersenneTwister::N);
    for (size_t i = 0; i < numWords; i++) {
        state->set_state_bytes_from_int(seeds[i], 4, i*4);
    }

    // If user provide values for entire state, simply return
    if (numWords == MersenneTwister::N) {
        return;
    }

    // Some state bits not set, use default strategy to set seeds
    uint32_t prev = state->get_state_bytes_as_int(numWords - 1);
    uint32_t curr;
    for (size_t i = numWords; i < MersenneTwister::N; i++) {
        curr = 1812433253UL * ( prev ^ (prev >> 30) ) + i;
        state->set_state_bytes_from_int(curr, 4, i*4);
        prev = curr;
    }
    nextRandWord = 0;
}

void MersenneTwister::_seed_random(uint32_t new_seed) {
    state->set_state_bytes_from_int(new_seed, 4, 0);
}

std::string MersenneTwister::name() {
    return "mt19937";
}

void MersenneTwister::reload() {
    // Generate N new values in state
    // Made clearer and faster by Matthew Bellew (matthew.bellew@home.com)
    uint32_t p0, p1, pM, pMN, s0;
    size_t pNext = 0;
    for (size_t i = 0; i < MersenneTwister::N - MersenneTwister::M; i++) {
        p0 = state->get_state_bytes_as_int(pNext +   0);
        p1 = state->get_state_bytes_as_int(pNext +   1);
        pM = state->get_state_bytes_as_int(pNext + MersenneTwister::M);
        state->set_state_bytes_from_int(twist(pM, p0, p1), 4, pNext*4);
        pNext++;
    }
    for(size_t i = 0; i < MersenneTwister::M; i++) {
        p0  = state->get_state_bytes_as_int(pNext +   0);
        p1  = state->get_state_bytes_as_int(pNext +   1);
        pMN = state->get_state_bytes_as_int(pNext + MersenneTwister::M - MersenneTwister::N);
        state->set_state_bytes_from_int(twist(pMN, p0, p1), 4, pNext*4);
        pNext++;
    }
    p0  = state->get_state_bytes_as_int(pNext +   0);
    s0  = state->get_state_bytes_as_int(          0);
    pMN = state->get_state_bytes_as_int(pNext + MersenneTwister::M - MersenneTwister::N);
    state->set_state_bytes_from_int(twist(pMN, p0, s0), 4, (MersenneTwister::N-1)*4);

    nextRandWord = 0;
}

uint32_t MersenneTwister::_read_random() {
    if (nextRandWord == MersenneTwister::N) {
        reload();
    }

    uint32_t S1 = state->get_state_bytes_as_int(nextRandWord);
    S1 ^= (S1 >> 11);
    S1 ^= (S1 <<  7) & 0x9d2c5680UL;
    S1 ^= (S1 << 15) & 0xefc60000UL;
    nextRandWord++;
    return ( S1 ^ (S1 >> 18) );
}
