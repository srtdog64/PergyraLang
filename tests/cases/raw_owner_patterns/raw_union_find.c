/* Equivalent pointer implementation; explicit lifetime, same workload. */
#include <stdint.h>
#include <stdlib.h>
int32_t PatternIterations(void);
int32_t PatternSeed(void);
void PatternStart(void);
void PatternFinish(int32_t);
static int32_t work(int32_t seed) {
    int32_t *parents = calloc(256, sizeof(*parents));
    if (parents == NULL) abort();
    int32_t *sizes = calloc(256, sizeof(*sizes));
    if (sizes == NULL) abort();
    int32_t i = 0;
    while (i < 256) { parents[i] = i; sizes[i] = 1; i = i + 1; }
    int32_t components = 256;
    int32_t x = seed + 1;
    i = 0;
    while (i < 512) {
        x = (x * 109 + 89) % 65536;
        int32_t a = (x / 256) % 256;
        x = (x * 109 + 89) % 65536;
        int32_t b = (x / 256) % 256;
        while (parents[a] != a) { parents[a] = parents[parents[a]]; a = parents[a]; }
        while (parents[b] != b) { parents[b] = parents[parents[b]]; b = parents[b]; }
        if (a != b) {
            if (sizes[a] < sizes[b]) { int32_t swap = a; a = b; b = swap; }
            parents[b] = a;
            sizes[a] = sizes[a] + sizes[b];
            components = components - 1;
        }
        i = i + 1;
    }
    int32_t result = components * 1009;
    i = 0;
    while (i < 256) {
        int32_t root = i;
        while (parents[root] != root) { parents[root] = parents[parents[root]]; root = parents[root]; }
        result = (result + (i + 1) * sizes[root]) % 1000003;
        i = i + 1;
    }
    free(sizes);
    free(parents);
    return result;
}

int main(void) {
    int32_t count = PatternIterations(), offset = PatternSeed(), checksum = 0;
    PatternStart();
    for (int32_t i = 0; i < count; ++i) checksum = (checksum + work((i + offset) % 251)) % 1000003;
    PatternFinish(checksum);
    return 0;
}
