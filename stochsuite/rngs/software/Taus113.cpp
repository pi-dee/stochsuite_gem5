#include "Taus113.hpp"

Taus113::Taus113() : RNGBase(16) {
    state->set_state_bytes_from_int(16289, 4, 0);
    state->set_state_bytes_from_int(8, 4, 4);
    state->set_state_bytes_from_int(16, 4, 8);
    state->set_state_bytes_from_int(128, 4, 12);
}

Taus113::Taus113(uint32_t S1, uint32_t S2, uint32_t S3, uint32_t S4)
    : RNGBase(16) {
    state->set_state_bytes_from_int(S1, 4, 0);
    state->set_state_bytes_from_int(S2, 4, 4);
    state->set_state_bytes_from_int(S3, 4, 8);
    state->set_state_bytes_from_int(S4, 4, 12);
}

void Taus113::_seed_random(uint32_t new_seed) {
    state->set_state_bytes_from_int(new_seed, 4, 0);
}

std::string Taus113::name() {
    return "Taus113";
}

uint32_t Taus113::_read_random(){
    uint32_t S1 = state->get_state_bytes_as_int(0);
    uint32_t S2 = state->get_state_bytes_as_int(4);
    uint32_t S3 = state->get_state_bytes_as_int(8);
    uint32_t S4 = state->get_state_bytes_as_int(12);
    S1 = ( ( S1 & Taus113::C1 ) << 18 ) ^ ( ( ( S1 <<  6 ) ^ S1 ) >> 13);
    S2 = ( ( S2 & Taus113::C2 ) <<  2 ) ^ ( ( ( S2 <<  2 ) ^ S2 ) >> 27);
    S3 = ( ( S3 & Taus113::C3 ) <<  7 ) ^ ( ( ( S3 << 13 ) ^ S3 ) >> 21);
    S4 = ( ( S4 & Taus113::C4 ) << 13 ) ^ ( ( ( S4 <<  3 ) ^ S4 ) >> 12);
    state->set_state_bytes_from_int(S1, 4, 0);
    state->set_state_bytes_from_int(S2, 4, 4);
    state->set_state_bytes_from_int(S3, 4, 8);
    state->set_state_bytes_from_int(S4, 4, 12);
    return (S1 ^ S2 ^ S3 ^ S4);
}
