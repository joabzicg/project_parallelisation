#include <vector>
#include <iostream>
#include <random>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef _OPENMP
#include <omp.h>
#endif

#include "fractal_land.hpp"
#include "ant.hpp"
#include "pheronome.hpp"
# include "renderer.hpp"
# include "window.hpp"
# include "rand_generator.hpp"

struct thread_marks_t {
    explicit thread_marks_t(std::size_t cell_count)
        : touched(cell_count, 0), values(cell_count, {{0., 0.}})
    {
        visited_positions.reserve(4096);
        touched_indices.reserve(4096);
    }

    void reset_iteration()
    {
        for (std::size_t idx : touched_indices)
            touched[idx] = 0;
        touched_indices.clear();
        visited_positions.clear();
    }

    std::vector<unsigned char> touched;
    std::vector<pheronome::pheronome_t> values;
    std::vector<position_t> visited_positions;
    std::vector<std::size_t> touched_indices;
};

void advance_time( const fractal_land& land, pheronome& phen, 
                   const position_t& pos_nest, const position_t& pos_food,
                   std::vector<ant>& ants, std::size_t& cpteur )
{
    for ( size_t i = 0; i < ants.size(); ++i )
        ants[i].advance(phen, land, pos_food, pos_nest, cpteur);
    phen.do_evaporation();
    phen.update();
}

