#include <vector>
#include <iostream>
#include <random>
#include <chrono>
#include "fractal_land.hpp"
#include "ant.hpp"
#include "pheronome.hpp"
# include "renderer.hpp"
# include "window.hpp"
# include "rand_generator.hpp"

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

    const std::size_t max_iterations = 500;
    double total_ants_ms = 0., total_evap_ms = 0., total_update_ms = 0., total_display_ms = 0.;

    while (cont_loop && it < max_iterations) {
        ++it;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                cont_loop = false;
        }

        auto t0 = std::chrono::high_resolution_clock::now();
        for ( size_t i = 0; i < ants.size(); ++i )
            ants[i].advance(phen, land, pos_food, pos_nest, food_quantity);
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
