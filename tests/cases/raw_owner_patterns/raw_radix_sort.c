/* Equivalent pointer implementation; explicit lifetime, same workload. */
#include <stdint.h>
#include <stdlib.h>
int32_t PatternIterations(void);
int32_t PatternSeed(void);
void PatternStart(void);
void PatternFinish(int32_t);
static int32_t work(int32_t seed) {
    int32_t *values = calloc(256, sizeof(*values));
    if (values == NULL) abort();
    int32_t *scratch = calloc(256, sizeof(*scratch));
    if (scratch == NULL) abort();
    int32_t *counts = calloc(256, sizeof(*counts));
    if (counts == NULL) abort();
    int32_t x = seed;
    int32_t i = 0;
    while (i < 256) {
        x = (x * 109 + 89) % 65536;
        values[i] = x;
        i = i + 1;
    }
    int32_t pass = 0;
    int32_t divisor = 1;
    while (pass < 2) {
        i = 0;
        while (i < 256) { counts[i] = 0; i = i + 1; }
        i = 0;
        while (i < 256) {
            int32_t bucket = (values[i] / divisor) % 256;
            counts[bucket] = counts[bucket] + 1;
            i = i + 1;
        }
        int32_t total = 0;
        i = 0;
        while (i < 256) {
            int32_t frequency = counts[i];
            counts[i] = total;
            total = total + frequency;
            i = i + 1;
        }
        i = 0;
        while (i < 256) {
            int32_t bucket = (values[i] / divisor) % 256;
            scratch[counts[bucket]] = values[i];
            counts[bucket] = counts[bucket] + 1;
            i = i + 1;
        }
        i = 0;
        while (i < 256) { values[i] = scratch[i]; i = i + 1; }
        divisor = divisor * 256;
        pass = pass + 1;
    }
    int32_t result = 0;
    i = 0;
    while (i < 256) {
        result = (result + (i % 31 + 1) * values[i]) % 1000003;
        i = i + 1;
    }
    free(counts);
    free(scratch);
    free(values);
    return result;
}

int main(void) {
    int32_t count = PatternIterations(), offset = PatternSeed(), checksum = 0;
    PatternStart();
    for (int32_t i = 0; i < count; ++i) checksum = (checksum + work((i + offset) % 251)) % 1000003;
    PatternFinish(checksum);
    return 0;
}
