/**
 * bench_mpi.cpp — MPI Approach 1 benchmark
 *
 * Implements the first parallelisation strategy from the project statement:
 *   "each process holds the full environment and controls only a subset of ants;
 *    during the evaporation phase each process handles a part of the map."
 *
 * Per-iteration protocol (see comments in pheronome.hpp for rationale):
 *   1. Each rank advances its m/P local ants (reads m_map, writes m_buffer).
 *   2. MPI_Allreduce(MPI_MAX) merges all rank buffers: cells visited by any rank
 *      take the maximum computed mark value as specified in the PDF.
 *   3. Each rank evaporates ONLY its assigned row stripe of the merged buffer
 *      (do_evaporation_stripe).
 *   4. A second global synchronization rebuilds one identical evaporated buffer
 *      on all ranks before update() (MPI_Allreduce with MPI_MIN).
 *   5. phen.update() swaps buffer → map and resets ghost cells / food / nest.
 *
 * Grid size note: fractal_land(8, 2, 1., 1024) → 1024 is the random SEED, not the
 * grid dimension.  Actual dim = nbSeeds * 2^ln2_dim + 1 = 2*256+1 = 513.
 * Buffer = (513+2)^2 * 2 = 530 450 doubles ≈ 4 MiB per MPI_Allreduce.
 *
 * Build:  (from MSYS2 bash with ucrt64 in PATH)
 *   c++.exe -IC:/msys64/ucrt64/include -LC:/msys64/ucrt64/lib \
 *           -std=c++17 -O2 -march=native \
 *           ant.cpp fractal_land.cpp bench_mpi.cpp \
 *           -l:libmsmpi.dll.a -o bench_mpi.exe
 * Run:    mpiexec -n <P> .\bench_mpi.exe
 */

// SDL2/SDL.h (via basic_types.hpp) defines "main → SDL_main"; suppress it so
// that the MPI main() entry point is left intact.
#define SDL_MAIN_HANDLED
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <vector>

#include <mpi.h>

#include "ant.hpp"
#include "fractal_land.hpp"
#include "pheronome.hpp"
#include "rand_generator.hpp"

