#include "RNGState.hpp"

#include <cassert>
#include <cstdio>


RNGState::RNGState(size_t numBytes)
    : numBytes(numBytes) {
    allState = new uint8_t[numBytes];
}

uint8_t *RNGState::getState() {
    return allState;
}

void RNGState::set_state_bytes_from_int(uint32_t new_state,
            uint32_t valid_bytes, size_t state_offset) {
    if (valid_bytes == 0) return;
    assert(valid_bytes <= 4);
    assert(state_offset + valid_bytes <= numBytes);
    for (size_t i = 0; i < valid_bytes; i++) {
        allState[state_offset + i] = (uint8_t)((new_state >> (8*i)) & 0xFF);
    }
}

void RNGState::set_state_bytes_from_long(uint64_t new_state,
            uint32_t valid_bytes, size_t state_offset) {
    if (valid_bytes == 0) return;
    assert(valid_bytes <= 8);
    assert(state_offset + valid_bytes <= numBytes);
    for (size_t i = 0; i < valid_bytes; i++) {
        allState[state_offset + i] = (uint8_t)((new_state >> (8*i)) & 0xFF);
    }
}

void RNGState::set_state_byte(uint8_t new_state, size_t state_offset) {
    assert(state_offset < numBytes);
    allState[state_offset] = new_state;
}

uint32_t RNGState::get_state_bytes_as_int(size_t state_offset) {
    assert(state_offset < numBytes);
    assert(state_offset + 4 <= numBytes);
    uint32_t result = 0;
    for (size_t i = 0; i < 4; i++) {
        result |= allState[i + state_offset] << (8*i);
    }
    return result;
}

uint64_t RNGState::get_state_bytes_as_long(size_t state_offset) {
    assert(state_offset < numBytes);
    assert(state_offset + 8 <= numBytes);
    uint64_t result = 0;
    for (size_t i = 0; i < 8; i++) {
        result |= allState[i + state_offset] << (8*i);
    }
    return result;
}

uint8_t RNGState::get_state_byte(size_t state_offset) {
    assert(state_offset < numBytes);
    return allState[state_offset];
}

size_t RNGState::size() {
    return numBytes;
}

void RNGState::dump(std::string filename) {

    // Write the hex values of the state bytes to a file, one byte per line and also print them to the console
    FILE *f = fopen(filename.c_str(), "w");
    for (size_t i = 0; i < numBytes; i++) {
        fprintf(f, "%02x\n", allState[i]);
        printf("State byte %zu: %u %02x\n", i, (uint32_t) allState[i], allState[i]);
    }
    fclose(f);
}

RNGState::~RNGState() {
    delete[] allState;
}
