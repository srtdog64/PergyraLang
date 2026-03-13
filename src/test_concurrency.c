#define _POSIX_C_SOURCE 199309L

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
    bool ok3;

    pgy_channel_init_Int(&channel, 1);
    PgyTaskHandle producer = pgy_spawn(producer_task, &channel);

    ok1 = pgy_channel_recv_Int(&channel, &first);
    ok2 = pgy_channel_recv_Int(&channel, &second);
    ok3 = pgy_channel_recv_Int(&channel, &first);

    pgy_await_void(producer);
    pgy_channel_destroy_Int(&channel);
    pgy_pool_shutdown();

    EXPECT(ok1);
    EXPECT(ok2);
    EXPECT(!ok3);
    EXPECT(first == 7);
    EXPECT(second == 11);
}

int
main(void)
{
    printf("=== Concurrency Runtime Test ===\n");

    test_spawn_runs_concurrently();
    test_channel_transfers_between_threads();

    printf("Tests run: %d\n", tests_run);
    if (tests_failed != 0) {
        printf("Failures: %d\n", tests_failed);
        return 1;
    }

    printf("All concurrency tests passed.\n");
    return 0;
}
