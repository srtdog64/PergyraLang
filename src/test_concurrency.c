#define _POSIX_C_SOURCE 200809L
#define PGY_ZONE_THREADSAFE 1

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdatomic.h>
#include <time.h>

#include "runtime/pgy_runtime.h"
#include "runtime/pgy_parallel.h"
#include "runtime/pgy_channel.h"

static int tests_run = 0;
static int tests_failed = 0;

#define EXPECT(cond) \
    do { \
        if (!(cond)) { \
            printf("[FAIL] %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            tests_failed++; \
            return; \
        } \
    } while (0)

#define TEST(name) \
    do { \
        tests_run++; \
        printf("TEST %s\n", (name)); \
    } while (0)

static void
sleep_ms(long ms)
{
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

static atomic_int active_workers;
static atomic_int max_active_workers;
static atomic_int zone_reads;
static atomic_int zone_true_reads;
static atomic_int zone_false_reads;

static void
record_parallelism(int current)
{
    int previous = atomic_load(&max_active_workers);
    while (current > previous
           && !atomic_compare_exchange_weak(&max_active_workers, &previous, current)) {
    }
}

static void *
overlap_task(void *arg)
{
    (void)arg;
    int current = atomic_fetch_add(&active_workers, 1) + 1;
    record_parallelism(current);
    sleep_ms(120);
    atomic_fetch_sub(&active_workers, 1);
    return NULL;
}

static void
test_spawn_runs_concurrently(void)
{
    TEST("spawn runs on multiple workers");
    atomic_store(&active_workers, 0);
    atomic_store(&max_active_workers, 0);

    pgy_pool_init(2);
    PgyTaskHandle first = pgy_spawn(overlap_task, NULL);
    PgyTaskHandle second = pgy_spawn(overlap_task, NULL);

    pgy_await_void(first);
    pgy_await_void(second);
    pgy_pool_shutdown();

    EXPECT(atomic_load(&max_active_workers) >= 2);
}

static void *
producer_task(void *arg)
{
    PgyChannel_Int *channel = (PgyChannel_Int *)arg;
    pgy_channel_send_Int(channel, 7);
    pgy_channel_send_Int(channel, 11);
    pgy_channel_close_Int(channel);
    return NULL;
}

static void
test_channel_transfers_between_threads(void)
{
    TEST("channel send/recv works across threads");
    pgy_pool_init(2);

    PgyChannel_Int channel;
    int32_t first = 0;
    int32_t second = 0;
    bool ok1;
    bool ok2;
    bool closed;
    int32_t remaining;

    pgy_channel_init_Int(&channel, 1);
    PgyTaskHandle producer = pgy_spawn(producer_task, &channel);

    ok1 = pgy_channel_recv_Int(&channel, &first);
    ok2 = pgy_channel_recv_Int(&channel, &second);
    closed = pgy_channel_closed_Int(&channel);
    remaining = pgy_channel_length_Int(&channel);

    pgy_await_void(producer);
    pgy_channel_destroy_Int(&channel);
    pgy_pool_shutdown();

    EXPECT(ok1);
    EXPECT(ok2);
    EXPECT(closed);
    EXPECT(remaining == 0);
    EXPECT(first == 7);
    EXPECT(second == 11);
}

static void *
cancellable_coro_task(void *arg)
{
    (void)arg;
    pgy_async_yield();
    int32_t *out = (int32_t *)malloc(sizeof(int32_t));
    if (out == NULL)
        return NULL;
    *out = pgy_task_is_cancelled() ? 9 : 0;
    return out;
}

static void *
cancel_propagation_child_task(void *arg)
{
    (void)arg;
    pgy_async_yield();
    int32_t *out = (int32_t *)malloc(sizeof(int32_t));
    if (out == NULL)
        return NULL;
    *out = pgy_task_is_cancelled() ? 17 : 0;
    return out;
}

static void *
cancel_propagation_parent_task(void *arg)
{
    (void)arg;
    PgyTaskHandle child = pgy_async_spawn(cancel_propagation_child_task, NULL);
    pgy_async_yield();

    int32_t *out = (int32_t *)malloc(sizeof(int32_t));
    if (out == NULL)
        return NULL;
    *out = pgy_await_take(child, int32_t);
    return out;
}

static void
test_async_task_cancel_is_visible_inside_task(void)
{
    TEST("async task observes cancellation cooperatively");

    PgyTaskHandle task = pgy_async_spawn(cancellable_coro_task, NULL);
    EXPECT(task.task != NULL);

    /* Let the coroutine start and yield once so cancellation is observed
     * cooperatively on resume rather than being skipped before entry. */
    EXPECT(pgy_async_progress_one());
    EXPECT(pgy_task_cancel(task));

    EXPECT(pgy_await_take(task, int32_t) == 9);
}

static void
test_async_task_cancel_propagates_to_spawned_children(void)
{
    TEST("async task cancellation propagates to spawned children");

    PgyTaskHandle parent = pgy_async_spawn(cancel_propagation_parent_task, NULL);
    EXPECT(parent.task != NULL);

    /* Start the parent so it spawns a child and yields. */
    EXPECT(pgy_async_progress_one());
    EXPECT(pgy_task_cancel(parent));

    EXPECT(pgy_await_take(parent, int32_t) == 17);
}

typedef struct {
    int32_t target;
} StressDamageEffect;

typedef struct {
    struct {
        StressDamageEffect items[8];
        bool active[8];
        uint8_t count;
        uint8_t cap;
    } damage;
    bool __layer_active_damage;
    PGY_ZONE_LOCK_FIELD
    PGY_ZONE_GENERATION_FIELD
} StressZone;

static inline void
StressZone_init(StressZone *self)
{
    memset(self, 0, sizeof(*self));
    PGY_ZONE_LOCK_INIT(self);
    PGY_EFFECT_POOL_INIT(self->damage);
}

static inline void
StressZone_destroy(StressZone *self)
{
    PGY_ZONE_LOCK_DESTROY(self);
}

static inline void
StressZone_sync(StressZone *self, int tick)
{
    StressDamageEffect effect = { .target = tick };

    PGY_ZONE_WRLOCK(self);
    PGY_ZONE_GENERATION_INC(self);
    PGY_EFFECT_POOL_INIT(self->damage);
    if ((tick % 3) != 0)
        PGY_EFFECT_POOL_APPLY(self->damage, effect);
    self->__layer_active_damage = PGY_EFFECT_POOL_ACTIVE_COUNT(self->damage) > 0;
    PGY_ZONE_UNLOCK(self);
}

static inline bool
StressZone_has_layer_damage(StressZone *self, uint32_t expected_gen)
{
    bool result;
    PGY_ZONE_RDLOCK(self);
    PGY_ZONE_GENERATION_WARN_IF_STALE(self, expected_gen, "StressZone.damage");
    result = self->__layer_active_damage;
    PGY_ZONE_UNLOCK(self);
    return result;
}

static inline bool
StressZone_has_layer_damage_current(StressZone *self)
{
    bool result;
    PGY_ZONE_RDLOCK(self);
    result = self->__layer_active_damage;
    PGY_ZONE_UNLOCK(self);
    return result;
}

typedef struct {
    StressZone *zone;
    int loops;
} StressZoneTaskArgs;

static void *
stress_zone_writer(void *arg)
{
    StressZoneTaskArgs *args = (StressZoneTaskArgs *)arg;
    for (int i = 0; i < args->loops; i++) {
        StressZone_sync(args->zone, i);
        if ((i % 32) == 0)
            sleep_ms(1);
    }
    return NULL;
}

static void *
stress_zone_reader(void *arg)
{
    StressZoneTaskArgs *args = (StressZoneTaskArgs *)arg;
    for (int i = 0; i < args->loops; i++) {
        bool active;

        active = StressZone_has_layer_damage_current(args->zone);
        atomic_fetch_add(&zone_reads, 1);
        if (active)
            atomic_fetch_add(&zone_true_reads, 1);
        else
            atomic_fetch_add(&zone_false_reads, 1);

        if ((i % 16) == 0)
            sleep_ms(1);
    }
    return NULL;
}

static void
test_zone_has_layer_stress_across_spawned_workers(void)
{
    TEST("zone HasLayer stress works across spawned workers");

    StressZone zone;
    StressZoneTaskArgs args = { .zone = &zone, .loops = 256 };
    PgyTaskHandle writer;
    PgyTaskHandle reader1;
    PgyTaskHandle reader2;

    atomic_store(&zone_reads, 0);
    atomic_store(&zone_true_reads, 0);
    atomic_store(&zone_false_reads, 0);

    StressZone_init(&zone);
    pgy_pool_init(3);

    writer = pgy_spawn(stress_zone_writer, &args);
    reader1 = pgy_spawn(stress_zone_reader, &args);
    reader2 = pgy_spawn(stress_zone_reader, &args);

    pgy_await_void(writer);
    pgy_await_void(reader1);
    pgy_await_void(reader2);

    pgy_pool_shutdown();
    StressZone_destroy(&zone);

    EXPECT(atomic_load(&zone_reads) > 0);
    EXPECT(atomic_load(&zone_true_reads) > 0);
    EXPECT(atomic_load(&zone_false_reads) > 0);
}

int
main(void)
{
    printf("=== Concurrency Runtime Test ===\n");

    test_spawn_runs_concurrently();
    test_channel_transfers_between_threads();
    test_async_task_cancel_is_visible_inside_task();
    test_async_task_cancel_propagates_to_spawned_children();
    test_zone_has_layer_stress_across_spawned_workers();

    printf("Tests run: %d\n", tests_run);
    if (tests_failed != 0) {
        printf("Failures: %d\n", tests_failed);
        return 1;
    }

    printf("All concurrency tests passed.\n");
    return 0;
}
