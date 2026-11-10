#include <stdint.h>
#include <stdlib.h>

int32_t PatternIterations(void);
void PatternStart(void);
void PatternFinish(int32_t);

static int32_t work(int32_t seed)
{
#ifdef RAW_GROW
    int32_t *values = NULL;
    size_t capacity = 0;
    for (size_t i = 0; i < 64; ++i) {
        if (i == capacity) {
            capacity = capacity != 0 ? capacity * 2 : 8;
            int32_t *grown = realloc(values, capacity * sizeof(*values));
            if (grown == NULL) { free(values); abort(); }
            values = grown;
        }
        values[i] = (seed + (int32_t)i * 7) % 1024;
    }
#else
    int32_t *values = malloc(64 * sizeof(*values));
    if (values == NULL) abort();
    for (int32_t i = 0; i < 64; ++i) values[i] = (seed + i * 7) % 1024;
#endif
    int32_t *view = values + 8;
    int32_t result = 0;
    for (int32_t turn = 0; turn < 96; ++turn) {
        int32_t j = (turn * 13 + seed) % 48;
        view[j] = (view[j] + turn + seed) % 1024;
        result += view[j];
    }
    free(values);
    return result;
}

int main(void)
{
    int32_t count = PatternIterations(), checksum = 0;
    PatternStart();
    for (int32_t i = 0; i < count; ++i) checksum = (checksum + work(i % 251)) % 1000003;
    PatternFinish(checksum);
    return 0;
}
