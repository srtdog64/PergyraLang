/* Equivalent pointer implementation; explicit lifetime, same workload. */
#include <stdint.h>
#include <stdlib.h>
int32_t PatternIterations(void);
int32_t PatternSeed(void);
void PatternStart(void);
void PatternFinish(int32_t);
static int32_t work(int32_t seed) {
    int32_t *offsets = calloc(65, sizeof(*offsets));
    if (offsets == NULL) abort();
    int32_t *targets = calloc(256, sizeof(*targets));
    if (targets == NULL) abort();
    int32_t *weights = calloc(256, sizeof(*weights));
    if (weights == NULL) abort();
    int32_t *distances = calloc(64, sizeof(*distances));
    if (distances == NULL) abort();
    int32_t *positions = calloc(64, sizeof(*positions));
    if (positions == NULL) abort();
    int32_t *heap = calloc(64, sizeof(*heap));
    if (heap == NULL) abort();
    int32_t *heuristic = calloc(64, sizeof(*heuristic));
    if (heuristic == NULL) abort();
    int32_t node = 0;
    int32_t edge = 0;
    while (node < 64) {
        offsets[node] = edge;
        int32_t direction = 0;
        while (direction < 4) {
            int32_t target = -1;
            if (direction == 0 && node % 8 < 7) { target = node + 1; }
            if (direction == 1 && node / 8 < 7) { target = node + 8; }
            if (direction == 2 && node % 8 > 0) { target = node - 1; }
            if (direction == 3 && node / 8 > 0) { target = node - 8; }
            if (target >= 0) {
                targets[edge] = target;
                weights[edge] = (node * 17 + target * 13 + seed) % 9 + 1;
                edge = edge + 1;
            }
            direction = direction + 1;
        }
        distances[node] = 1000000000;
        positions[node] = -1;
        heuristic[node] = 0;
        node = node + 1;
    }
    offsets[64] = edge;
    int32_t *adjacent = targets;
    int32_t *costs = weights;
    distances[0] = 0;
    heap[0] = 0;
    positions[0] = 0;
    int32_t count = 1;
    while (count > 0) {
        int32_t current = heap[0];
        count = count - 1;
        positions[current] = -2;
        if (count > 0) {
            heap[0] = heap[count];
            positions[heap[0]] = 0;
            int32_t at = 0;
            while (at * 2 + 1 < count) {
                int32_t child = at * 2 + 1;
                if (child + 1 < count) {
                    int32_t left = heap[child];
                    int32_t right = heap[child + 1];
                    if (distances[right] + heuristic[right] < distances[left] + heuristic[left]) {
                        child = child + 1;
                    }
                }
                int32_t parentNode = heap[at];
                int32_t childNode = heap[child];
                if (distances[parentNode] + heuristic[parentNode] <= distances[childNode] + heuristic[childNode]) { break; }
                heap[at] = childNode;
                heap[child] = parentNode;
                positions[childNode] = at;
                positions[parentNode] = child;
                at = child;
            }
        }
        if (current == 63) { break; }
        edge = offsets[current];
        while (edge < offsets[current + 1]) {
            int32_t target = adjacent[edge];
            int32_t candidate = distances[current] + costs[edge];
            if (positions[target] != -2 && candidate < distances[target]) {
                distances[target] = candidate;
                int32_t at = positions[target];
                if (at < 0) {
                    at = count;
                    heap[at] = target;
                    positions[target] = at;
                    count = count + 1;
                }
                while (at > 0) {
                    int32_t parent = (at - 1) / 2;
                    int32_t parentNode = heap[parent];
                    if (distances[parentNode] + heuristic[parentNode] <= candidate + heuristic[target]) { break; }
                    heap[at] = parentNode;
                    heap[parent] = target;
                    positions[parentNode] = at;
                    positions[target] = parent;
                    at = parent;
                }
            }
            edge = edge + 1;
        }
    }
    int32_t result = distances[63];
    free(heuristic);
    free(heap);
    free(positions);
    free(distances);
    free(weights);
    free(targets);
    free(offsets);
    return result;
}

int main(void) {
    int32_t count = PatternIterations(), offset = PatternSeed(), checksum = 0;
    PatternStart();
    for (int32_t i = 0; i < count; ++i) checksum = (checksum + work((i + offset) % 251)) % 1000003;
    PatternFinish(checksum);
    return 0;
}
