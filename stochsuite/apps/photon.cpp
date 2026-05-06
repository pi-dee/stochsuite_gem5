/* From
https://www.scratchapixel.com/lessons/mathematics-physics-for-computer-graphics/monte-carlo-methods-in-practice/monte-carlo-simulation.html
https://github.com/scratchapixel/scratchapixel-code/blob/main/monte-carlo-methods-in-practice/mcsim.cpp
*/
#include "RNGFactory.hpp"
#include "RandomNumberGenerator.hpp"

#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <cstring>
#include <memory>
#include <gem5/m5ops.h>
#include "m5_mmap.h"

// sampling the H-G scattering phase function
double getCosTheta(const double &g, const std::unique_ptr<RNGBase>& rng) {
    double mu;

    if (g == 0) {
        return 2 * rng->read_random_double() - 1;
    }
    mu = (1 - g * g) / (1 - g + 2 * g * rng->read_random_double());
    return (1 + g * g - mu * mu) / (2.0 * g);
}

void spin(double &mu_x, double &mu_y, double &mu_z, const double &g,
    const std::unique_ptr<RNGBase>& rng) {

    double costheta = getCosTheta(g, rng);
    double phi = 2 * M_PI * rng->read_random_double();
    double sintheta = sqrt(1.0 - costheta * costheta); // sin(theta)
    double sinphi = sin(phi);
    double cosphi = cos(phi);
    if (mu_z == 1.0) {
        mu_x = sintheta * cosphi;
        mu_y = sintheta * sinphi;
        mu_z = costheta;
    }
    else if (mu_z == -1.0) {
        mu_x = sintheta * cosphi;
        mu_y = -sintheta * sinphi;
        mu_z = -costheta;
    }
    else {
        double denom = sqrt(1.0 - mu_z * mu_z);
        double muzcosphi = mu_z * cosphi;
        double ux = sintheta * (mu_x * muzcosphi - mu_y * sinphi) / denom + mu_x * costheta;
        double uy = sintheta * (mu_y * muzcosphi + mu_x * sinphi) / denom + mu_y * costheta;
        double uz = -denom * sintheta * cosphi + mu_z * costheta;
        mu_x = ux, mu_y = uy, mu_z = uz;
    }
}

void MCSimulation(double *&records, const uint32_t &size, const uint32_t &nphotons,
    const std::unique_ptr<RNGBase>& rng) {
    double sigma_a = 1, sigma_s = 2, sigma_t = sigma_a + sigma_s;
    double d = 0.5, slabsize = 0.5, g = 0.75;
    static const short m = 10;
    for (uint32_t n = 0; n < nphotons; ++n) {
        double w = 1;
        double x = 0, y = 0, z = 0, mux = 0, muy = 0, muz = 1;
        while (w != 0) {
            double s = -log(rng->read_random_double()) / sigma_t;
            double distToBoundary = 0;
            if (muz > 0) distToBoundary = (d - z) / muz;
            else if (muz < 0) distToBoundary = -z / muz;
            if (s > distToBoundary) {
                int xi = (int)((x + slabsize / 2) / slabsize * size);
                int yi = (int)((y + slabsize / 2) / slabsize * size);
                if (muz > 0 && xi >= 0 && xi < (int)size && yi >= 0 && yi < (int)size) {
                    records[yi * size + xi] += w;
                }
                break;
            }
            x += s * mux;
            y += s * muy;
            z += s * muz;
            double dw = sigma_a / sigma_t;
            w -= dw; w = std::max(0.0, w);
            if (w < 0.001) { // russian roulette test
                if (rng->read_random_double() > 1.0 / m) {
                    break;
                } else {
		            w *= m;
                }
            }
            spin(mux, muy, muz, g, rng);
        }
    }
}

int main(int argc, char *argv[]) {
    size_t niters = 64;
    uint32_t seed = 12312332;
    std::string rngStr = "Taus88";
    uint32_t image_size = 512;
    bool save_result = false;
    uint32_t photons = 100000;

    if (argc > 1) {
        if (argc > 12) {
            std::cout << "usage: ./" << argv[0] << " -seed <SEED> ";
            std::cout << " -iters <NUM_REFINEMENT_PASSES>";
            std::cout << " -rng <RNG>";
            std::cout << " -size <IMAGE_SIZE>";
            std::cout << " -photons <NUM_PHOTONS>";
            std::cout << " -save" << std::endl;
            return 1;
        }
        for (size_t i = 0; i < (size_t)argc; i++) {
            if (strcmp("-seed", argv[i]) == 0) {
                assert(i + 1 < (size_t)argc);
                seed = (uint32_t) strtol(argv[i+1], NULL, 10);
                i++;
            }
            if (strcmp("-iters", argv[i]) == 0) {
                assert(i + 1 < (size_t)argc);
                niters = (size_t) strtol(argv[i+1], NULL, 10);
                i++;
            }
            if (strcmp("-rng", argv[i]) == 0) {
                assert(i + 1 < (size_t)argc);
                rngStr = (std::string) argv[i+1];
                i++;
            }
            if (strcmp("-size", argv[i]) == 0) {
                assert(i + 1 < (size_t)argc);
                image_size = (size_t) strtol(argv[i+1], NULL, 10);
                i++;
            }
            if (strcmp("-photons", argv[i]) == 0) {
                assert(i + 1 < (size_t)argc);
                photons = (uint32_t) strtol(argv[i+1], NULL, 10);
                i++;
            }
            if (strcmp("-save", argv[i]) == 0) {
                save_result = true;
            }
        }
    }
    std::unique_ptr<RNGBase> rng = RNGFactory::createRNG(rngStr);
    if (!rng) {
        std::cout << "Failed to instantiate RNG instance!" << std::endl;
        return 1;
    }
    std::cout << "Successfully initialized RNG: " << rng->name() << std::endl;

    rng->seed_random(seed);

    double *records = new double[image_size * image_size];
    memset(records, 0x0, sizeof(double) * image_size * image_size);

    float *pixels = new float[image_size * image_size]; // image

    // Iterate for niters refinement passes
    map_m5_mem();
    m5_work_begin_addr(0, 0);
    for (uint32_t npasses = 1; npasses < niters; npasses++) {
        MCSimulation(records, image_size, photons, rng);
        for (uint32_t i = 0; i < image_size * image_size; i++) {
            pixels[i] = records[i] / npasses;
        }
    }
    m5_work_end_addr(0, 0);

    if (save_result) {
        // save image to file
        std::ofstream ofs;
        ofs.open("./out.ppm", std::ios::out | std::ios::binary);
        ofs << "P6\n" << image_size << " " << image_size << "\n255\n";
        for (uint32_t i = 0; i < image_size * image_size; ++i) {
            unsigned char val = (unsigned char)(255 * std::min(1.0f, pixels[i]));
            ofs << val << val << val;
        }

        ofs.close();
    }

    delete [] records;
    delete [] pixels;

    return 0;
}
