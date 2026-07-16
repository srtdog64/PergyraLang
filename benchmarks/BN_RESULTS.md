# B_n cycle-tally benchmark -- same-machine results

Signed-permutation (hyperoctahedral B_n) cycle distribution, n=10 =
3,715,891,200 signed permutations, single-method Loops. All numbers are from ONE
machine (this Windows box, 16 logical cores, mingw gcc/gfortran -O2). They are
NOT comparable to another machine's numbers (e.g. an Apple M4 Max reference
suite); only same-machine ratios mean anything.

Every implementation prints the same verified totals (n=4 -> 384,
n=10 -> 3,715,891,200), so this is apples-to-apples on identical output.

## n=10

| language / backend        | serial   | parallel            | serial vs C | parallel speedup |
|---------------------------|----------|---------------------|-------------|------------------|
| hand-C  (gcc -O2)         | 301.57s  | 45.26s (OpenMP)     | 1.00x       | 6.66x            |
| Fortran (gfortran -O2)    | 404.0s   | 75.71s (OpenMP)     | 1.34x       | 5.34x            |
| Pergyra (pgy --backend=c) | 386.67s  | 119.43s (`join with sum`) | 1.28x | 3.24x       |

## n=9

| language | serial  | parallel |
|----------|---------|----------|
| hand-C   | 13.96s  | 2.22s    |
| Fortran  | 19.33s  | 3.43s    |
| Pergyra  | 15.94s  | 5.30s    |

## Honest reading

1. **Serial: Pergyra is C-class.** 1.28x hand-C at n=10 (28% overhead) with
   always-on array bounds checking, on a pointer-chasing / branch-heavy integer
   workload -- not arithmetic loops. Pergyra also edges gfortran here (0.96x at
   n=10, 0.82x at n=9): this combinatorial workload is not Fortran's dense-array
   home turf, and Pergyra's C backend rides gcc while gfortran's frontend is
   weaker on this shape. No general Fortran-parity claim is made (see docs/168);
   this is one measured workload.

2. **Parallel: Pergyra's parallelism is real but its runtime trails OpenMP.**
   The whole parallelization is one `parallel (p in 0..n) join with sum` line
   plus per-task-local scratch (the compiler forbids shared mutable capture, so
   it is data-race-free by construction). It gives a correct 3.24x speedup, but
   mature OpenMP extracts 6.66x (C) / 5.34x (Fortran) from the same
   decomposition.

3. **The parallel limiter is the runtime, not the decomposition.** Refining the
   prefix from the first element (10 tasks) to the first two (90 tasks) barely
   moved Pergyra (119.43s -> 115.35s, ~3%), so load balancing is not the
   bottleneck -- the lane-scheduler runtime's per-task / synchronization
   overhead is. That is the concrete thing to improve (the SEA lane runtime is a
   partially-filled facade), and it is identified here by measurement rather than
   guessed.

## Files

- `perf_bn_cycles.pgy`          both-methods oracle (Loops + union-find cross-check)
- `perf_bn_cycles_loops.pgy`    serial single-method Loops
- `perf_bn_cycles_parallel.pgy` prefix-parallel Loops (`join with sum`)
- `baseline_bn_loops.c` / `.f90`   hand-C / Fortran serial
- `baseline_bn_par.c` / `.f90`     hand-C / Fortran OpenMP parallel
- `run_bn_compare.sh`           manual reproduction runner

## Tarjan (union-find, no rank / no compression), n=9

| language | serial  | vs C  |
|----------|---------|-------|
| hand-C   | 26.35s  | 1.00x |
| Pergyra  | 29.76s  | 1.13x |

Tarjan is ~1.87x the Loops variant on this machine (two parent-chain walks per
edge with no path compression); Pergyra stays C-class (1.13x, matching Loops's
1.14x) on both algorithms. `perf_bn_cycles_tarjan.pgy` / `baseline_bn_tarjan.c`.
