#include <algorithm>
#include <chrono>
#include <iostream>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#include "ant.hpp"
#include "fractal_land.hpp"
#include "pheronome.hpp"
#include "rand_generator.hpp"

int main()
{
    std::size_t seed = 2026;
    const int nb_ants = 5000;
    const double eps = 0.8;
    const double alpha = 0.7;
    const double beta = 0.999;
    const std::size_t max_iterations = 500;

    position_t pos_nest{256, 256};
    position_t pos_food{500, 500};

    fractal_land land(8, 2, 1., 1024);
    double max_val = 0.0;
    double min_val = 0.0;
    for (fractal_land::dim_t i = 0; i < land.dimensions(); ++i) {
        for (fractal_land::dim_t j = 0; j < land.dimensions(); ++j) {
            max_val = std::max(max_val, land(i, j));
            min_val = std::min(min_val, land(i, j));
        }
    }

    const double delta = max_val - min_val;
    for (fractal_land::dim_t i = 0; i < land.dimensions(); ++i) {
        for (fractal_land::dim_t j = 0; j < land.dimensions(); ++j) {
            land(i, j) = (land(i, j) - min_val) / delta;
        }
    }

    ant::set_exploration_coef(eps);
    std::vector<ant> ants;
    ants.reserve(nb_ants);
    auto gen_ant_pos = [&land, &seed]() {
        return rand_int32(0, static_cast<std::int32_t>(land.dimensions() - 1), seed);
    };
    for (int i = 0; i < nb_ants; ++i) {
        ants.emplace_back(position_t{gen_ant_pos(), gen_ant_pos()}, seed);
    }

    pheronome phen(land.dimensions(), pos_food, pos_nest, alpha, beta);
    std::size_t food_quantity = 0;
    double total_ants_ms = 0.;
    double total_evap_ms = 0.;
    double total_update_ms = 0.;

    for (std::size_t iteration = 0; iteration < max_iterations; ++iteration) {
        const auto t0 = std::chrono::high_resolution_clock::now();
        for (std::size_t i = 0; i < ants.size(); ++i) {
            ants[i].advance(phen, land, pos_food, pos_nest, food_quantity);
        }
        const auto t1 = std::chrono::high_resolution_clock::now();
        phen.do_evaporation();
        const auto t2 = std::chrono::high_resolution_clock::now();
        phen.update();
        const auto t3 = std::chrono::high_resolution_clock::now();

        total_ants_ms += std::chrono::duration<double, std::milli>(t1 - t0).count();
        total_evap_ms += std::chrono::duration<double, std::milli>(t2 - t1).count();
        total_update_ms += std::chrono::duration<double, std::milli>(t3 - t2).count();
    }

    const double total_compute_ms = total_ants_ms + total_evap_ms + total_update_ms;
    std::cout << "=== Sequential headless benchmark over " << max_iterations << " iterations ===" << std::endl;
    std::cout << "Ant movement  : " << total_ants_ms << " ms total, " << total_ants_ms / max_iterations << " ms/iter" << std::endl;
    std::cout << "Evaporation   : " << total_evap_ms << " ms total, " << total_evap_ms / max_iterations << " ms/iter" << std::endl;
    std::cout << "Pheromone upd : " << total_update_ms << " ms total, " << total_update_ms / max_iterations << " ms/iter" << std::endl;
    std::cout << "Compute total : " << total_compute_ms << " ms total, " << total_compute_ms / max_iterations << " ms/iter" << std::endl;
    std::cout << "Food returned : " << food_quantity << std::endl;
    return 0;
}

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    return main();
}
#endif