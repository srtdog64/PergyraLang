/* Equivalent pointer implementation; explicit lifetime, same workload. */
#include <stdint.h>
#include <stdlib.h>
int32_t PatternIterations(void);
int32_t PatternSeed(void);
void PatternStart(void);
void PatternFinish(int32_t);
static int32_t work(int32_t seed) {
    int32_t *lookup = calloc(128, sizeof(*lookup));
    if (lookup == NULL) abort();
    int32_t *keys = calloc(64, sizeof(*keys));
    if (keys == NULL) abort();
    int32_t *values = calloc(64, sizeof(*values));
    if (values == NULL) abort();
    int32_t *previous = calloc(64, sizeof(*previous));
    if (previous == NULL) abort();
    int32_t *following = calloc(64, sizeof(*following));
    if (following == NULL) abort();
    int32_t i = 0;
    while (i < 128) { lookup[i] = -1; i = i + 1; }
    int32_t head = -1;
    int32_t tail = -1;
    int32_t count = 0;
    int32_t result = 0;
    i = 0;
    while (i < 512) {
        int32_t key = (i * 37 + seed + i / 17) % 128;
        int32_t at = lookup[key];
        if (at >= 0) {
            result = (result + values[at]) % 1000003;
            if (at != head) {
                int32_t before = previous[at];
                int32_t after = following[at];
                following[before] = after;
                if (after >= 0) { previous[after] = before; }
                if (at == tail) { tail = before; }
                previous[head] = at;
                following[at] = head;
                previous[at] = -1;
                head = at;
            }
        } else {
            if (count < 64) {
                at = count;
                count = count + 1;
            } else {
                at = tail;
                lookup[keys[at]] = -1;
                tail = previous[at];
                following[tail] = -1;
            }
            keys[at] = key;
            values[at] = key * 3 + seed;
            lookup[key] = at;
            previous[at] = -1;
            following[at] = head;
            if (head >= 0) { previous[head] = at; }
            head = at;
            if (tail < 0) { tail = at; }
        }
        i = i + 1;
    }
    int32_t at = head;
    int32_t rank = 1;
    while (at >= 0) {
        result = (result + rank * (keys[at] + values[at])) % 1000003;
        at = following[at];
        rank = rank + 1;
    }
    free(following);
    free(previous);
    free(values);
    free(keys);
    free(lookup);
    return result;
}

int main(void) {
    int32_t count = PatternIterations(), offset = PatternSeed(), checksum = 0;
    PatternStart();
    for (int32_t i = 0; i < count; ++i) checksum = (checksum + work((i + offset) % 251)) % 1000003;
    PatternFinish(checksum);
    return 0;
}
