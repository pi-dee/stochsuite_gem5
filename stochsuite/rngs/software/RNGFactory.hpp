#ifndef __RANDOM_NUMBER_GENERATOR_FACTORY__
#define __RANDOM_NUMBER_GENERATOR_FACTORY__

#include "RandomNumberGenerator.hpp"

#include <memory>

class RNGFactory {
    public:
        RNGFactory();
        ~RNGFactory();
        static std::unique_ptr<RNGBase> createRNG(const std::string type);
};
#endif // __RANDOM_NUMBER_GENERATOR_FACTORY__
