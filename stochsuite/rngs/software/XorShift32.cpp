#include "XorShift32.hpp"

#include <cassert>

void XorShift32::_init_valid_configs() {
    ABCSetPtr = new ABCSet();
    ABCSetPtr->insert(std::make_tuple(1, 3, 10));
    ABCSetPtr->insert(std::make_tuple(1, 5,16));
    ABCSetPtr->insert(std::make_tuple(1, 5,19));
    ABCSetPtr->insert(std::make_tuple(1, 9,29));
    ABCSetPtr->insert(std::make_tuple(1,11, 6));
    ABCSetPtr->insert(std::make_tuple(1,11,16));
    ABCSetPtr->insert(std::make_tuple(1,19, 3));
    ABCSetPtr->insert(std::make_tuple(1,21,20));
    ABCSetPtr->insert(std::make_tuple(1,27,27));
    ABCSetPtr->insert(std::make_tuple(2, 5,15 ));
    ABCSetPtr->insert(std::make_tuple(2, 5,21 ));
    ABCSetPtr->insert(std::make_tuple(2, 7, 7 ));
    ABCSetPtr->insert(std::make_tuple(2, 7, 9 ));
    ABCSetPtr->insert(std::make_tuple(2, 7,25 ));
    ABCSetPtr->insert(std::make_tuple(2, 9,15 ));
    ABCSetPtr->insert(std::make_tuple(2,15,17 ));
    ABCSetPtr->insert(std::make_tuple(2,15,25 ));
    ABCSetPtr->insert(std::make_tuple(2,21, 9));
    ABCSetPtr->insert(std::make_tuple(3, 1,14 ));
    ABCSetPtr->insert(std::make_tuple(3, 3,26 ));
    ABCSetPtr->insert(std::make_tuple(3, 3,28 ));
    ABCSetPtr->insert(std::make_tuple(3, 3,29 ));
    ABCSetPtr->insert(std::make_tuple(3, 5,20 ));
    ABCSetPtr->insert(std::make_tuple(3, 5,22 ));
    ABCSetPtr->insert(std::make_tuple(3, 5,25 ));
    ABCSetPtr->insert(std::make_tuple(3, 7,29 ));
    ABCSetPtr->insert(std::make_tuple(3,13, 7 ));
    ABCSetPtr->insert(std::make_tuple(3,23,25 ));
    ABCSetPtr->insert(std::make_tuple(3,25,24 ));
    ABCSetPtr->insert(std::make_tuple(3,27,11 ));
    ABCSetPtr->insert(std::make_tuple(4, 3,17 ));
    ABCSetPtr->insert(std::make_tuple(4, 3,27 ));
    ABCSetPtr->insert(std::make_tuple(4, 5,15 ));
    ABCSetPtr->insert(std::make_tuple(5, 3,21 ));
    ABCSetPtr->insert(std::make_tuple(5, 7,22 ));
    ABCSetPtr->insert(std::make_tuple(5, 9,7 ));
    ABCSetPtr->insert(std::make_tuple(5, 9,28 ));
    ABCSetPtr->insert(std::make_tuple(5, 9,31 ));
    ABCSetPtr->insert(std::make_tuple(5,13, 6 ));
    ABCSetPtr->insert(std::make_tuple(5,15,17 ));
    ABCSetPtr->insert(std::make_tuple(5,17,13 ));
    ABCSetPtr->insert(std::make_tuple(5,21,12 ));
    ABCSetPtr->insert(std::make_tuple(5,27, 8 ));
    ABCSetPtr->insert(std::make_tuple(5,27,21 ));
    ABCSetPtr->insert(std::make_tuple(5,27,25));
    ABCSetPtr->insert(std::make_tuple(5,27,28 ));
    ABCSetPtr->insert(std::make_tuple(6, 1,11 ));
    ABCSetPtr->insert(std::make_tuple(6, 3,17 ));
    ABCSetPtr->insert(std::make_tuple(6,17, 9 ));
    ABCSetPtr->insert(std::make_tuple(6,21, 7 ));
    ABCSetPtr->insert(std::make_tuple(6,21,13 ));
    ABCSetPtr->insert(std::make_tuple(7, 1, 9 ));
    ABCSetPtr->insert(std::make_tuple(7, 1,18 ));
    ABCSetPtr->insert(std::make_tuple(7, 1,25));
    ABCSetPtr->insert(std::make_tuple(7,13,25 ));
    ABCSetPtr->insert(std::make_tuple(7,17,21 ));
    ABCSetPtr->insert(std::make_tuple(7,25,12 ));
    ABCSetPtr->insert(std::make_tuple(7,25,20 ));
    ABCSetPtr->insert(std::make_tuple(8, 7,23 ));
    ABCSetPtr->insert(std::make_tuple(8,9,23  ));
    ABCSetPtr->insert(std::make_tuple(9, 5,1  )); // TODO something seems wrong, paper states a < c
    ABCSetPtr->insert(std::make_tuple(9, 5,25 ));
    ABCSetPtr->insert(std::make_tuple(9,11,19));
    ABCSetPtr->insert(std::make_tuple(9,21,16));
    ABCSetPtr->insert(std::make_tuple(10, 9,21));
    ABCSetPtr->insert(std::make_tuple(10, 9,25));
    ABCSetPtr->insert(std::make_tuple(11, 7,12));
    ABCSetPtr->insert(std::make_tuple(11, 7,16));
    ABCSetPtr->insert(std::make_tuple(11,17,13));
    ABCSetPtr->insert(std::make_tuple(11,21,13));
    ABCSetPtr->insert(std::make_tuple(12, 9,23));
    ABCSetPtr->insert(std::make_tuple(13, 3,17));
    ABCSetPtr->insert(std::make_tuple(13, 3,27));
    ABCSetPtr->insert(std::make_tuple(13, 5,19));
    ABCSetPtr->insert(std::make_tuple(13,17,15));
    ABCSetPtr->insert(std::make_tuple(14, 1,15));
    ABCSetPtr->insert(std::make_tuple(14,13,15));
    ABCSetPtr->insert(std::make_tuple(15, 1,29));
    ABCSetPtr->insert(std::make_tuple(17,15,20));
    ABCSetPtr->insert(std::make_tuple(17,15,23));
    ABCSetPtr->insert(std::make_tuple(17,15,26));

    ABCLLLSetPtr = new ABCLLLSet();
    for (std::tuple<uint32_t, uint32_t, uint32_t> abc : *ABCSetPtr) {
        uint32_t a_val = std::get<0>(abc);
        uint32_t b_val = std::get<1>(abc);
        uint32_t c_val = std::get<2>(abc);

        ABCLLLSetPtr->insert(std::make_tuple(a_val, b_val, c_val, true, false, true));
        ABCLLLSetPtr->insert(std::make_tuple(c_val, b_val, a_val, true, false, true));
        ABCLLLSetPtr->insert(std::make_tuple(a_val, b_val, c_val, false, true, false));
        ABCLLLSetPtr->insert(std::make_tuple(c_val, b_val, a_val, false, true, false));
        ABCLLLSetPtr->insert(std::make_tuple(a_val, c_val, b_val, true, true, false));
        ABCLLLSetPtr->insert(std::make_tuple(c_val, a_val, b_val, true, true, false));
        ABCLLLSetPtr->insert(std::make_tuple(a_val, c_val, b_val, false, false, true));
        ABCLLLSetPtr->insert(std::make_tuple(c_val, a_val, b_val, false, false, true));
    }
}

