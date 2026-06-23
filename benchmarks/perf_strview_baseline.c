/*
 * perf_strview_baseline.c -- proof that a non-owning string view closes the
 * structural allocation gap in tight string-window scans.
 *
 * Tight Substring+IndexOf loop. Pergyra's `Substring` returns an owned char*
 * (heap allocation per call). This benchmark runs the same scan with PgyStrView
 * (borrow, no allocation) and is the C-level evidence behind the allocation-gap
 * section in perf_close_to_c.md.
 *
 *   gcc -O2 -I<repo-root> benchmarks/perf_strview_baseline.c -o strview
 *
 * The stable lesson is structural: the allocating Substring loop is the slow
 * path, while PgyStrView is the no-allocation window path. Output must equal
 * the allocating version.
 */
#include <stdio.h>
#include "src/runtime/pgy_runtime_strview_inline.h"

int main(void)
{
    const char *s = "the quick brown fox jumps over the lazy dog and runs away fast";
    const int32_t source_len = (int32_t)strlen(s);
    long long total = 0;
    for (int i = 0; i < 40000000; i++) {
        int start = i % 50;
        PgyStrView v = pgy_strview_with_len(s, source_len, start, 8); /* borrow */
        total += pgy_strview_indexof(v, "o");
    }
    printf("total=%lld\n", total);
    return 0;
}
