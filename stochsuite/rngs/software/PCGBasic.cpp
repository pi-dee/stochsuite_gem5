#include "PCGBasic.hpp"

PCGBasic::PCGBasic() : RNGBase(16) {
    // Set state_reg
    state->set_state_bytes_from_long(0x853c49e6748fea9bULL, 8, 0);
    // Set inc_reg
    state->set_state_bytes_from_long(0xda3e39cb94b95bdbULL, 8, 8);
}

PCGBasic::PCGBasic(uint64_t new_seed, uint64_t new_inc) : RNGBase(16) {
    // Set state_reg
    state->set_state_bytes_from_long(new_seed, 8, 0);
    // Set inc_reg
    state->set_state_bytes_from_long(new_inc, 8, 8);
}

void PCGBasic::_seed_random(uint32_t new_seed) {
    state->set_state_bytes_from_int(new_seed, 4, 0);
}

std::string PCGBasic::name() {
    return "PCG_Basic";
}

uint32_t PCGBasic::_read_random(){
    uint64_t oldstate = state->get_state_bytes_as_long(0);
    uint64_t oldinc   = state->get_state_bytes_as_long(8);
    uint64_t newstate = oldstate * 6364136223846793005ULL + oldinc;
    uint32_t xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
    uint32_t rot = oldstate >> 59u;

    // Set state_reg
    state->set_state_bytes_from_long(newstate, 8, 0);
    // Apprently, we don't need to modify the inc???
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}
