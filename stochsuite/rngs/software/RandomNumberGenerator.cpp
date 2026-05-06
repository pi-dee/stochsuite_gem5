#include "RandomNumberGenerator.hpp"
#include "RNGState.hpp"

#include <cmath>

RNGBase::RNGBase(size_t stateBytes)
    : state(new RNGState(stateBytes)),
      num_reseeds(0),
      num_rands_read(0) {
}

uint32_t RNGBase::read_random() {
    num_rands_read++;
    return _read_random();
}
uint32_t RNGBase::read_random_range(uint32_t low, uint32_t high) {
    uint32_t elems = high - low;
    uint32_t bias = MAX() - (MAX() % elems);
    num_rands_read++;
    uint32_t rand = _read_random();
    while (rand > bias) {
        num_rands_read++;
        rand = _read_random();
    }
    return rand;
}

uint32_t RNGBase::read_random_range(uint32_t low, uint32_t high, size_t retries) {
    uint32_t elems = high - low;
    uint32_t bias = MAX() - (MAX() % elems);
    num_rands_read++;
    uint32_t rand = _read_random();
    for (size_t i = 1; i < retries; i++) {
        if (rand <= bias) {
            return rand + low;
        }
        num_rands_read++;
        rand = _read_random();
    }
    return rand;
}

double RNGBase::read_random_double() {
    uint32_t rand = _read_random();
    double frac = ((double) rand) / ((double) MAX());
    return frac;
}

// A simple implementation of the Box-Muller algorithm, used to generate
// gaussian random numbers
double RNGBase::gaussian_box_muller() {
    double x = 0.0;
    double y = 0.0;
    double ret;
    double euclid_sq = 0.0;
    // Continue generating two uniform random variables
    // until the square of their "euclidean distance"
    // is less than unity
    do {
        x = 2.0 * read_random_double() - 1;
        y = 2.0 * read_random_double() - 1;
        euclid_sq = x*x + y*y;
    } while (euclid_sq >= 1.0);
    ret = x*sqrt(-2*log(euclid_sq)/euclid_sq);
    return ret;
}

double RNGBase::gaussian_box_muller(double mean, double stddev) {
    double ret = gaussian_box_muller();
    return (ret * stddev) + mean;
}

double RNGBase::lognormal_distribution(double mean, double stddev) {
    double ret = gaussian_box_muller(mean, stddev);
    return exp(ret);
}

void RNGBase::seed_random(uint32_t new_seed) {
    num_reseeds++;
    _seed_random(new_seed);
}

void RNGBase::dump_state(std::string filename) {
    state->dump(filename);
}

RNGBase::~RNGBase() {
    delete state;
}