static int bench_mpi_main(int argc, char** argv)
{
    MPI_Init(&argc, &argv);

    int rank, nproc;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nproc);

    // ── Simulation parameters (identical defaults to bench_seq.cpp) ─────────
    std::size_t seed            = 2026;
    int         nb_ants         = 5000;
    const double eps            = 0.8;
    const double alpha          = 0.7;
    const double beta           = 0.999;
    int         max_iterations  = 300;

    // CLI: mpiexec -n P .\bench_mpi.exe [nb_ants] [iterations]
    if (argc >= 2) {
        nb_ants = std::max(1, std::atoi(argv[1]));
    }
    if (argc >= 3) {
        max_iterations = std::max(1, std::atoi(argv[2]));
    }

    position_t pos_nest{256, 256};
    position_t pos_food{500, 500};

    // ── Build fractal land (deterministic, same seed → same map on all ranks) ─
    fractal_land land(8, 2, 1., 1024);
    double max_val = 0.0, min_val = 0.0;
    for (fractal_land::dim_t i = 0; i < land.dimensions(); ++i)
        for (fractal_land::dim_t j = 0; j < land.dimensions(); ++j) {
            max_val = std::max(max_val, land(i, j));
            min_val = std::min(min_val, land(i, j));
        }
    const double delta = max_val - min_val;
    for (fractal_land::dim_t i = 0; i < land.dimensions(); ++i)
        for (fractal_land::dim_t j = 0; j < land.dimensions(); ++j)
            land(i, j) = (land(i, j) - min_val) / delta;

    // ── Generate all ants with the same seed on every rank, then partition ───
    ant::set_exploration_coef(eps);
    std::vector<ant> all_ants;
    all_ants.reserve(nb_ants);
    auto gen_pos = [&land, &seed]() {
        return rand_int32(0, static_cast<std::int32_t>(land.dimensions() - 1), seed);
    };
    for (int i = 0; i < nb_ants; ++i)
        all_ants.emplace_back(position_t{gen_pos(), gen_pos()}, seed);

    // Partition: rank r owns ants [start, end)
    int local_n   = nb_ants / nproc;
    int remainder = nb_ants % nproc;
    int start     = rank * local_n + std::min(rank, remainder);
    int end       = start + local_n + (rank < remainder ? 1 : 0);
    std::vector<ant> ants(all_ants.begin() + start, all_ants.begin() + end);

    // ── Pheromone map: full copy on each rank ────────────────────────────────
    pheronome phen(land.dimensions(), pos_food, pos_nest, alpha, beta);
    std::size_t food_quantity = 0;

    const int    buf_count = static_cast<int>(phen.raw_buffer_size());
    double* const buf_ptr  = phen.raw_buffer();

    // ── Timed loop ───────────────────────────────────────────────────────────
    double total_ants_ms      = 0.0;
    double total_reduce_ms    = 0.0;
    double total_evap_ms      = 0.0;
    double total_sync_ms      = 0.0;
    double total_update_ms    = 0.0;

    // Evaporation stripe for this rank: interior rows are 1-based [1, dim].
    const std::size_t dim_grid = phen.dim();
    std::size_t stripe_begin = 1 + rank       * dim_grid / nproc;
    std::size_t stripe_end   = 1 + (rank + 1) * dim_grid / nproc;
    if (rank == nproc - 1) stripe_end = dim_grid + 1;  // absorb remainder

    for (int iter = 0; iter < max_iterations; ++iter) {

        // Phase 1: local ant movement
        auto t0 = std::chrono::high_resolution_clock::now();
        for (auto& a : ants)
            a.advance(phen, land, pos_food, pos_nest, food_quantity);
        auto t1 = std::chrono::high_resolution_clock::now();

        // Phase 2: merge pheromone buffers across all ranks (element-wise MAX)
        MPI_Allreduce(MPI_IN_PLACE, buf_ptr, buf_count, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
        auto t2 = std::chrono::high_resolution_clock::now();

        // Phase 3: distributed evaporation — each rank evaporates only its stripe.
        phen.do_evaporation_stripe(stripe_begin, stripe_end);
        auto t3 = std::chrono::high_resolution_clock::now();

        // Phase 4: rebuild one identical evaporated buffer on all ranks.
        // For interior cells, pheromone values are non-negative and beta < 1,
        // so evaporated values are <= non-evaporated ones; MPI_MIN therefore
        // selects the evaporated value for each cell globally.
        MPI_Allreduce(MPI_IN_PLACE, buf_ptr, buf_count, MPI_DOUBLE, MPI_MIN, MPI_COMM_WORLD);
        auto t4 = std::chrono::high_resolution_clock::now();

        // Phase 5: swap buffer/map, reset ghost cells and source cells
        phen.update();
        auto t5 = std::chrono::high_resolution_clock::now();

        total_ants_ms   += std::chrono::duration<double, std::milli>(t1 - t0).count();
        total_reduce_ms += std::chrono::duration<double, std::milli>(t2 - t1).count();
        total_evap_ms   += std::chrono::duration<double, std::milli>(t3 - t2).count();
        total_sync_ms   += std::chrono::duration<double, std::milli>(t4 - t3).count();
        total_update_ms += std::chrono::duration<double, std::milli>(t5 - t4).count();
    }

    // ── Collect worst-case (wall-time = slowest rank) on rank 0 ─────────────
    double g_ants = 0, g_reduce = 0, g_evap = 0, g_sync = 0, g_update = 0;
    MPI_Reduce(&total_ants_ms,   &g_ants,   1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&total_reduce_ms, &g_reduce, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&total_evap_ms,   &g_evap,   1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&total_sync_ms,   &g_sync,   1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&total_update_ms, &g_update, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        double total_ms = g_ants + g_reduce + g_evap + g_sync + g_update;
        double N = static_cast<double>(max_iterations);
        std::ostringstream report;
        report << "=== MPI benchmark: P=" << nproc
               << " over " << max_iterations << " iterations ===\n";
        report << "Total ants: " << nb_ants << "\n";
        report << "Grid dim  : " << dim_grid << " (seed=1024, not dim)\n";
        report << "Ants/rank : " << (end - start) << "\n";
        report << "Buf size  : " << buf_count << " doubles ("
               << (buf_count * 8.0 / (1024.0 * 1024.0)) << " MiB per allreduce)\n\n";
        report << "Phase           | ms/iter\n";
        report << "----------------|--------\n";
        report << "Ant movement    | " << g_ants / N << "\n";
        report << "MPI merge (MAX) | " << g_reduce / N << "\n";
        report << "Evaporation     | " << g_evap / N << "\n";
        report << "MPI sync (MIN)  | " << g_sync / N << "\n";
        report << "Pheromone upd   | " << g_update / N << "\n";
        report << "Total compute   | " << total_ms / N << "\n";

        std::cout << report.str();
    }

    MPI_Finalize();
    return 0;
}

#ifdef main
#undef main
#endif

int main(int argc, char** argv)
{
    return bench_mpi_main(argc, argv);
}
