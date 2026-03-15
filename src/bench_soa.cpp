#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#include "fractal_land.hpp"
#include "pheronome.hpp"

class ant_colony_soa
{
public:
    ant_colony_soa(std::size_t count, std::size_t land_dim, std::size_t pher_stride,
                   double alpha, std::size_t& seed)
        : m_land_dim(land_dim),
          m_pher_stride(pher_stride),
          m_alpha(alpha),
          m_land_index(count),
          m_pher_index(count),
          m_loaded(count, 0),
          m_seed(count),
          m_touched(pher_stride * pher_stride, 0)
    {
        for (std::size_t i = 0; i < count; ++i) {
            std::uint32_t init_seed = static_cast<std::uint32_t>(seed);
            const auto x = rand_int32_local(0, static_cast<std::int32_t>(land_dim - 1), init_seed);
            const auto y = rand_int32_local(0, static_cast<std::int32_t>(land_dim - 1), init_seed);
            m_land_index[i] = static_cast<std::uint32_t>(x + y * land_dim);
            m_pher_index[i] = static_cast<std::uint32_t>((x + 1) * pher_stride + (y + 1));
            m_seed[i] = init_seed;
            seed = init_seed;
        }

        m_touched_indices.reserve(count * 2);
        m_visited_indices.reserve(count * 4);
    }

    void set_exploration_coef(double eps)
    {
        m_eps = eps;
    }

    std::size_t size() const
    {
        return m_land_index.size();
    }

    void advance_all(pheronome& phen, const fractal_land& land, const position_t& pos_food,
                     const position_t& pos_nest, std::size_t& food_counter)
    {
        reset_touched();
        m_visited_indices.clear();

        const auto* land_data = land.data();
        const auto* map = phen.raw_map_cells();
        auto* buffer = phen.raw_buffer_cells();
        const std::uint32_t nest_land_index =
            static_cast<std::uint32_t>(pos_nest.x + pos_nest.y * static_cast<long>(m_land_dim));
        const std::uint32_t food_land_index =
            static_cast<std::uint32_t>(pos_food.x + pos_food.y * static_cast<long>(m_land_dim));

        for (std::size_t ant_index = 0; ant_index < size(); ++ant_index) {
            advance_one(ant_index, land_data, map, nest_land_index, food_land_index, food_counter);
        }

        for (std::uint32_t idx : m_visited_indices) {
            if (m_touched[idx] == 0) {
                m_touched[idx] = 1;
                m_touched_indices.push_back(idx);
            }
        }

        for (std::size_t idx : m_touched_indices) {
            buffer[idx] = compute_mark(map, idx);
        }
    }

private:
    static std::uint32_t next_seed(std::uint32_t seed)
    {
        return static_cast<std::uint32_t>((1664525ull * seed + 1013904223ull) % 0xFFFFFFFFu);
    }

    static std::int32_t rand_int32_local(std::int32_t min_val, std::int32_t max_val, std::uint32_t& seed)
    {
        seed = next_seed(seed);
        return min_val + static_cast<std::int32_t>(seed % static_cast<std::uint32_t>(max_val - min_val + 1));
    }

    static double rand_double_local(double min_val, double max_val, std::uint32_t& seed)
    {
        seed = next_seed(seed);
        return min_val + std::fmod(static_cast<double>(seed), (max_val - min_val + 1.0));
    }

    pheronome::pheronome_t compute_mark(const pheronome::pheronome_t* map, std::size_t idx) const
    {
        const auto& left_cell = map[idx - m_pher_stride];
        const auto& right_cell = map[idx + m_pher_stride];
        const auto& upper_cell = map[idx - 1];
        const auto& bottom_cell = map[idx + 1];
        const double v1_left = std::max(left_cell[0], 0.0);
        const double v2_left = std::max(left_cell[1], 0.0);
        const double v1_right = std::max(right_cell[0], 0.0);
        const double v2_right = std::max(right_cell[1], 0.0);
        const double v1_upper = std::max(upper_cell[0], 0.0);
        const double v2_upper = std::max(upper_cell[1], 0.0);
        const double v1_bottom = std::max(bottom_cell[0], 0.0);
        const double v2_bottom = std::max(bottom_cell[1], 0.0);
        return pheronome::pheronome_t{{
            m_alpha * std::max({v1_left, v1_right, v1_upper, v1_bottom}) +
                (1.0 - m_alpha) * 0.25 * (v1_left + v1_right + v1_upper + v1_bottom),
            m_alpha * std::max({v2_left, v2_right, v2_upper, v2_bottom}) +
                (1.0 - m_alpha) * 0.25 * (v2_left + v2_right + v2_upper + v2_bottom)}};
    }