void XorShift32::_cleanup_valid_configs() {
    if (ABCSetPtr != nullptr) {
        delete ABCSetPtr;
        ABCSetPtr = nullptr;
    }
    if (ABCLLLSetPtr != nullptr) {
        delete ABCLLLSetPtr;
        ABCLLLSetPtr = nullptr;
    }
}

bool XorShift32::_test_config_valid(uint32_t a_test, uint32_t b_test, uint32_t c_test,
                                   bool left_a_test,  bool left_b_test,  bool left_c_test) {
    if (ABCLLLSetPtr->find(std::make_tuple(a_test, b_test, c_test, left_a_test, left_b_test, left_c_test)) != ABCLLLSetPtr->end()) {
        return true;
    }
    else {
        return false;
    }
}

XorShift32::XorShift32() : RNGBase(4) {
    state->set_state_bytes_from_int(2463534242, 4, 0);
    a = 13;
    b = 17;
    c = 5;
    left_a = true;
    left_b = false;
    left_c = true;
    _init_valid_configs();
    bool config_valid = _test_config_valid(a, b, c, left_a, left_b, left_c);
    _cleanup_valid_configs();

    assert(config_valid);
}

XorShift32::XorShift32(uint32_t set_a, uint32_t set_b, uint32_t set_c, bool shift_a_left, bool shift_b_left, bool shift_c_left) : RNGBase(4) {
    state->set_state_bytes_from_int(2463534242, 4, 0);

    _init_valid_configs();
    bool config_valid = _test_config_valid(set_a, set_b, set_c, shift_a_left, shift_b_left, shift_c_left);
    _cleanup_valid_configs();

    assert(config_valid);
    a = set_a;
    b = set_b;
    c = set_c;
    left_a = shift_a_left;
    left_b = shift_b_left;
    left_c = shift_c_left;
}

XorShift32::XorShift32(uint32_t seed) : RNGBase(4) {
    // Seeding rules prohibit a seed of 0
    assert(seed != 0);
    state->set_state_bytes_from_int(seed, 4, 0);
    a = 13;
    b = 17;
    c = 5;
    left_a = true;
    left_b = false;
    left_c = true;
    _init_valid_configs();
    bool config_valid = _test_config_valid(a, b, c, left_a, left_b, left_c);
    _cleanup_valid_configs();
    assert(config_valid);
}

XorShift32::XorShift32(uint32_t seed, uint32_t set_a, uint32_t set_b, uint32_t set_c, bool shift_a_left, bool shift_b_left, bool shift_c_left) : RNGBase(4) {
    assert(seed != 0);
    state->set_state_bytes_from_int(seed, 4, 0);

    _init_valid_configs();
    bool config_valid = _test_config_valid(set_a, set_b, set_c, shift_a_left, shift_b_left, shift_c_left);
    _cleanup_valid_configs();

    assert(config_valid);

    a = set_a;
    b = set_b;
    c = set_c;
    left_a = shift_a_left;
    left_b = shift_b_left;
    left_c = shift_c_left;
}

void XorShift32::_seed_random(uint32_t new_seed) {
    // Seeding rules prohibit a seed of 0
    assert(new_seed != 0);
    state->set_state_bytes_from_int(new_seed, 4, 0);
}

std::string XorShift32::name() {
    return "XorShift32";
}

uint32_t XorShift32::_read_random(){
    uint32_t s = state->get_state_bytes_as_int(0);
    if (left_a)
        s ^= s << a;
    else
        s ^= s >> a;
    if (left_b)
        s ^= s << b;
    else
        s ^= s >> b;
    if (left_c)
        s ^= s << c;
    else
        s ^= s >> c;
    state->set_state_bytes_from_int(s, 4, 0);
    return s;
}
