#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#include "fractal_land.hpp"
#include "pheronome.hpp"
#include "rand_generator.hpp"

class ant_colony_soa
{
public:
    ant_colony_soa(std::size_t count, const fractal_land& land, std::size_t& seed)
        : m_pos_x(count), m_pos_y(count), m_loaded(count, 0), m_seed(count)
    {
        for (std::size_t i = 0; i < count; ++i) {
            m_pos_x[i] = rand_int32(0, static_cast<std::int32_t>(land.dimensions() - 1), seed);
            m_pos_y[i] = rand_int32(0, static_cast<std::int32_t>(land.dimensions() - 1), seed);
            m_seed[i] = seed;
        }
    }

    void set_exploration_coef(double eps)
    {
        m_eps = eps;
    }

    std::size_t size() const
    {
        return m_pos_x.size();
    }

    void advance_all(pheronome& phen, const fractal_land& land, const position_t& pos_food,
                     const position_t& pos_nest, std::size_t& food_counter)
    {
        for (std::size_t ant_index = 0; ant_index < size(); ++ant_index) {
            advance_one(ant_index, phen, land, pos_food, pos_nest, food_counter);
        }
    }

private:
    double pheromone_at(const pheronome& phen, int x, int y, int pheromone_index) const
    {
        return phen[position_t{x, y}][pheromone_index];
    }

    void advance_one(std::size_t ant_index, pheronome& phen, const fractal_land& land,
                     const position_t& pos_food, const position_t& pos_nest, std::size_t& food_counter)
    {
        auto ant_choice = [this, ant_index]() mutable {
            return rand_double(0., 1., m_seed[ant_index]);
        };
        auto dir_choice = [this, ant_index]() mutable {
            return rand_int32(1, 4, m_seed[ant_index]);
        };

        double consumed_time = 0.;
        while (consumed_time < 1.) {
            const int pheromone_index = (m_loaded[ant_index] != 0) ? 1 : 0;
            const double choice = ant_choice();
            const int old_x = m_pos_x[ant_index];
            const int old_y = m_pos_y[ant_index];
            int new_x = old_x;
            int new_y = old_y;

            const double left = pheromone_at(phen, old_x - 1, old_y, pheromone_index);
            const double right = pheromone_at(phen, old_x + 1, old_y, pheromone_index);
            const double up = pheromone_at(phen, old_x, old_y - 1, pheromone_index);
            const double down = pheromone_at(phen, old_x, old_y + 1, pheromone_index);
            const double max_phen = std::max({left, right, up, down});

            if ((choice > m_eps) || (max_phen <= 0.)) {
                do {
                    new_x = old_x;
                    new_y = old_y;
                    const int direction = dir_choice();
                    if (direction == 1) {
                        --new_x;
                    }
                    if (direction == 2) {
                        --new_y;
                    }
                    if (direction == 3) {
                        ++new_x;
                    }
                    if (direction == 4) {
                        ++new_y;
                    }
                } while (pheromone_at(phen, new_x, new_y, pheromone_index) == -1.);
            } else if (left == max_phen) {
                --new_x;
            } else if (right == max_phen) {
                ++new_x;
            } else if (up == max_phen) {
                --new_y;
            } else {
                ++new_y;
            }

            consumed_time += land(static_cast<unsigned long>(new_x), static_cast<unsigned long>(new_y));
            phen.mark_pheronome(position_t{new_x, new_y});
            m_pos_x[ant_index] = new_x;
            m_pos_y[ant_index] = new_y;

            if ((new_x == pos_nest.x) && (new_y == pos_nest.y)) {
                if (m_loaded[ant_index] != 0) {
                    ++food_counter;
                }
                m_loaded[ant_index] = 0;
            }
            if ((new_x == pos_food.x) && (new_y == pos_food.y)) {
                m_loaded[ant_index] = 1;
            }
        }
    }

    double m_eps = 0.;
    std::vector<int> m_pos_x;
    std::vector<int> m_pos_y;
    std::vector<std::uint8_t> m_loaded;
    std::vector<std::size_t> m_seed;
};

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

    ant_colony_soa ants(nb_ants, land, seed);
    ants.set_exploration_coef(eps);
    pheronome phen(land.dimensions(), pos_food, pos_nest, alpha, beta);

    std::size_t food_quantity = 0;
    double total_ants_ms = 0.;
    double total_evap_ms = 0.;
    double total_update_ms = 0.;

    for (std::size_t iteration = 0; iteration < max_iterations; ++iteration) {
        const auto t0 = std::chrono::high_resolution_clock::now();
        ants.advance_all(phen, land, pos_food, pos_nest, food_quantity);
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
    std::cout << "=== SoA headless benchmark over " << max_iterations << " iterations ===" << std::endl;
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