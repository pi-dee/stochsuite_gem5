#include "LCG.hpp"

LCG::LCG() : RNGBase(4) {
    _mul = 69069;
    _add = 1234567;
    _name = "LCG";
    _mod = 0xFFFFFFFF;
    state->set_state_bytes_from_int(1634404289, 4, 0);
}

LCG::LCG(uint32_t seed, uint64_t multiplier, uint32_t additive,
    uint32_t modulus, std::string name) : RNGBase(4) {
    _mul = multiplier;
    _add = additive;
    _name = name;
    _mod = modulus;
    state->set_state_bytes_from_int(seed, 4, 0);
}

void LCG::_seed_random(uint32_t new_seed) {
    state->set_state_bytes_from_int(new_seed, 4, 0);
}

uint32_t LCG::MAX() {
    return _mod;
}

std::string LCG::name() {
    return _name;
}

uint32_t LCG::_read_random(){
    uint32_t s = state->get_state_bytes_as_int(0);
    uint64_t internal = (_mul * s) + _add;
    s = internal & _mod;
    state->set_state_bytes_from_int(s, 4, 0);
    return s;
}
