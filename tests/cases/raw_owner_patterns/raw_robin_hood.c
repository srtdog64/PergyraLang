/* Equivalent pointer implementation; explicit lifetime, same workload. */
#include <stdint.h>
#include <stdlib.h>
int32_t PatternIterations(void);
int32_t PatternSeed(void);
void PatternStart(void);
void PatternFinish(int32_t);
static int32_t work(int32_t seed) {
    int32_t *keys = calloc(256, sizeof(*keys));
    if (keys == NULL) abort();
    int32_t *values = calloc(256, sizeof(*values));
    if (values == NULL) abort();
    int32_t *distances = calloc(256, sizeof(*distances));
    if (distances == NULL) abort();
    int32_t i = 0;
    while (i < 192) {
        int32_t key = (i * 37 + seed) % 1009 + 1;
        int32_t value = key * 3 + seed;
        int32_t distance = 0;
        int32_t slot = ((key * 97) % 1009) % 256;
        while (keys[slot] != 0) {
            if (distance > distances[slot]) {
                int32_t oldKey = keys[slot];
                int32_t oldValue = values[slot];
                int32_t oldDistance = distances[slot];
                keys[slot] = key;
                values[slot] = value;
                distances[slot] = distance;
                key = oldKey;
                value = oldValue;
                distance = oldDistance;
            }
            slot = (slot + 1) % 256;
            distance = distance + 1;
        }
        keys[slot] = key;
        values[slot] = value;
        distances[slot] = distance;
        i = i + 1;
    }
    i = 0;
    while (i < 192) {
        int32_t key = (i * 37 + seed) % 1009 + 1;
        int32_t slot = ((key * 97) % 1009) % 256;
        while (keys[slot] != key) { slot = (slot + 1) % 256; }
        int32_t next = (slot + 1) % 256;
        while (keys[next] != 0 && distances[next] > 0) {
            keys[slot] = keys[next];
            values[slot] = values[next];
            distances[slot] = distances[next] - 1;
            slot = next;
            next = (next + 1) % 256;
        }
        keys[slot] = 0;
        values[slot] = 0;
        distances[slot] = 0;
        i = i + 2;
    }
    int32_t result = 0;
    i = 0;
    while (i < 192) {
        int32_t key = (i * 37 + seed) % 1009 + 1;
        int32_t slot = ((key * 97) % 1009) % 256;
        int32_t distance = 0;
        int32_t found = 0;
        while (keys[slot] != 0 && distance <= distances[slot]) {
            if (keys[slot] == key) { found = values[slot]; break; }
            distance = distance + 1;
            slot = (slot + 1) % 256;
        }
        result = (result + (i % 31 + 1) * found) % 1000003;
        i = i + 1;
    }
    free(distances);
    free(values);
    free(keys);
    return result;
}

int main(void) {
    int32_t count = PatternIterations(), offset = PatternSeed(), checksum = 0;
    PatternStart();
    for (int32_t i = 0; i < count; ++i) checksum = (checksum + work((i + offset) % 251)) % 1000003;
    PatternFinish(checksum);
    return 0;
}
