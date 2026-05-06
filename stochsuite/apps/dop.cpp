/* Taken from https://www.quantstart.com/articles/Digital-option-pricing-with-C-via-Monte-Carlo-methods/ */

#include "RNGFactory.hpp"
#include "RandomNumberGenerator.hpp"

#include <cmath>
#include <cassert>
#include <iostream>
#include <cstring>
#include <memory>
#include <gem5/m5ops.h>
#include "m5_mmap.h"

// This is the Heaviside step function, named after English
// mathematician Oliver Heaviside. It returns unity when val
// is greater than or equal to zero and returns zero otherwise
double heaviside(const double& val) {
  if (val >= 0) {
      return 1.0;
  } else {
    return 0.0;
  }
}

// Pricing a digital call option with a Monte Carlo method
double monte_carlo_digital_call_price(const int& num_sims, const double& S,
    const double& K, const double& r, const double& v, const double& T,
    const std::unique_ptr<RNGBase>& rng) {
    double S_adjust = S * exp(T*(r-0.5*v*v));
    double S_cur = 0.0;
    double payoff_sum = 0.0;

        for (int i=0; i<num_sims; i++) {
            double gauss_bm = rng->gaussian_box_muller();
            S_cur = S_adjust * exp(sqrt(v*v*T)*gauss_bm);
            payoff_sum += heaviside(S_cur - K);
        }

    return (payoff_sum / static_cast<double>(num_sims)) * exp(-r*T);
}

// Pricing a digital put option with a Monte Carlo method
double monte_carlo_digital_put_price(const int& num_sims, const double& S,
    const double& K, const double& r, const double& v, const double& T,
    const std::unique_ptr<RNGBase>& rng) {
    double S_adjust = S * exp(T*(r-0.5*v*v));
    double S_cur = 0.0;
    double payoff_sum = 0.0;

    for (int i=0; i<num_sims; i++) {
        double gauss_bm = rng->gaussian_box_muller();
        S_cur = S_adjust * exp(sqrt(v*v*T)*gauss_bm);
        payoff_sum += heaviside(K - S_cur);
    }

    return (payoff_sum / static_cast<double>(num_sims)) * exp(-r*T);
}

int main(int argc, char **argv) {
    // First we create the parameter list
    uint32_t num_sims = 10000000;   // Number of simulated asset paths
    uint32_t seed = 123141323;
    std::string rngStr = "Taus88";

    double S = 100.0;  // Option price
    double K = 100.0;  // Strike price
    double r = 0.05;   // Risk-free rate (5%)
    double v = 0.2;    // Volatility of the underlying (20%)
    double T = 1.0;    // One year until expiry
    std::unique_ptr<RNGBase> rng = RNGFactory::createRNG(rngStr);
    if (!rng) {
        std::cout << "Failed to instantiate RNG instance!" << std::endl;
        return 1;
    }
    std::cout << "Successfully initialized RNG: " << rng->name() << std::endl;

    rng->seed_random(seed);
    if (argc > 1) {
        if (argc > 7) {
            std::cout << "Usage: ./" << argv[0] << " -seed <SEED>";
            std::cout << " -iters <NUM_ITERS> -rng <RNG>" << std::endl;
            return 1;
        }
        for (size_t i = 0; i < (size_t)argc; i++) {
            if (strcmp("-seed",argv[i]) == 0) {
                assert(i + 1 < (size_t)argc);
                seed = (uint32_t)strtol(argv[i+1], NULL, 10);
                i++;
                assert(seed);
            }
            if (strcmp("-iters", argv[i]) == 0) {
                assert(i + 1 < (size_t)argc);
                num_sims = (uint32_t)strtol(argv[i+1], NULL, 10);
                i++;
                assert(num_sims);
            }
            if (strcmp("-rng", argv[i]) == 0) {
                assert(i + 1 < (size_t)argc);
                rngStr = (std::string) argv[i+1];
                i++;
            }
        }
    }
    // Then we calculate the call/put values via Monte Carlo
    map_m5_mem();
    m5_work_begin_addr(0, 0);
    double call = monte_carlo_digital_call_price(num_sims, S, K, r, v, T, rng);
    double put = monte_carlo_digital_put_price(num_sims, S, K, r, v, T, rng);
    m5_work_end_addr(0, 0);
    // Finally we output the parameters and prices

    std::cout << "Number of Paths: " << num_sims << std::endl;
    std::cout << "Underlying:      " << S << std::endl;
    std::cout << "Strike:          " << K << std::endl;
    std::cout << "Risk-Free Rate:  " << r << std::endl;
    std::cout << "Volatility:      " << v << std::endl;
    std::cout << "Maturity:        " << T << std::endl;

    std::cout << "Call Price:      " << call << std::endl;
    std::cout << "Put Price:       " << put << std::endl;

    return 0;
}
