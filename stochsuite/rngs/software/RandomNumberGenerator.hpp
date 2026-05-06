#ifndef __RANDOM_NUMBER_GENERATOR_BASE__
#define __RANDOM_NUMBER_GENERATOR_BASE__

#include "RNGState.hpp"

#include <stdint.h>
#include <string>

class RNGBase {
    public:
        RNGBase(size_t stateBytes);
        ~RNGBase();

        uint32_t read_random();
        uint32_t read_random_range(uint32_t low, uint32_t high);
        uint32_t read_random_range(uint32_t low, uint32_t high, size_t retries);
        virtual uint32_t _read_random() = 0;

        // Wrapper for calls to child class to allow for
        // generalizable hooks and counters
        void seed_random(uint32_t new_seed);

        // Called by parent class to hook into child
        // class implementations
        virtual void _seed_random(uint32_t new_seed) = 0;

        virtual std::string name() = 0;

        virtual uint32_t MAX() { return 0xFFFFFFFF; };
        virtual uint32_t MIN() { return 0xFFFFFFFF; };

        double read_random_double();

        double gaussian_box_muller();
        double gaussian_box_muller(double mean, double stddev);
        double lognormal_distribution(double mean, double stddev);

        void dump_state(std::string filename);

    protected:
        RNGState* state;
    private:
        size_t num_reseeds;
        size_t num_rands_read;
};
#endif // __RANDOM_NUMBER_GENERATOR_BASE__
