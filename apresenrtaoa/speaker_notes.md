# Speaker Notes

## Slide 1

This presentation studies three optimization directions for the ant colony simulation: data layout, OpenMP, and MPI. The structure is simple: first I define the problem and the benchmark, then I show what changed in the code, and finally I show what the measurements say.

If needed, I can summarize the whole project in one sentence: OpenMP gave the strongest practical speedup, SoA gave a small gain, and MPI only became useful when the workload was increased.

## Slide 2

The simulation runs on a 513 by 513 terrain, and the reference workload uses 5000 ants. The dominant cost is ant movement, so that phase is the real target of the optimization work. This is important because if I optimize only a secondary part, the total execution time will not improve much.

Before comparing methods, I corrected two issues in the original code so that the benchmark became reliable. The first one was the random seed of each ant, which was not initialized correctly. The second one was the pheromone buffer refresh, because after each update the buffer had to be rebuilt from the active map to avoid stale values.

If the professor asks why this matters, the answer is simple: performance numbers are only meaningful if the implementation is first correct and deterministic enough to compare.

## Slide 3

For the data layout part, the ant storage was changed from objects to separate arrays. This is called SoA, which means Structure of Arrays. In the original style, each ant was stored as a full object with position, state, and seed together. In the SoA version, each field is stored in its own array.

The practical reason is that the memory layout becomes more regular. In this project I also used linear indices for the terrain and pheromone arrays, and I delayed the pheromone buffer update until the touched cells were known. So the code reduces some redundant writes and some index overhead.

The result was a small but measurable gain, about 1.03 times. So this method helped, but only slightly. The main reason is that the ant movement still has random choices, irregular control flow, and scattered accesses, so the kernel is still not very friendly to vectorization.

If the professor asks what SoA means, I can answer: instead of storing a vector of ant objects, I store one array for positions, one for states, and one for seeds.

## Slide 4

OpenMP was the most effective method in this project. The first attempt only parallelized evaporation, but that was not enough because evaporation was not the main bottleneck. The important change was to parallelize ant movement, which is the dominant phase.

To preserve correctness, each thread first stores its local marks in a private structure. After the parallel region, those local results are merged into the global pheromone buffer. This avoids race conditions, because two threads do not write directly to the same shared cell during the movement phase.

This produced the best result of the project, about 2.29 times faster than the one-thread case. Another useful observation is that the gain already becomes strong around the physical cores, and then it starts to saturate, which is expected.

If the professor asks why OpenMP worked better than SoA, the answer is that OpenMP attacked the dominant phase directly, while SoA improved memory organization but did not fundamentally change the algorithm.

## Slide 5

The MPI version keeps a full copy of the map on each process and distributes only the ants. This means each process computes its own subset of ants locally, but all of them still know the full environment.

The iteration works in three main steps. First, each process updates only its local ants. Second, the pheromone buffer is synchronized with `MPI_Allreduce`, so all processes reconstruct a common pheromone state. Third, evaporation is applied by stripes of the map, and then there is another synchronization so that every process ends the iteration with the same final map.

So when I explain those three points orally, I can say: each process has part of the ants, then all processes merge the pheromone information, and finally they split evaporation and synchronize again.

The method is correct, but the communication cost is high. For that reason, MPI is slower at 5000 ants and becomes more relevant only at larger workloads. The reason is that there are two global synchronizations per iteration, and for small workloads this fixed communication cost dominates the useful computation.

If the professor asks why MPI was not good at 5000 ants, the short answer is: too much communication for too little work.

I can also mention the second MPI approach very briefly: in that version, each process would store only one part of the map, exchange border data with neighbors, and transfer ants when they cross a subdomain boundary. I did not implement that version as code; I described the strategy in the report, which is what the non-bonus requirement asked for.

## Slide 6

This is the fairest comparison because all methods are shown with the same 5000-ant workload. In that common regime, the SoA version is slightly better than the sequential baseline, OpenMP is clearly the best method, and MPI is not competitive.

This slide is useful because it avoids a misleading comparison. I did not increase the workload for OpenMP just to get a better number. Instead, I kept the same common workload and showed that OpenMP already improves performance there.

The practical conclusion for the common case is therefore OpenMP. MPI can still be interesting, but only in a different regime where the amount of computation is large enough to amortize communication.

## Slide 7

The final message is simple. Correctness fixes were necessary before any comparison, because otherwise the benchmark would not be trustworthy. The SoA version improved the code layout and produced a small gain, but it did not change the fundamental behavior of the movement kernel.

OpenMP gave the strongest result because it targeted the dominant phase directly and did so with relatively low overhead. MPI was valid after the build fix, but its communication cost is only amortized at larger workloads.

So the final scientific conclusion is: among the tested modifications, OpenMP gave the best balance between implementation effort and performance gain for the standard workload, while MPI is better interpreted as a scalability study for larger runs.

If needed, I can close with this clarification: the project covers the mandatory requirements of the README, and the only part not implemented as code is the optional MPI bonus corresponding to the second approach.