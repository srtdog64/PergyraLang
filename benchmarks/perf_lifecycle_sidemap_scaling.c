/*
 * Lifecycle side-map lookup scaling: linear-scan (the pre-2026-06-24 runtime)
 * vs O(1) open-addressing hash (the current runtime).
 *
 * The lifecycle guard (pgy_runtime_lifecycle_guard_export /
 * pgy_runtime_lifecycle_slot) interns each subject instance into a fixed side-
 * map keyed by the instance pointer. The original implementation was an
 * O(count) linear scan, so per-guard cost grew with the number of live tracked
 * subjects -- a latent scalability cliff for subject-heavy programs (e.g. a
 * game with hundreds of stateful entities). This benchmark replicates both
 * shapes faithfully and reports per-lookup ns and the speedup at several live-
 * subject counts.
 *
 * Build/run (mingw):
 *   gcc -O3 -std=c11 benchmarks/perf_lifecycle_sidemap_scaling.c -o sidemap && ./sidemap
 *
 * Measured (2026-06-24, x86-64, gcc -O3):
 *   subj   linear_ns   hash_ns   speedup
 *   1      1.87        1.99      0.94   (wash: hash mix ~= 1-element scan)
 *   8      2.73        1.89      1.44
 *   64     18.89       2.54      7.43
 *   200    47.92       2.69      17.83
 *
 * Conclusion: the hash is a large win for many live subjects and a ~6%
 * regression only at exactly one subject. The runtime now uses the hash
 * (src/runtime/pgy_runtime_lib_authority_file_core.h and
 * src/runtime/pgy_runtime_panic_checked_inline.h, kept as twins).
 */
#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <stddef.h>

#define CAP 256
typedef struct { const void *key; int32_t state; } Entry;

/* ---- linear-scan (original runtime shape) ---- */
static Entry lin[CAP];
static int lin_count = 0;
static Entry *lin_slot(const void *inst) {
    for (int i = 0; i < lin_count; i++)
        if (lin[i].key == inst) return &lin[i];
    if (lin_count >= CAP) return NULL;
    lin[lin_count].key = inst; lin[lin_count].state = 0;
    return &lin[lin_count++];
}

/* ---- open-addressing hash (current runtime shape) ---- */
static Entry hsh[CAP]; /* key==NULL == empty (zero-init) */
static Entry *hsh_slot(const void *inst) {
    size_t cap = (size_t)CAP;
    uintptr_t h = (uintptr_t)inst;
    h ^= h >> 7; h *= (uintptr_t)2654435761u; h ^= h >> 11;
    size_t start = (size_t)h % cap;
    for (size_t p = 0; p < cap; p++) {
        size_t i = (start + p) % cap;
        if (hsh[i].key == inst) return &hsh[i];
        if (hsh[i].key == NULL) { hsh[i].key = inst; hsh[i].state = 0; return &hsh[i]; }
    }
    return NULL;
}

static double now_s(void) { return (double)clock() / (double)CLOCKS_PER_SEC; }
static char pool[CAP][8];

int main(void) {
    const int64_t N = 300000000LL;
    int counts[] = { 1, 8, 64, 200 };
    printf("%-6s %-12s %-12s %-8s\n", "subj", "linear_ns", "hash_ns", "speedup");
    for (size_t c = 0; c < sizeof(counts) / sizeof(counts[0]); c++) {
        int K = counts[c];
        lin_count = 0;
        for (int i = 0; i < CAP; i++) hsh[i].key = NULL;
        for (int k = 0; k < K; k++) { lin_slot(pool[k]); hsh_slot(pool[k]); }
        volatile int64_t sink = 0;
        double t0 = now_s();
        for (int64_t i = 0; i < N; i++) { Entry *e = lin_slot(pool[i % K]); sink += e->state; }
        double t1 = now_s();
        for (int64_t i = 0; i < N; i++) { Entry *e = hsh_slot(pool[i % K]); sink += e->state; }
        double t2 = now_s();
        double lns = (t1 - t0) / (double)N * 1e9, hns = (t2 - t1) / (double)N * 1e9;
        printf("%-6d %-12.3f %-12.3f %-8.2f\n", K, lns, hns, lns / hns);
        (void)sink;
    }
    return 0;
}
