/*
 * perf_guard_amortization.c -- Track A fixture for evidence-driven guard
 * amortization.
 *
 * The baseline models a slot read that pays owner/generation/capability/state
 * checks on every access. The repeated-preflight path validates the same facts
 * for each access without caching the view. The cached preflight path validates
 * once and then consumes an evidence view in the hot loop.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef PGY_GUARD_ITERATIONS
#define PGY_GUARD_ITERATIONS 50000000
#endif

#if defined(_MSC_VER)
#define PGY_NOINLINE __declspec(noinline)
#else
#define PGY_NOINLINE __attribute__((noinline))
#endif

enum {
    PGY_GUARD_DATA_LEN = 1024,
    PGY_SLOT_CAP_READ = 1u,
    PGY_SLOT_STATE_VALID = 7u
};

typedef struct {
    int32_t *data;
    int32_t length;
    volatile int32_t generation;
    volatile uint32_t capability_mask;
    volatile uint32_t state;
} PgyGuardedSlot;

typedef struct {
    const int32_t *data;
    int32_t length;
    int32_t generation;
    uint32_t capability_mask;
} PgySlotEvidenceView;

static int32_t g_data[PGY_GUARD_DATA_LEN];

static void
init_slot(PgyGuardedSlot *slot)
{
    int32_t seed = 17;

    for (int32_t i = 0; i < PGY_GUARD_DATA_LEN; i++) {
        seed = (seed * 1103515245 + 12345) & 0x7fffffff;
        g_data[i] = (seed >> 8) & 255;
    }
    slot->data = g_data;
    slot->length = PGY_GUARD_DATA_LEN;
    slot->generation = 3;
    slot->capability_mask = PGY_SLOT_CAP_READ;
    slot->state = PGY_SLOT_STATE_VALID;
}

static PGY_NOINLINE int32_t
slot_guarded_read(const PgyGuardedSlot *slot,
                  int32_t index,
                  int32_t expected_generation)
{
    if (slot == NULL || slot->data == NULL)
        return -1;
    if (index < 0 || index >= slot->length)
        return -1;
    if (slot->generation != expected_generation)
        return -1;
    if ((slot->capability_mask & PGY_SLOT_CAP_READ) == 0)
        return -1;
    if (slot->state != PGY_SLOT_STATE_VALID)
        return -1;
    return slot->data[index];
}

static PGY_NOINLINE int
slot_preflight_view(const PgyGuardedSlot *slot,
                    int32_t expected_generation,
                    PgySlotEvidenceView *out)
{
    if (slot == NULL || slot->data == NULL || out == NULL)
        return 0;
    if (slot->length <= 0)
        return 0;
    if (slot->generation != expected_generation)
        return 0;
    if ((slot->capability_mask & PGY_SLOT_CAP_READ) == 0)
        return 0;
    if (slot->state != PGY_SLOT_STATE_VALID)
        return 0;
    out->data = slot->data;
    out->length = slot->length;
    out->generation = expected_generation;
    out->capability_mask = PGY_SLOT_CAP_READ;
    return 1;
}

static PGY_NOINLINE int64_t
run_per_access_guard(const PgyGuardedSlot *slot, int32_t iterations)
{
    int64_t acc = 0;
    int32_t expected_generation = slot->generation;

    for (int32_t i = 0; i < iterations; i++) {
        acc += slot_guarded_read(slot,
                                 i & (PGY_GUARD_DATA_LEN - 1),
                                 expected_generation);
    }
    return acc;
}

static PGY_NOINLINE int64_t
run_preflight_view(const PgyGuardedSlot *slot, int32_t iterations)
{
    PgySlotEvidenceView view;
    int64_t acc = 0;

    if (!slot_preflight_view(slot, slot->generation, &view))
        return -1;
    for (int32_t i = 0; i < iterations; i++)
        acc += view.data[i & (PGY_GUARD_DATA_LEN - 1)];
    return acc;
}

static PGY_NOINLINE int64_t
run_repeated_preflight_view(const PgyGuardedSlot *slot, int32_t iterations)
{
    int64_t acc = 0;

    for (int32_t i = 0; i < iterations; i++) {
        PgySlotEvidenceView view;
        if (!slot_preflight_view(slot, slot->generation, &view))
            return -1;
        acc += view.data[i & (PGY_GUARD_DATA_LEN - 1)];
    }
    return acc;
}

static PGY_NOINLINE int64_t
run_invalid_preflight(PgyGuardedSlot *slot)
{
    PgySlotEvidenceView view;

    slot->generation++;
    return slot_preflight_view(slot, slot->generation - 1, &view) ? 0 : -1;
}

static int
run_timed_mode(const PgyGuardedSlot *slot, const char *mode)
{
    clock_t start;
    clock_t end;
    int64_t sum;

    start = clock();
    if (strcmp(mode, "per") == 0) {
        sum = run_per_access_guard(slot, PGY_GUARD_ITERATIONS);
    } else if (strcmp(mode, "pre") == 0) {
        sum = run_preflight_view(slot, PGY_GUARD_ITERATIONS);
    } else if (strcmp(mode, "repeat-pre") == 0) {
        sum = run_repeated_preflight_view(slot, PGY_GUARD_ITERATIONS);
    } else {
        return 1;
    }
    end = clock();
    if (start == (clock_t)-1 || end == (clock_t)-1 || end < start)
        return 1;

    printf("seconds=%.9f\n", (double)(end - start) / (double)CLOCKS_PER_SEC);
    printf("sum=%lld\n", (long long)sum);
    return sum < 0 ? 1 : 0;
}

int
main(int argc, char **argv)
{
    PgyGuardedSlot slot;
    int64_t per_access;
    int64_t preflight;
    int64_t repeated_preflight;

    init_slot(&slot);
    if (argc == 2 && strcmp(argv[1], "per") == 0) {
        printf("sum=%lld\n",
               (long long)run_per_access_guard(&slot, PGY_GUARD_ITERATIONS));
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "pre") == 0) {
        printf("sum=%lld\n",
               (long long)run_preflight_view(&slot, PGY_GUARD_ITERATIONS));
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "repeat-pre") == 0) {
        printf("sum=%lld\n",
               (long long)run_repeated_preflight_view(&slot,
                                                       PGY_GUARD_ITERATIONS));
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "invalid") == 0) {
        printf("invalid=%lld\n", (long long)run_invalid_preflight(&slot));
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "time-per") == 0)
        return run_timed_mode(&slot, "per");
    if (argc == 2 && strcmp(argv[1], "time-pre") == 0)
        return run_timed_mode(&slot, "pre");
    if (argc == 2 && strcmp(argv[1], "time-repeat-pre") == 0)
        return run_timed_mode(&slot, "repeat-pre");

    per_access = run_per_access_guard(&slot, PGY_GUARD_ITERATIONS);
    preflight = run_preflight_view(&slot, PGY_GUARD_ITERATIONS);
    repeated_preflight =
        run_repeated_preflight_view(&slot, PGY_GUARD_ITERATIONS);
    printf("iterations=%d\n", PGY_GUARD_ITERATIONS);
    printf("per_access_sum=%lld\n", (long long)per_access);
    printf("preflight_sum=%lld\n", (long long)preflight);
    printf("repeated_preflight_sum=%lld\n", (long long)repeated_preflight);
    printf("same=%d\n", per_access == preflight ? 1 : 0);
    printf("repeated_same=%d\n",
           repeated_preflight == preflight ? 1 : 0);
    printf("invalid=%lld\n", (long long)run_invalid_preflight(&slot));
    return per_access == preflight && repeated_preflight == preflight ? 0 : 1;
}
