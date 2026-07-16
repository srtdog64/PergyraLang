/* baseline_par_fib.c -- C twins for the nested fork-join benchmark.
 * fib(38), serial cutoff below 28 (same shape as perf_parallel_fib.pgy).
 * Modes: serial | omptask
 * Build: gcc -O2 -fopenmp baseline_par_fib.c -o baseline_par_fib
 */
#include <stdio.h>
#include <string.h>

static int fib_ser(int n)
{
    if (n < 2) return n;
    return fib_ser(n - 1) + fib_ser(n - 2);
}

static int fib_par(int n)
{
    if (n < 28) return fib_ser(n);
    int a = 0, b = 0;
    #pragma omp task shared(a)
    a = fib_par(n - 1);
    b = fib_par(n - 2);
    #pragma omp taskwait
    return a + b;
}

int main(int argc, char **argv)
{
    int result;
    if (argc > 1 && strcmp(argv[1], "serial") == 0) {
        result = fib_ser(38);
    } else {
        #pragma omp parallel
        #pragma omp single
        result = fib_par(38);
    }
    printf("total=%d\n", result);
    return 0;
}