    void reset_touched()
    {
        for (std::size_t idx : m_touched_indices) {
            m_touched[idx] = 0;
        }
        m_touched_indices.clear();
    }

    void advance_one(std::size_t ant_index, const double* land_data, const pheronome::pheronome_t* map,
                     std::uint32_t nest_land_index, std::uint32_t food_land_index,
                     std::size_t& food_counter)
    {
        std::uint32_t land_index = m_land_index[ant_index];
        std::uint32_t pher_index = m_pher_index[ant_index];
        std::uint32_t& rng_state = m_seed[ant_index];
        std::uint8_t& loaded = m_loaded[ant_index];

        double consumed_time = 0.;
        while (consumed_time < 1.) {
            const int pheromone_index = (loaded != 0) ? 1 : 0;
            const double choice = rand_double_local(0., 1., rng_state);
            std::uint32_t new_land_index = land_index;
            std::uint32_t new_pher_index = pher_index;

            const double left = map[pher_index - m_pher_stride][pheromone_index];
            const double right = map[pher_index + m_pher_stride][pheromone_index];
            const double up = map[pher_index - 1][pheromone_index];
            const double down = map[pher_index + 1][pheromone_index];
            const double max_phen = std::max({left, right, up, down});

            if ((choice > m_eps) || (max_phen <= 0.)) {
                do {
                    new_land_index = land_index;
                    new_pher_index = pher_index;
                    const int direction = rand_int32_local(1, 4, rng_state);
                    if (direction == 1) {
                        --new_land_index;
                        new_pher_index -= static_cast<std::uint32_t>(m_pher_stride);
                    }
                    if (direction == 2) {
                        new_land_index -= static_cast<std::uint32_t>(m_land_dim);
                        --new_pher_index;
                    }
                    if (direction == 3) {
                        ++new_land_index;
                        new_pher_index += static_cast<std::uint32_t>(m_pher_stride);
                    }
                    if (direction == 4) {
                        new_land_index += static_cast<std::uint32_t>(m_land_dim);
                        ++new_pher_index;
                    }
                } while (map[new_pher_index][pheromone_index] == -1.);
            } else if (left == max_phen) {
                --new_land_index;
                new_pher_index -= static_cast<std::uint32_t>(m_pher_stride);
            } else if (right == max_phen) {
                ++new_land_index;
                new_pher_index += static_cast<std::uint32_t>(m_pher_stride);
            } else if (up == max_phen) {
                new_land_index -= static_cast<std::uint32_t>(m_land_dim);
                --new_pher_index;
            } else {
                new_land_index += static_cast<std::uint32_t>(m_land_dim);
                ++new_pher_index;
            }

            consumed_time += land_data[new_land_index];
            m_visited_indices.push_back(new_pher_index);
            land_index = new_land_index;
            pher_index = new_pher_index;

            if (new_land_index == nest_land_index) {
                if (loaded != 0) {
                    ++food_counter;
                }
                loaded = 0;
            }
            if (new_land_index == food_land_index) {
                loaded = 1;
            }
        }

        m_land_index[ant_index] = land_index;
        m_pher_index[ant_index] = pher_index;
    }

    double m_eps = 0.;
    std::size_t m_land_dim;
    std::size_t m_pher_stride;
    double m_alpha;
    std::vector<std::uint32_t> m_land_index;
    std::vector<std::uint32_t> m_pher_index;
    std::vector<std::uint8_t> m_loaded;
    std::vector<std::uint32_t> m_seed;
    std::vector<unsigned char> m_touched;
    std::vector<std::size_t> m_touched_indices;
    std::vector<std::uint32_t> m_visited_indices;
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

    pheronome phen(land.dimensions(), pos_food, pos_nest, alpha, beta);
    ant_colony_soa ants(nb_ants, land.dimensions(), phen.stride(), alpha, seed);
    ants.set_exploration_coef(eps);

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
    std::cout << "=== Improved SoA headless benchmark over " << max_iterations << " iterations ===" << std::endl;
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
