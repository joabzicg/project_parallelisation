#ifndef _PHERONOME_HPP_
#define _PHERONOME_HPP_
#include <algorithm>
#include <array>
#include <cassert>
#include <iostream>
#include <utility>
#include <vector>
#include "basic_types.hpp"

/**
 * @brief Carte des phéronomes
 * @details Gère une carte des phéronomes avec leurs mis à jour ( dont l'évaporation )
 *
 */
class pheronome {
public:
    using size_t      = unsigned long;
    using pheronome_t = std::array< double, 2 >;

    /**
     * @brief Construit une carte initiale des phéronomes
     * @details La carte des phéronomes est initialisées à zéro ( neutre )
     *          sauf pour les bords qui sont marqués comme indésirables
     *
     * @param dim Nombre de cellule dans chaque direction
     * @param alpha Paramètre de bruit
     * @param beta Paramêtre d'évaporation
     */
    pheronome( size_t dim, const position_t& pos_food, const position_t& pos_nest,
               double alpha = 0.7, double beta = 0.9999 )
        : m_dim( dim ),
          m_stride( dim + 2 ),
          m_alpha(alpha), m_beta(beta),
          m_map_of_pheronome( m_stride * m_stride, {{0., 0.}} ),
          m_buffer_pheronome( ),
          m_pos_nest( pos_nest ),
          m_pos_food( pos_food ) 
          {
        m_map_of_pheronome[index(pos_food)][0] = 1.;
        m_map_of_pheronome[index(pos_nest)][1] = 1.;
        cl_update( );
        m_buffer_pheronome = m_map_of_pheronome;
    }
    pheronome( const pheronome& ) = delete;
    pheronome( pheronome&& )      = delete;
    ~pheronome( )                 = default;

    pheronome_t& operator( )( long i, long j ) {
        return m_map_of_pheronome[( i + 1 ) * m_stride + ( j + 1 )];
    }

    const pheronome_t& operator( )( long i, long j ) const {
        return m_map_of_pheronome[( i + 1 ) * m_stride + ( j + 1 )];
    }

    pheronome_t& operator[] ( const position_t& pos ) {
      return m_map_of_pheronome[index(pos)];
    }

    const pheronome_t& operator[] ( const position_t& pos ) const {
      return m_map_of_pheronome[index(pos)];
    }

    // MPI helper: expose buffer as a flat array of doubles (2 doubles per cell).
    double*      raw_buffer()       { return reinterpret_cast<double*>(m_buffer_pheronome.data()); }
    std::size_t  raw_buffer_size()  const { return m_buffer_pheronome.size() * 2; }
        std::size_t  dim()    const { return m_dim;    }
        std::size_t  stride() const { return m_stride; }

        // Evaporate only interior rows [row_begin, row_end) — 1-based interior indices.
        // Used by MPI approach 1: each rank evaporates only its assigned stripe so that
        // the evaporation work is O(dim²/P) per rank.  After the distributed evaporation
        // every rank calls update() independently; this is safe because the merged buffer
        // (identical on all ranks after MPI_Allreduce) is modified the same way on every
        // rank for the cells that rank owns, and ants always read m_map_of_pheromone (not
        // m_buffer_pheronome), which is set correctly by each rank's own update() call.
        void do_evaporation_stripe( std::size_t row_begin, std::size_t row_end ) {
            for ( std::size_t i = row_begin; i < row_end; ++i )
                for ( std::size_t j = 1; j <= m_dim; ++j ) {
                    m_buffer_pheronome[i * m_stride + j][0] *= m_beta;
                    m_buffer_pheronome[i * m_stride + j][1] *= m_beta;
                }
        }

        void do_evaporation( ) {
#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
        for ( std::size_t i = 1; i <= m_dim; ++i )
            for ( std::size_t j = 1; j <= m_dim; ++j ) {
                m_buffer_pheronome[i * m_stride + j][0] *= m_beta;
                m_buffer_pheronome[i * m_stride + j][1] *= m_beta;
            }
    }

    void mark_pheronome( const position_t& pos ) {
        long i = pos.x;
        long j = pos.y;
        assert( i >= 0 );
        assert( j >= 0 );
        assert( i < static_cast<long>( m_dim ) );
        assert( j < static_cast<long>( m_dim ) );
        pheronome&        phen        = *this;
        const pheronome_t& left_cell   = phen( i - 1, j );
        const pheronome_t& right_cell  = phen( i + 1, j );
        const pheronome_t& upper_cell  = phen( i, j - 1 );
        const pheronome_t& bottom_cell = phen( i, j + 1 );
        double             v1_left     = std::max( left_cell[0], 0. );
        double             v2_left     = std::max( left_cell[1], 0. );
        double             v1_right    = std::max( right_cell[0], 0. );
        double             v2_right    = std::max( right_cell[1], 0. );
        double             v1_upper    = std::max( upper_cell[0], 0. );
        double             v2_upper    = std::max( upper_cell[1], 0. );
        double             v1_bottom   = std::max( bottom_cell[0], 0. );
        double             v2_bottom   = std::max( bottom_cell[1], 0. );
        m_buffer_pheronome[( i + 1 ) * m_stride + ( j + 1 )][0] =
            m_alpha * std::max( {v1_left, v1_right, v1_upper, v1_bottom} ) +
            ( 1 - m_alpha ) * 0.25 * ( v1_left + v1_right + v1_upper + v1_bottom );
        m_buffer_pheronome[( i + 1 ) * m_stride + ( j + 1 )][1] =
            m_alpha * std::max( {v2_left, v2_right, v2_upper, v2_bottom} ) +
            ( 1 - m_alpha ) * 0.25 * ( v2_left + v2_right + v2_upper + v2_bottom );
    }

    void update( ) {
        m_map_of_pheronome.swap( m_buffer_pheronome );
        cl_update( );
        m_map_of_pheronome[( m_pos_food.x + 1 ) * m_stride + m_pos_food.y + 1][0] = 1;
        m_map_of_pheronome[( m_pos_nest.x + 1 ) * m_stride + m_pos_nest.y + 1][1] = 1;
    }

private:
    size_t index( const position_t& pos ) const
    {
      return (pos.x+1)*m_stride + pos.y + 1;
    }
    /**
     * @brief Mets à jour les conditions limites sur les cellules fantômes
     * @details Mets à jour les conditions limites sur les cellules fantômes :
     *     pour l'instant, on se contente simplement de mettre ces cellules avec
     *     des valeurs à -1 pour être sûr que les fourmis évitent ces cellules
     */
    void cl_update( ) {
        // On mets tous les bords à -1 pour les marquer comme indésirables :
        for ( unsigned long j = 0; j < m_stride; ++j ) {
            m_map_of_pheronome[j]                            = {{-1., -1.}};
            m_map_of_pheronome[j + m_stride * ( m_dim + 1 )] = {{-1., -1.}};
            m_map_of_pheronome[j * m_stride]                 = {{-1., -1.}};
            m_map_of_pheronome[j * m_stride + m_dim + 1]     = {{-1., -1.}};
        }
    }
    unsigned long              m_dim, m_stride;
    double                     m_alpha, m_beta;
    std::vector< pheronome_t > m_map_of_pheronome, m_buffer_pheronome;
    position_t m_pos_nest, m_pos_food;
};

#endif
