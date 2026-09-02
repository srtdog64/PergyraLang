#include "../src/runtime/pgy_runtime.h"
#include "../src/runtime/pgy_lane_scheduler.h"

#include <stdio.h>
#include <string.h>

#if defined(PGY_CONTEXT_LLVM_RUNTIME)
extern void pgy_pool_init_export(size_t worker_count);
extern void pgy_pool_shutdown_export(void);
extern void *pgy_await_export(PgyTaskHandle handle);
extern PgyTaskHandle pgy_lane_spawn_dispatch_export(
    int32_t lane, void *(*fn)(void *), void *arg);
#define runtime_test_pool_init(n) pgy_pool_init_export((n))
#define runtime_test_pool_shutdown() pgy_pool_shutdown_export()
#define runtime_test_spawn(lane, fn, arg) \
    pgy_lane_spawn_dispatch_export((int32_t)(lane), (fn), (arg))
#define runtime_test_await(handle) pgy_await_export((handle))
#elif defined(PGY_CONTEXT_CEXT_RUNTIME)
#define runtime_test_pool_init(n) pgy_pool_init((n))
#define runtime_test_pool_shutdown() pgy_pool_shutdown()
#define runtime_test_spawn(lane, fn, arg) \
    pgy_lane_spawn_dispatch((lane), (fn), (arg))
#define runtime_test_await(handle) pgy_lane_await((handle))
#else
#define runtime_test_pool_init(n) pgy_pool_init((n))
#define runtime_test_pool_shutdown() pgy_pool_shutdown()
#define runtime_test_spawn(lane, fn, arg) \
    pgy_lane_spawn_dispatch((lane), (fn), (arg))
#define runtime_test_await(handle) pgy_lane_await((handle))
#endif

typedef struct {
    uint64_t expected_instance;
    uint32_t expected_caps;
    PgyBudgetState *expected_budget;
    int before_ok;
    int after_ok;
} RuntimeContextObservation;

static int
runtime_context_matches(RuntimeContextObservation *observation)
{
    return pgy_runtime_context_instance_id() == observation->expected_instance
        && pgy_cap_granted_export() == observation->expected_caps
        && pgy_budget_state_slot() == observation->expected_budget;
}

static void *
observe_runtime_context(void *raw)
{
    RuntimeContextObservation *observation =
        (RuntimeContextObservation *)raw;
    observation->before_ok = runtime_context_matches(observation);
    pgy_budget_charge_export(PGY_BUDGET_ALLOC_BYTES, 1, "context-child");
    observation->after_ok = runtime_context_matches(observation);
    return NULL;
}

static void *
require_denied_child_io_write(void *raw)
{
    (void)raw;
    pgy_cap_require_export(PGY_CAP_IO_WRITE, "context-child-io-write");
    return NULL;
}

static void *
observe_runtime_context_across_yield(void *raw)
{
    RuntimeContextObservation *observation =
        (RuntimeContextObservation *)raw;
    observation->before_ok = runtime_context_matches(observation);
#if defined(PGY_CONTEXT_LLVM_RUNTIME) || defined(PGY_CONTEXT_CEXT_RUNTIME)
    {
        RuntimeContextObservation nested = *observation;
        PgyTaskHandle child;

        nested.before_ok = 0;
        nested.after_ok = 0;
        child = runtime_test_spawn(
            PGY_LANE_LOCAL_ASYNC, observe_runtime_context, &nested);
        if (child.task == NULL)
            return NULL;
        (void)runtime_test_await(child);
        observation->after_ok = runtime_context_matches(observation)
            && nested.before_ok && nested.after_ok;
    }
#else
    pgy_async_yield();
    pgy_budget_charge_export(PGY_BUDGET_ALLOC_BYTES, 1,
                             "context-coroutine-child");
    observation->after_ok = runtime_context_matches(observation);
#endif
    return NULL;
}

static void *
observe_nested_runtime_context(void *raw)
{
    RuntimeContextObservation *observation =
        (RuntimeContextObservation *)raw;
    RuntimeContextObservation nested = *observation;
    PgyTaskHandle child;

    observation->before_ok = runtime_context_matches(observation);
    nested.before_ok = 0;
    nested.after_ok = 0;
    child = runtime_test_spawn(
        PGY_LANE_WORKER_POOL, observe_runtime_context, &nested);
    if (child.task == NULL)
        return NULL;
    (void)runtime_test_await(child);
    observation->after_ok = runtime_context_matches(observation)
        && nested.before_ok && nested.after_ok;
    return NULL;
}

static int
run_observation(PgyExecutionLane lane,
                void *(*fn)(void *),
                RuntimeContextObservation *observation)
{
    PgyTaskHandle handle;

    observation->before_ok = 0;
    observation->after_ok = 0;
    handle = runtime_test_spawn(lane, fn, observation);
    if (handle.task == NULL)
        return 0;
    (void)runtime_test_await(handle);
    if (!observation->before_ok || !observation->after_ok) {
        fprintf(stderr,
                "runtime context observation failed for lane=%d before=%d after=%d\n",
                (int)lane, observation->before_ok, observation->after_ok);
        return 0;
    }
    return 1;
}