int main(int nargs, char* argv[])
{
    SDL_Init( SDL_INIT_VIDEO );
    std::size_t seed = 2026; // Graine pour la génération aléatoire ( reproductible )
    const int nb_ants = 5000; // Nombre de fourmis
    const double eps = 0.8;  // Coefficient d'exploration
    const double alpha=0.7; // Coefficient de chaos
    //const double beta=0.9999; // Coefficient d'évaporation
    const double beta=0.999; // Coefficient d'évaporation
    // Location du nid
    position_t pos_nest{256,256};
    // Location de la nourriture
    position_t pos_food{500,500};
    //const int i_food = 500, j_food = 500;    
    // Génération du territoire 512 x 512 ( 2*(2^8) par direction )
    fractal_land land(8,2,1.,1024);
    double max_val = 0.0;
    double min_val = 0.0;
    for ( fractal_land::dim_t i = 0; i < land.dimensions(); ++i )
        for ( fractal_land::dim_t j = 0; j < land.dimensions(); ++j ) {
            max_val = std::max(max_val, land(i,j));
            min_val = std::min(min_val, land(i,j));
        }
    double delta = max_val - min_val;
    /* On redimensionne les valeurs de fractal_land de sorte que les valeurs
    soient comprises entre zéro et un */
    for ( fractal_land::dim_t i = 0; i < land.dimensions(); ++i )
        for ( fractal_land::dim_t j = 0; j < land.dimensions(); ++j )  {
            land(i,j) = (land(i,j)-min_val)/delta;
        }
    // Définition du coefficient d'exploration de toutes les fourmis.
    ant::set_exploration_coef(eps);
    // On va créer des fourmis un peu partout sur la carte :
    std::vector<ant> ants;
    ants.reserve(nb_ants);
    auto gen_ant_pos = [&land, &seed] () { return rand_int32(0, land.dimensions()-1, seed); };
    for ( size_t i = 0; i < nb_ants; ++i )
        ants.emplace_back(position_t{gen_ant_pos(),gen_ant_pos()}, seed);
    // On crée toutes les fourmis dans la fourmilière.
    pheronome phen(land.dimensions(), pos_food, pos_nest, alpha, beta);

    Window win("Ant Simulation", 2*land.dimensions()+10, land.dimensions()+266);
    Renderer renderer( land, phen, pos_nest, pos_food, ants );
    // Compteur de la quantité de nourriture apportée au nid par les fourmis
    size_t food_quantity = 0;
    SDL_Event event;
    bool cont_loop = true;
    bool not_food_in_nest = true;
    std::size_t it = 0;

    int worker_count = 1;
#ifdef _OPENMP
    worker_count = omp_get_max_threads();
#endif
    const std::size_t cell_count = phen.raw_buffer_size() / 2;
    std::vector<thread_marks_t> thread_marks;
    thread_marks.reserve(static_cast<std::size_t>(worker_count));
    for (int t = 0; t < worker_count; ++t)
        thread_marks.emplace_back(cell_count);

    const std::size_t max_iterations = 500;
    double total_ants_ms = 0., total_evap_ms = 0., total_update_ms = 0., total_display_ms = 0.;

    while (cont_loop && it < max_iterations) {
        ++it;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                cont_loop = false;
        }

        auto t0 = std::chrono::high_resolution_clock::now();
        std::size_t food_delta = 0;
#ifdef _OPENMP
        if (worker_count > 1) {
#pragma omp parallel reduction(+ : food_delta)
            {
                const int thread_id = omp_get_thread_num();
                thread_marks_t& local_marks = thread_marks[static_cast<std::size_t>(thread_id)];
                local_marks.reset_iteration();
                std::size_t local_food = 0;

#pragma omp for schedule(static)
                for (int i = 0; i < nb_ants; ++i) {
                    ants[static_cast<std::size_t>(i)].advance_collect(
                        phen, land, pos_food, pos_nest, local_food, local_marks.visited_positions);
                }

                for (const position_t& pos : local_marks.visited_positions) {
                    const std::size_t idx = phen.raw_index(pos);
                    if (local_marks.touched[idx] == 0) {
                        local_marks.touched[idx] = 1;
                        local_marks.touched_indices.push_back(idx);
                        local_marks.values[idx] = phen.compute_mark(pos);
                    }
                }

                food_delta += local_food;
            }

            for (const thread_marks_t& local_marks : thread_marks)
                for (std::size_t idx : local_marks.touched_indices)
                    phen.set_buffer_cell(idx, local_marks.values[idx]);
        } else {
            for ( size_t i = 0; i < ants.size(); ++i )
                ants[i].advance(phen, land, pos_food, pos_nest, food_delta);
        }
#else
        for ( size_t i = 0; i < ants.size(); ++i )
            ants[i].advance(phen, land, pos_food, pos_nest, food_delta);
#endif
        food_quantity += food_delta;
        auto t1 = std::chrono::high_resolution_clock::now();
        phen.do_evaporation();
        auto t2 = std::chrono::high_resolution_clock::now();
        phen.update();
        auto t3 = std::chrono::high_resolution_clock::now();
        renderer.display( win, food_quantity );
        win.blit();
        auto t4 = std::chrono::high_resolution_clock::now();

        total_ants_ms    += std::chrono::duration<double,std::milli>(t1-t0).count();
        total_evap_ms    += std::chrono::duration<double,std::milli>(t2-t1).count();
        total_update_ms  += std::chrono::duration<double,std::milli>(t3-t2).count();
        total_display_ms += std::chrono::duration<double,std::milli>(t4-t3).count();

        if ( not_food_in_nest && food_quantity > 0 ) {
            std::cout << "First food arrived at nest at iteration " << it << std::endl;
            not_food_in_nest = false;
        }
    }

    double total_ms = total_ants_ms + total_evap_ms + total_update_ms + total_display_ms;
    std::cout << std::endl;
    std::cout << "=== Timing results over " << it << " iterations ===" << std::endl;
    std::cout << "Ant movement  : " << total_ants_ms    << " ms total, " << total_ants_ms/it    << " ms/iter" << std::endl;
    std::cout << "Evaporation   : " << total_evap_ms    << " ms total, " << total_evap_ms/it    << " ms/iter" << std::endl;
    std::cout << "Pheromone upd : " << total_update_ms  << " ms total, " << total_update_ms/it  << " ms/iter" << std::endl;
    std::cout << "Display+blit  : " << total_display_ms << " ms total, " << total_display_ms/it << " ms/iter" << std::endl;
    std::cout << "Total per iter: " << total_ms/it << " ms/iter" << std::endl;
    std::cout << "Total time    : " << total_ms << " ms" << std::endl;
    SDL_Quit();
    return 0;
}

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    return main(__argc, __argv);
}
#endif
