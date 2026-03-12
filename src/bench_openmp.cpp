#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <vector>

#ifdef _OPENMP
#include <omp.h>
#endif

#ifdef _WIN32
#include <windows.h>
#endif

#include "ant.hpp"
#include "fractal_land.hpp"
#include "pheronome.hpp"
#include "rand_generator.hpp"

struct metrics_t {
    double compute_ms_iter;
    double evap_ms_iter;
};

static metrics_t run_once(int num_threads)
{
#ifdef _OPENMP
    omp_set_num_threads(num_threads);
#endif

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

    const double compute_total_ms = total_ants_ms + total_evap_ms + total_update_ms;
    return metrics_t{compute_total_ms / max_iterations, total_evap_ms / max_iterations};
}

int main()
{
    const int repeats = 3;
    std::vector<int> thread_counts;
    if (const char* env_threads = std::getenv("OMP_NUM_THREADS")) {
        const int parsed = std::atoi(env_threads);
        if (parsed > 0) {
            thread_counts.push_back(parsed);
        }
    }
    if (thread_counts.empty()) {
        thread_counts = {1, 2, 4, 6, 12};
    }

    std::vector<double> compute_avg(thread_counts.size(), 0.0);
    std::vector<double> evap_avg(thread_counts.size(), 0.0);

    for (std::size_t idx = 0; idx < thread_counts.size(); ++idx) {
        const int t = thread_counts[idx];
        double compute_sum = 0.0;
        double evap_sum = 0.0;
        for (int r = 0; r < repeats; ++r) {
            const auto m = run_once(t);
            compute_sum += m.compute_ms_iter;
            evap_sum += m.evap_ms_iter;
        }
        compute_avg[idx] = compute_sum / static_cast<double>(repeats);
        evap_avg[idx] = evap_sum / static_cast<double>(repeats);
    }

    const double base_compute = compute_avg[0];
    const double base_evap = evap_avg[0];

    std::cout << "OpenMP section 7 benchmark (500 iterations, average of 3 runs)" << std::endl;
    std::cout << "" << std::endl;
    std::cout << "| Threads | Compute time (ms/iter) | Speedup (compute) | Evaporation time (ms/iter) | Speedup (evaporation) |" << std::endl;
    std::cout << "|---------|-------------------------|-------------------|-----------------------------|------------------------|" << std::endl;
    for (std::size_t idx = 0; idx < thread_counts.size(); ++idx) {
        const int t = thread_counts[idx];
        const double compute_sp = base_compute / compute_avg[idx];
        const double evap_sp = base_evap / evap_avg[idx];
        std::cout << "| " << t
                  << " | " << std::fixed << std::setprecision(3) << compute_avg[idx]
                  << " | " << std::fixed << std::setprecision(3) << compute_sp
                  << " | " << std::fixed << std::setprecision(3) << evap_avg[idx]
                  << " | " << std::fixed << std::setprecision(3) << evap_sp
                  << " |" << std::endl;
    }

    return 0;
}

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    return main();
}
#endif