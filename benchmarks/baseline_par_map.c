/* baseline_par_map.c -- C twins for the map benchmarks.
 * Body everywhere: v(i) = ((i % 1000) * 31 + 7) % 100, 32-bit-safe.
 * Modes:
 *   serial  <n>   plain loop (speedup denominator)
 *   ompfor  <n>   #pragma omp parallel for reduction (runtime-chunked idiom)
 *   omptask <n>   taskloop grainsize(1): ONE TASK PER ELEMENT -- the
 *                 apples-to-apples twin of Pergyra's task-per-index lowering
 * Build: gcc -O2 -fopenmp baseline_par_map.c -o baseline_par_map
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: %s serial|ompfor|omptask <n>\n", argv[0]); return 2; }
    const char *mode = argv[1];
    long n = atol(argv[2]);
    long long total = 0;

    if (strcmp(mode, "serial") == 0) {
        for (long i = 0; i < n; i++)
            total += ((i % 1000) * 31 + 7) % 100;
    } else if (strcmp(mode, "ompfor") == 0) {
        #pragma omp parallel for reduction(+:total)
        for (long i = 0; i < n; i++)
            total += ((i % 1000) * 31 + 7) % 100;
    } else if (strcmp(mode, "omptask") == 0) {
        #pragma omp parallel
        #pragma omp single
        {
            #pragma omp taskloop grainsize(1) reduction(+:total)
            for (long i = 0; i < n; i++)
                total += ((i % 1000) * 31 + 7) % 100;
        }
    } else {
        fprintf(stderr, "unknown mode %s\n", mode);
        return 2;
    }
    printf("total=%lld\n", total);
    return 0;
}
