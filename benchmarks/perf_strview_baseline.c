/*
 * perf_strview_baseline.c -- proof that a non-owning string view closes the
 * one structural allocation gap measured against idiomatic Rust string code.
 *
 * Tight Substring+IndexOf loop. Pergyra's `Substring` returns an owned char*
 * (heap allocation per call); Rust's `&str` slice borrows (no allocation). This
 * benchmark runs the same scan with PgyStrView (borrow, no allocation) and is
 * the C-level evidence behind the perf_close_to_c.md "Versus Rust" section.
 *
 *   gcc -O2 -I<repo-root> benchmarks/perf_strview_baseline.c -o strview
 *
 * Measured (Windows, gcc -O2, 40M iters, best-of-7): view ~189 ms vs an
 * allocating Substring loop ~1587 ms (8.4x) and Rust's zero-alloc slice ~153 ms
 * (1.24x -- near parity). Output must equal the allocating/Rust versions.
 */
#include <stdio.h>
#include "src/runtime/pgy_runtime_strview_inline.h"

int main(void)
{
    const char *s = "the quick brown fox jumps over the lazy dog and runs away fast";
    long long total = 0;
    for (int i = 0; i < 40000000; i++) {
        int start = i % 50;
        PgyStrView v = pgy_strview(s, start, 8);   /* borrow, no allocation */
        total += pgy_strview_indexof(v, "o");
    }
    printf("total=%lld\n", total);
    return 0;
}