int
main(int argc, char **argv)
{
    PgyRuntimeContext first;
    PgyRuntimeContext second;
    RuntimeContextObservation observation;
    uint64_t expected_parent_used = 38;

    if (argc == 2 && strcmp(argv[1], "deny-child-io-write") == 0) {
        PgyTaskHandle denied_child;

        pgy_runtime_context_init(&first, 303);
        if (!pgy_runtime_context_bind(&first))
            return 6;
        pgy_cap_set_manifest_export(PGY_CAP_IO_READ);
        runtime_test_pool_init(1);
        denied_child = runtime_test_spawn(
            PGY_LANE_WORKER_POOL, require_denied_child_io_write, NULL);
        if (denied_child.task == NULL) {
            runtime_test_pool_shutdown();
            return 7;
        }
        (void)runtime_test_await(denied_child);
        runtime_test_pool_shutdown();
        fputs("child io_write unexpectedly ran\n", stderr);
        return 0;
    }

    pgy_runtime_context_init(&first, 101);
    pgy_runtime_context_init(&second, 202);
    if (!pgy_runtime_context_bind(&first)
        || pgy_runtime_context_instance_id() != 101)
        return 1;

    pgy_cap_set_manifest_export(PGY_CAP_IO_READ);
    pgy_budget_set_limit_export(PGY_BUDGET_ALLOC_BYTES, 64);
    pgy_budget_charge_export(PGY_BUDGET_ALLOC_BYTES, 32, "context-first");

    observation.expected_instance = 101;
    observation.expected_caps = PGY_CAP_IO_READ;
    observation.expected_budget = pgy_budget_state_slot();

    runtime_test_pool_init(1);
    if (!run_observation(PGY_LANE_INLINE, observe_runtime_context,
                         &observation)
        || !run_observation(PGY_LANE_PINNED_ZONE, observe_runtime_context,
                            &observation)
        || !run_observation(PGY_LANE_WORKER_POOL, observe_runtime_context,
                            &observation)
        || !run_observation(PGY_LANE_BLOCKING_POOL, observe_runtime_context,
                            &observation)
        || !run_observation(PGY_LANE_LOCAL_ASYNC,
                            observe_runtime_context_across_yield,
                            &observation)
        || !run_observation(PGY_LANE_WORKER_POOL,
                            observe_nested_runtime_context,
                            &observation)
#ifdef PGY_CONTEXT_EXPECT_MOVABLE
        || !run_observation(PGY_LANE_MOVABLE_SCHEDULER,
                            observe_runtime_context, &observation)
#endif
        || pgy_runtime_context_instance_id() != 101
        || pgy_cap_granted_export() != PGY_CAP_IO_READ
        || pgy_budget_state_slot() != observation.expected_budget
        || pgy_budget_used_export(PGY_BUDGET_ALLOC_BYTES) !=
#ifdef PGY_CONTEXT_EXPECT_MOVABLE
            39
#else
            38
#endif
        ) {
        fprintf(stderr,
                "parent context restore failed instance=%llu caps=%u budget_same=%d used=%llu\n",
                (unsigned long long)pgy_runtime_context_instance_id(),
                (unsigned)pgy_cap_granted_export(),
                pgy_budget_state_slot() == observation.expected_budget,
                (unsigned long long)pgy_budget_used_export(
                    PGY_BUDGET_ALLOC_BYTES));
        runtime_test_pool_shutdown();
        return 2;
    }
#ifdef PGY_CONTEXT_EXPECT_MOVABLE
    expected_parent_used = 39;
#endif
    runtime_test_pool_shutdown();

    if (!pgy_runtime_context_bind(&second)
        || pgy_runtime_context_instance_id() != 202
        || pgy_cap_granted_export() != PGY_CAP_ALL
        || pgy_budget_used_export(PGY_BUDGET_ALLOC_BYTES) != 0)
        return 3;

    pgy_cap_set_manifest_export(PGY_CAP_NETWORK);
    pgy_budget_charge_export(PGY_BUDGET_ALLOC_BYTES, 16, "context-second");
    if (!pgy_runtime_context_bind(&first)
        || pgy_runtime_context_instance_id() != 101
        || pgy_cap_granted_export() != PGY_CAP_IO_READ
        || pgy_budget_used_export(PGY_BUDGET_ALLOC_BYTES) !=
            expected_parent_used)
        return 4;

    pgy_runtime_context_unbind();
    if (pgy_runtime_context_instance_id() != 0)
        return 5;
    puts("[runtime-context] PASS bound-instance isolation and spawn context propagation");
    return 0;
}
