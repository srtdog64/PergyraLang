#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY_BIN_WAS_EXPLICIT=0
if [[ -n "${PGY_BIN:-}" ]]; then
    PGY="$PGY_BIN"
    PGY_BIN_WAS_EXPLICIT=1
else
    PGY="$ROOT_DIR/bin/pgy"
fi
if [[ "$PGY" != *.exe && -x "${PGY}.exe" ]]; then
    PGY="${PGY}.exe"
fi

MODEL_DOC="$ROOT_DIR/docs/113_memory_concurrency_model.md"
if [[ ! -f "$MODEL_DOC" ]]; then
    echo "[memory-concurrency] missing model doc: $MODEL_DOC" >&2
    exit 1
fi

for required in \
    "Memory And Concurrency Model Beta Contract" \
    "beta-freeze-source-of-truth" \
    "core execution primitive" \
    "spawn Worker(args...)" \
    "joins before control continues" \
    "Shared " \
    "task-boundary conflicts are rejected" \
    "Slot read/write and write/write overlaps across sibling" \
    "boundary-witness \`op_guard\` refinement" \
    "src/semantic/boundary_witness.{h,c}" \
    "PgyBoundaryWitnessSummary" \
    "read/write overlap is a semantic error" \
    "Undefined-Behavior Hygiene Contract" \
    "Non-atomic shared counters are forbidden across worker threads" \
    "insert/rehash invalidates concurrent readers" \
    "published as an immutable snapshot" \
    "Static local buffers/state are not thread-safe by default" \
    "\`GetFiberStats\` returns the numeric counters by value" \
    "registry-owned borrowed string" \
    "no non-atomic" \
    "Non-blocking/timeout receive is copy-only for beta" \
    "ChannelClose(Channel<T>)" \
    "ChannelDestroy(Channel<T>)" \
    "Future Await Contract" \
    "handle is consumed by the join" \
    "returned \`DispatchResult\` owns its role-id strings" \
    "Cancel(Future<T>)" \
    "Capture-bearing detached async block stability" \
    "Full weak-memory ordering vocabulary" \
    "make memory-concurrency-model-test-smoke"; do
    if ! grep -Fq "$required" "$MODEL_DOC"; then
        echo "[memory-concurrency] model doc missing: $required" >&2
        exit 1
    fi
done

for required_boundary_witness_guard in \
    "PgyBoundaryWitnessSummary" \
    "pgy_boundary_witness_guard_accepts" \
    "resource_snapshot_record_parallel_boundary_witness" \
    "boundary-witness oracle matches op_guard" \
    "parallel-rejected: read-write slot race fails op_guard" \
    "parallel-rejected: write-read slot race fails op_guard"; do
    if ! grep -Fq "$required_boundary_witness_guard" \
        "$ROOT_DIR/src/semantic/boundary_witness.h" \
        "$ROOT_DIR/src/semantic/boundary_witness.c" \
        "$ROOT_DIR/src/semantic/type_checker_flow_resources.c" \
        "$ROOT_DIR/src/semantic/type_checker_flow_parallel.c" \
        "$ROOT_DIR/src/tests/semantic/test_semantic_parallel_context.cases.h"; then
        echo "[memory-concurrency] boundary witness refinement missing: $required_boundary_witness_guard" >&2
        exit 1
    fi
done

if ! grep -Fq "static _Thread_local unsigned pgy_zone_stale_warn_count" \
    "$ROOT_DIR/src/runtime/pgy_runtime_zone_result_option_inline.h"; then
    echo "[memory-concurrency] stale zone warning counter must be thread-local" >&2
    exit 1
fi
for required_zone_lock_guard in \
    "zone lock initialization failed" \
    "zone read lock failed" \
    "zone write lock failed" \
    "zone unlock failed"; do
    if ! grep -Fq "$required_zone_lock_guard" \
        "$ROOT_DIR/src/runtime/pgy_runtime_zone_result_option_inline.h"; then
        echo "[memory-concurrency] zone lock macros must fail closed: $required_zone_lock_guard" >&2
        exit 1
    fi
done
for required_world_roster_borrow_contract in \
    "World/roster async execution is a borrowed-handle surface for beta" \
    "does not deep-copy \`RosterContext\`, \`DispatcherConfig\`" \
    "timeout from" \
    "Borrowed-handle contract"; do
    if ! grep -Fq "$required_world_roster_borrow_contract" \
        "$ROOT_DIR/docs/113_memory_concurrency_model.md" \
        "$ROOT_DIR/src/runtime/world_roster.h"; then
        echo "[memory-concurrency] world/roster async borrowed-handle contract missing: $required_world_roster_borrow_contract" >&2
        exit 1
    fi
done
for required_party_dispatch_borrow_contract in \
    "DispatchParallel borrows this graph" \
    "mutate or free the FiberMap" \
    "Dispatch is quiescent-borrowed" \
    "copied plan or a channel/result" \
    "DispatchGeneratedFiberMap(" \
    "FreeFiberMap(map)" \
    "FreeDispatchResult(DispatchResult* result)" \
    "DispatchResult-owned" \
    "party_dispatch_copy_role_id" \
    "UpdateFiberStats(map->entries[i].roleId" \
    "DispatchGeneratedFiberMap( \\"; do
    if ! grep -Fq "$required_party_dispatch_borrow_contract" \
        "$ROOT_DIR/src/runtime/party_runtime.h" \
        "$ROOT_DIR/src/runtime/party_runtime_dispatch.c"; then
        echo "[memory-concurrency] party dispatch borrowed graph contract missing: $required_party_dispatch_borrow_contract" >&2
        exit 1
    fi
done
if ! grep -Fq "FreeDispatchResult(result)" \
    "$ROOT_DIR/src/runtime/world_roster.c"; then
    echo "[memory-concurrency] world/roster dispatch results must use the dispatch-result owner" >&2
    exit 1
fi
if grep -A6 -F "#define DISPATCH_PARTY" "$ROOT_DIR/src/runtime/party_runtime.h" | \
    grep -Fq "GenerateFiberMap("; then
    echo "[memory-concurrency] DISPATCH_PARTY must not leak a generated FiberMap" >&2
    exit 1
fi
if ! grep -Fq "parallel task spawn failed" \
    "$ROOT_DIR/src/runtime/pgy_parallel_run.h"; then
    echo "[memory-concurrency] parallel runtime must fail closed on spawn failure" >&2
    exit 1
fi
if ! grep -Fq "PGY_RUNTIME_PANIC_CLASS_OOM" \
    "$ROOT_DIR/src/runtime/pgy_parallel.h"; then
    echo "[memory-concurrency] parallel runtime allocation failure must panic" >&2
    exit 1
fi
for required_parallel_sync_guard in \
    "pgy_task_sync_init" \
    "task mutex initialization failed" \
    "task condition initialization failed" \
    "queue mutex initialization failed" \
    "queue condition initialization failed" \
    "worker condition wait failed"; do
    if ! grep -Fq "$required_parallel_sync_guard" \
        "$ROOT_DIR/src/runtime/pgy_parallel.h" \
        "$ROOT_DIR/src/runtime/pgy_parallel_pool_lifecycle.h" \
        "$ROOT_DIR/src/runtime/pgy_parallel_blocking.h"; then
        echo "[memory-concurrency] parallel runtime missing sync init fail-closed guard: $required_parallel_sync_guard" >&2
        exit 1
    fi
done
for forbidden_task_sync_init in \
    "pthread_mutex_init(&task->mutex, NULL)" \
    "pthread_cond_init(&task->cond, NULL)"; do
    sync_init_count=$(grep -R -F "$forbidden_task_sync_init" \
        "$ROOT_DIR/src/runtime/pgy_parallel.h" \
        "$ROOT_DIR/src/runtime/pgy_parallel_blocking.h" | wc -l)
    if [[ "$sync_init_count" -ne 1 ]]; then
        echo "[memory-concurrency] task sync initialization must go through pgy_task_sync_init: $forbidden_task_sync_init" >&2
        exit 1
    fi
done
for required_await_guard in \
    "await task handle is null" \
    "await condition wait failed" \
    "Future await returned null result"; do
    if ! grep -Fq "$required_await_guard" \
        "$ROOT_DIR/src/runtime/pgy_parallel_task_ops.h"; then
        echo "[memory-concurrency] await runtime missing fail-closed guard: $required_await_guard" >&2
        exit 1
    fi
done
if grep -R -Fq "cancellation disabled because cancel node allocation failed" \
    "$ROOT_DIR/src/runtime/pgy_parallel.h" \
    "$ROOT_DIR/src/runtime/pgy_parallel_blocking.h" \
    "$ROOT_DIR/src/runtime/pgy_parallel_task_ops.h"; then
    echo "[memory-concurrency] cancellation allocation failure must not silently disable cancellation" >&2
    exit 1
fi
if ! grep -Fq "detach task handle is null" \
    "$ROOT_DIR/src/runtime/pgy_parallel_coroutine.h"; then
    echo "[memory-concurrency] async detach must panic on null task handle" >&2
    exit 1
fi
if ! grep -Fq "detached async requires coroutine runtime support" \
    "$ROOT_DIR/src/runtime/pgy_parallel_coroutine.h"; then
    echo "[memory-concurrency] async detach fallback must fail closed when coroutine runtime is unavailable" >&2
    exit 1
fi
for channel_destroy_contract_file in \
    "$ROOT_DIR/src/runtime/pgy_runtime_channel_inline.h" \
    "$ROOT_DIR/src/runtime/pgy_runtime_channel_string_inline.h" \
    "$ROOT_DIR/src/runtime/pgy_runtime_channel_spsc_inline.h" \
    "$ROOT_DIR/src/runtime/pgy_runtime_lib_channel_int_exports.h" \
    "$ROOT_DIR/src/runtime/pgy_runtime_lib_channel_string_exports.h"; do
    if ! grep -Fq "Destroy is quiescent-only" "$channel_destroy_contract_file"; then
        echo "[memory-concurrency] channel runtime missing quiescent destroy contract: $channel_destroy_contract_file" >&2
        exit 1
    fi
done
for channel_init_contract_file in \
    "$ROOT_DIR/src/runtime/pgy_runtime_channel_inline.h" \
    "$ROOT_DIR/src/runtime/pgy_runtime_channel_string_inline.h"; do
    for required_channel_init_guard in \
        "mutex initialization failed" \
        "not-full condition initialization failed" \
        "not-empty condition initialization failed"; do
        if ! grep -Fq "$required_channel_init_guard" "$channel_init_contract_file"; then
            echo "[memory-concurrency] channel runtime missing init fail-closed guard: $channel_init_contract_file missing $required_channel_init_guard" >&2
            exit 1
        fi
    done
done
for channel_wait_contract_file in \
    "$ROOT_DIR/src/runtime/pgy_runtime_channel_inline.h" \
    "$ROOT_DIR/src/runtime/pgy_runtime_channel_string_inline.h"; do
    for required_channel_wait_guard in \
        "not-full condition wait failed" \
        "not-full condition timed wait failed" \
        "not-empty condition wait failed" \
        "not-empty condition timed wait failed"; do
        if ! grep -Fq "$required_channel_wait_guard" "$channel_wait_contract_file"; then
            echo "[memory-concurrency] channel runtime missing wait fail-closed guard: $channel_wait_contract_file missing $required_channel_wait_guard" >&2
            exit 1
        fi
    done
done
# Lifecycle-violation promotion (docs/179 3a; V1 bug class #8-R): an op on
# a NULL/uninitialized channel panics fail-closed. Warn-and-continue here
# once let a pre-fix binary retry a lost send forever and log 4.7GB. The
# positive pin requires the panic guard; the negative pin keeps the old
# warn-and-continue shape from coming back in any channel twin.
for channel_lifecycle_contract_file in \
    "$ROOT_DIR/src/runtime/pgy_runtime_channel_inline.h" \
    "$ROOT_DIR/src/runtime/pgy_runtime_channel_string_inline.h" \
    "$ROOT_DIR/src/runtime/pgy_runtime_channel_spsc_inline.h"; do
    if ! grep -Fq "pgy_channel_require_operable" \
        "$channel_lifecycle_contract_file"; then
        echo "[memory-concurrency] channel runtime missing lifecycle-violation panic guard: $channel_lifecycle_contract_file" >&2
        exit 1
    fi
    if grep -Fq "channel is not initialized" \
        "$channel_lifecycle_contract_file"; then
        echo "[memory-concurrency] channel runtime reintroduced warn-and-continue on an uninitialized channel: $channel_lifecycle_contract_file" >&2
        exit 1
    fi
done
# The guard body moved to its own lifecycle owner header (parallel
# capture SoT landing); the panic-class pin follows the owner.
if ! grep -Fq "PGY_RUNTIME_PANIC_CLASS_INVALID_LIFECYCLE_STATE" \
    "$ROOT_DIR/src/runtime/pgy_runtime_channel_lifecycle_inline.h"; then
    echo "[memory-concurrency] channel lifecycle violation must panic with the invalid-lifecycle-state class" >&2
    exit 1
fi
if ! grep -Fq "PGY_CHANNEL_DEFINE(Int, int32_t, extern)" \
    "$ROOT_DIR/src/runtime/pgy_runtime_lib_channel_int_exports.h"; then
    echo "[memory-concurrency] Int channel export twin must reuse the shared fail-closed body" >&2
    exit 1
fi
if ! grep -Fq "#define PGY_CH_STR_STORAGE extern" \
    "$ROOT_DIR/src/runtime/pgy_runtime_lib_channel_string_exports.h"; then
    echo "[memory-concurrency] String channel export twin must reuse the shared fail-closed body" >&2
    exit 1
fi
if ! grep -Fq "Destroy is quiescent-only" \
    "$ROOT_DIR/src/runtime/async/concurrent_queue.h"; then
    echo "[memory-concurrency] concurrent queue destroy contract must stay explicit" >&2
    exit 1
fi
if ! grep -Fq "bool ConcurrentQueuePushBatch" \
    "$ROOT_DIR/src/runtime/async/concurrent_queue.h"; then
    echo "[memory-concurrency] concurrent queue batch push must report failure" >&2
    exit 1
fi
if ! grep -Fq "PushBatch is all-or-nothing" \
    "$ROOT_DIR/src/runtime/async/concurrent_queue.h"; then
    echo "[memory-concurrency] concurrent queue batch push contract must stay all-or-nothing" >&2
    exit 1
fi
if grep -Fq "void ConcurrentQueuePushBatch" \
    "$ROOT_DIR/src/runtime/async/concurrent_queue.h"; then
    echo "[memory-concurrency] concurrent queue batch push must not regress to void" >&2
    exit 1
fi
if ! grep -Fq "queue_size_increment_by(queue, count)" \
    "$ROOT_DIR/src/runtime/async/concurrent_queue.c"; then
    echo "[memory-concurrency] concurrent queue batch push must publish a prebuilt batch in one size update" >&2
    exit 1
fi
if ! grep -Fq "pthread_mutex_init(&state->mutex, NULL) != 0" \
    "$ROOT_DIR/src/runtime/async/concurrent_queue.c"; then
    echo "[memory-concurrency] concurrent queue must fail closed when mutex initialization fails" >&2
    exit 1
fi
if grep -Fq "ConcurrentQueuePush(queue, items[i])" \
    "$ROOT_DIR/src/runtime/async/concurrent_queue.c"; then
    echo "[memory-concurrency] concurrent queue batch push must not partially enqueue via single-item push loop" >&2
    exit 1
fi
for required_queue_sentinel_contract in \
    "NULL payloads are rejected" \
    "NULL payload is reserved as the empty sentinel"; do
    if ! grep -Fq "$required_queue_sentinel_contract" \
        "$ROOT_DIR/src/runtime/async/concurrent_queue.h" \
        "$ROOT_DIR/src/runtime/async/concurrent_queue.c"; then
        echo "[memory-concurrency] concurrent queue must reject NULL payload ambiguity: $required_queue_sentinel_contract" >&2
        exit 1
    fi
done
for forbidden_queue_push in \
    "ConcurrentQueuePush(worker->localRunQueue, fiber);" \
    "ConcurrentQueuePush(scheduler->globalRunQueue, fiber);" \
    "ConcurrentQueuePush(thief->localRunQueue, stolen);"; do
    if grep -R -Fq "$forbidden_queue_push" \
        "$ROOT_DIR/src/runtime/async"; then
        echo "[memory-concurrency] async scheduler must not ignore queue push failure: $forbidden_queue_push" >&2
        exit 1
    fi
done
for required_scheduler_guard in \
    "scheduler failed to requeue ready fiber" \
    "scheduler failed to unblock fiber" \
    "scheduler failed to enqueue stolen fiber"; do
    if ! grep -R -Fq "$required_scheduler_guard" \
        "$ROOT_DIR/src/runtime/async"; then
        echo "[memory-concurrency] scheduler missing fail-closed guard: $required_scheduler_guard" >&2
        exit 1
    fi
done
if ! grep -Fq "atomic_compare_exchange_weak_explicit" \
        "$ROOT_DIR/src/runtime/async/concurrent_queue.c"; then
    echo "[memory-concurrency] concurrent queue size counter must use atomic RMW saturation" >&2
    exit 1
fi
for required_party_scheduler_registry_guard in \
    "g_schedulerRegistryMutex" \
    "pthread_mutex_lock(&g_schedulerRegistryMutex)" \
    "pthread_mutex_unlock(&g_schedulerRegistryMutex)" \
    "g_schedulerByTag[tag]"; do
    if ! grep -Fq "$required_party_scheduler_registry_guard" \
        "$ROOT_DIR/src/runtime/party_runtime_scheduler.c"; then
        echo "[memory-concurrency] party scheduler registry must stay mutex-owned: $required_party_scheduler_registry_guard" >&2
        exit 1
    fi
done
for required_scheduler_stat_guard in \
    "scheduler_stat_decrement_nonzero(&scheduler->totalFibers)" \
    "scheduler_stat_decrement_nonzero(&scheduler->activeFibers)"; do
    if ! grep -Fq "$required_scheduler_stat_guard" \
        "$ROOT_DIR/src/runtime/async/scheduler.c"; then
        echo "[memory-concurrency] scheduler must decrement completed fiber counters without unsigned underflow: $required_scheduler_stat_guard" >&2
        exit 1
    fi
done
if grep -Fq "atomic_fetch_add(&scheduler->totalFibers, -1)" \
    "$ROOT_DIR/src/runtime/async/scheduler.c"; then
    echo "[memory-concurrency] scheduler stats must not use unsigned -1 fetch-add" >&2
    exit 1
fi
for required_scheduler_surface_impl in \
    "void SchedulerRegisterIoEvent" \
    "void SchedulerUnregisterIoEvent" \
    "void SchedulerScheduleTimer" \
    "scheduler timer support is not implemented" \
    "void SchedulerSetDeterministicMode" \
    "void SchedulerGetStats" \
    "stats->totalFibersCreated = (UINT64_MAX - completed < active)"; do
    if ! grep -Fq "$required_scheduler_surface_impl" \
        "$ROOT_DIR/src/runtime/async/scheduler.c"; then
        echo "[memory-concurrency] scheduler public async surface must be implemented or fail closed: $required_scheduler_surface_impl" >&2
        exit 1
    fi
done
for required_scheduler_init_guard in \
    "park mutex initialization failed" \
    "park condition initialization failed" \
    "park condition wait failed"; do
    if ! grep -Fq "$required_scheduler_init_guard" \
        "$ROOT_DIR/src/runtime/async/scheduler.c"; then
        echo "[memory-concurrency] scheduler must fail closed on park primitive failure: $required_scheduler_init_guard" >&2
        exit 1
    fi
done
for required_async_scope_pattern_guard in \
    "AsyncScopeSpawn(scope, ParallelForWorker, &state) == NULL" \
    "AsyncScopeSpawn(raceScope, RaceWorker, &workerArgs[i]) == NULL" \
    "workerArgs[i].index = i" \
    "AsyncScopeCancel(raceScope)"; do
    if ! grep -Fq "$required_async_scope_pattern_guard" \
        "$ROOT_DIR/src/runtime/async/async_scope_patterns.c"; then
        echo "[memory-concurrency] async scope pattern missing spawn/index guard: $required_async_scope_pattern_guard" >&2
        exit 1
    fi
done
if grep -Fq "FiberGetCurrent()" \
        "$ROOT_DIR/src/runtime/async/async_scope_patterns.c"; then
    echo "[memory-concurrency] async scope race must not infer task index from mutable fiber-list position" >&2
    exit 1
fi
for required_async_scope_lifecycle_guard in \
    "cancel-before-start must not hang" \
    "Do not hold disposeMutex while waiting" \
    "Holding the lifecycle" \
    "fiber-list mutex initialization failed" \
    "dispose mutex initialization failed" \
    "if (!AsyncScopeIsCancelled(scope) && routine != NULL)" \
    "AsyncScopeRemoveFiber(scope, currentFiber)" \
    "pthread_mutex_lock(&scope->disposeMutex)" \
    "pthread_mutex_unlock(&scope->disposeMutex)" \
    "pthread_mutex_lock(&scope->fiberListMutex)" \
    "hasError = scope->hasError" \
    "firstError = scope->firstError"; do
    if ! grep -Fq "$required_async_scope_lifecycle_guard" \
        "$ROOT_DIR/src/runtime/async/async_scope.c"; then
        echo "[memory-concurrency] async scope lifecycle/error state must stay lock-owned: $required_async_scope_lifecycle_guard" >&2
        exit 1
    fi
done
for required_async_scope_map_reduce_guard in \
    "AsyncScopeMapReduce(AsyncScope* scope" \
    "AsyncScopeParallelFor(scope, tasks, inputCount)" \
    "acc = reducer(acc, mapped[i])"; do
    if ! grep -Fq "$required_async_scope_map_reduce_guard" \
        "$ROOT_DIR/src/runtime/async/async_scope_patterns.c"; then
        echo "[memory-concurrency] async scope map-reduce must keep parallel map plus sequential reduce contract: $required_async_scope_map_reduce_guard" >&2
        exit 1
    fi
done
for required_codegen_guard in \
    "if (_ph_%zu.task == NULL) PGY_RUNTIME_PANIC" \
    "if (_ah_%u.task == NULL) PGY_RUNTIME_PANIC" \
    "codegen_worker_boundary_storage_kind_from_type_name(type_name, false)" \
    "codegen_worker_boundary_storage_kind_from_type_name(type_name, true)" \
    "async block spawn failed"; do
    if ! grep -Fq "$required_codegen_guard" \
        "$ROOT_DIR/src/codegen/transpiler_async_parallel_emit.c"; then
        echo "[memory-concurrency] C backend spawn emission missing fail-closed guard: $required_codegen_guard" >&2
        exit 1
    fi
done
for required_c_spawn_expr_guard in \
    "spawn task creation failed" \
    "_pgy_spawn_h.task == NULL" \
    "free(_pgy_args); PGY_RUNTIME_PANIC" \
    "transpiler_spawn_reject_worker_storage" \
    "codegen_worker_boundary_storage_kind_from_type_name(type_name, true)" \
    "cannot transport %s<T> storage across a worker boundary"; do
    if ! grep -Fq "$required_c_spawn_expr_guard" \
        "$ROOT_DIR/src/codegen/transpiler_spawn_channel_emit.c"; then
        echo "[memory-concurrency] C named-spawn expression missing fail-closed guard: $required_c_spawn_expr_guard" >&2
        exit 1
    fi
done
for required_llvm_guard in \
    "llvm_emit_task_handle_nonnull_guard" \
    "LLVM parallel task spawn failed" \
    "LLVM async block spawn failed" \
    "codegen_worker_boundary_storage_kind_from_constructor_name" \
    "pgy_runtime_panic_internal_invariant_export"; do
    if ! grep -Fq "$required_llvm_guard" \
        "$ROOT_DIR/src/codegen/llvm_stmt_parallel_async.c"; then
        echo "[memory-concurrency] LLVM spawn emission missing fail-closed guard: $required_llvm_guard" >&2
        exit 1
    fi
done
for required_llvm_spawn_expr_guard in \
    "llvm_spawn_emit_nonnull_guard" \
    "LLVM spawn argument allocation failed" \
    "LLVM spawn task creation failed" \
    "LLVMBuildExtractValue(ctx->builder, handle, 0"; do
    if ! grep -Fq "$required_llvm_spawn_expr_guard" \
        "$ROOT_DIR/src/codegen/llvm_expr_spawn_call_helpers.c"; then
        echo "[memory-concurrency] LLVM named-spawn expression missing fail-closed guard: $required_llvm_spawn_expr_guard" >&2
        exit 1
    fi
done
for required_llvm_spawn_worker_guard in \
    "llvm_spawn_reject_worker_storage_param" \
    "codegen_worker_boundary_storage_kind_from_type_name(" \
    "param_type_name, true" \
    "cannot transport %s<T> storage across a worker boundary" \
    "llvm_spawn_reject_worker_storage_arg" \
    "codegen_worker_boundary_storage_kind_from_constructor_name" \
    "cannot transport %s<T> storage '%s' across a worker boundary"; do
    if ! grep -Fq "$required_llvm_spawn_worker_guard" \
        "$ROOT_DIR/src/codegen/llvm_expr_spawn_worker_boundary.c"; then
        echo "[memory-concurrency] LLVM named-spawn worker boundary missing fail-closed guard: $required_llvm_spawn_worker_guard" >&2
        exit 1
    fi
done
if ! grep -Fq "codegen_worker_boundary_storage_kind_from_type_name" \
    "$ROOT_DIR/src/codegen/transpiler_type_mapping.c"; then
    echo "[memory-concurrency] worker-boundary storage classifier must stay in the shared codegen type mapping owner" >&2
    exit 1
fi
for required_storage_policy_owner in \
    "PGY_WORKER_BOUNDARY_STORAGE_ARRAY" \
    "PGY_WORKER_BOUNDARY_STORAGE_SLICE" \
    "PGY_WORKER_BOUNDARY_STORAGE_HASHMAP" \
    "PGY_WORKER_BOUNDARY_STORAGE_CHANNEL" \
    "PgyWorkerBoundaryStorageSpec" \
    "strcmp(constructor_name" \
    "pgy_worker_boundary_storage_kind_from_constructor_name" \
    "pgy_worker_boundary_storage_kind_from_type_name" \
    "pgy_worker_boundary_storage_kind_name"; do
    if ! grep -Fq "$required_storage_policy_owner" \
        "$ROOT_DIR/src/common/worker_boundary_storage_policy.h" \
        "$ROOT_DIR/src/common/worker_boundary_storage_policy.c"; then
        echo "[memory-concurrency] worker-boundary storage policy owner missing term: $required_storage_policy_owner" >&2
        exit 1
    fi
done
for required_codegen_storage_policy_shape in \
    "bool include_array_slice_alias" \
    "\"Array/Slice\", true, true" \
    "\"Array/Slice\", false, true"; do
    if ! grep -Fq "$required_codegen_storage_policy_shape" \
        "$ROOT_DIR/src/codegen/codegen_type_mapping.h" \
        "$ROOT_DIR/src/codegen/transpiler_type_mapping.c" \
        "$ROOT_DIR/src/codegen/llvm_expr_spawn_call_helpers.c" \
        "$ROOT_DIR/src/codegen/llvm_expr_spawn_worker_boundary.c" \
        "$ROOT_DIR/src/codegen/llvm_stmt_parallel_async.c"; then
        echo "[memory-concurrency] codegen worker-boundary storage policy must expose Array/Slice alias explicitly: $required_codegen_storage_policy_shape" >&2
        exit 1
    fi
done
for required_storage_policy_file in \
    "$ROOT_DIR/src/semantic/type_checker_helpers_resources.c" \
    "$ROOT_DIR/src/codegen/transpiler_type_mapping.c"; do
    for required_storage_policy_consumer in \
        "../common/worker_boundary_storage_policy.h" \
        "pgy_worker_boundary_storage_kind_name"; do
        if ! grep -Fq "$required_storage_policy_consumer" \
            "$required_storage_policy_file"; then
            echo "[memory-concurrency] storage classifier must consume common policy: $required_storage_policy_file missing $required_storage_policy_consumer" >&2
            exit 1
        fi
    done
done
if ! grep -Fq "pgy_worker_boundary_storage_kind_from_type_name(type_name" \
        "$ROOT_DIR/src/codegen/transpiler_type_mapping.c"; then
    echo "[memory-concurrency] codegen worker-boundary type-name classification must consume common policy" >&2
    exit 1
fi
if ! grep -Fq "pgy_worker_boundary_storage_kind_from_constructor_name(" \
        "$ROOT_DIR/src/semantic/type_checker_helpers_resources.c"; then
    echo "[memory-concurrency] semantic worker-boundary constructor classification must consume common policy" >&2
    exit 1
fi
if grep -n -E 'return "(Array|Slice|List|Queue|Set|HashMap|Channel)"' \
        "$ROOT_DIR/src/semantic/type_checker_helpers_resources.c" \
        "$ROOT_DIR/src/codegen/transpiler_type_mapping.c"; then
    echo "[memory-concurrency] worker-boundary storage display strings must stay in common policy owner" >&2
    exit 1
fi
for required_llvm_await_guard in \
    "Future await returned null result" \
    "pgy_runtime_panic_internal_invariant_export" \
    "LLVMBuildUnreachable(ctx->builder)"; do
    if ! grep -Fq "$required_llvm_await_guard" \
        "$ROOT_DIR/src/codegen/llvm_expr_await_task.c"; then
        echo "[memory-concurrency] LLVM await expression missing fail-closed guard: $required_llvm_await_guard" >&2
        exit 1
    fi
done
for required_semantic_await_consume_guard in \
    "awaited_sym->is_consumed = true" \
    "await consumes a named local Future handle"; do
    if ! grep -Fq "$required_semantic_await_consume_guard" \
        "$ROOT_DIR/src/semantic/type_checker_expr.c" \
        "$ROOT_DIR/src/tests/semantic/test_semantic_misc_a_part_b_1.cases.h" \
        "$ROOT_DIR/src/tests/semantic/test_semantic_misc_a_part_b_2.cases.h"; then
        echo "[memory-concurrency] semantic await must consume named Future handles: $required_semantic_await_consume_guard" >&2
        exit 1
    fi
done
for required_semantic_worker_boundary_guard in \
    "semantic_report_worker_storage_boundary" \
    "Spawn argument cannot transport Array" \
    "Spawn argument cannot transport Channel" \
    "CFG spawn rejects borrowed Slice boundary crossing"; do
    if ! grep -Fq "$required_semantic_worker_boundary_guard" \
        "$ROOT_DIR/src/semantic/type_checker_async_channel.c" \
        "$ROOT_DIR/src/tests/semantic/test_semantic_misc_a_part_b_1.cases.h" \
        "$ROOT_DIR/src/tests/semantic/test_semantic_misc_a_part_b_2.cases.h"; then
        echo "[memory-concurrency] semantic worker-boundary storage reject must stay source-owned: $required_semantic_worker_boundary_guard" >&2
        exit 1
    fi
done
for required_rir_await_consume_guard in \
    "RIR_OP_AWAIT_LOCAL" \
    "RIR await consumes remote Future handles" \
    "future->final_state == RIR_STATE_RELEASED" \
    "pending->final_state == RIR_STATE_RELEASED"; do
    if ! grep -Fq "$required_rir_await_consume_guard" \
        "$ROOT_DIR/src/compiler/rir.h" \
        "$ROOT_DIR/src/tests/rir/test_rir_lowering_1.cases.h" \
        "$ROOT_DIR/src/tests/rir/test_rir_lowering_2.cases.h"; then
        echo "[memory-concurrency] RIR await must expose consumed Future handles: $required_rir_await_consume_guard" >&2
        exit 1
    fi
done
for required_mir_await_layout_guard in \
    "MIR carries await Future ABI layouts from RIR ops" \
    "RIR_OP_AWAIT_LOCAL" \
    "abi_type_name, \"Future\"" \
    "abi_type_name, \"RemoteFuture\""; do
    if ! grep -Fq "$required_mir_await_layout_guard" \
        "$ROOT_DIR/src/compiler/mir_lower_population.c" \
        "$ROOT_DIR/src/tests/mir/test_mir_lowering_part_a_1.cases.h" \
        "$ROOT_DIR/src/tests/mir/test_mir_lowering_part_a_2.cases.h"; then
        echo "[memory-concurrency] MIR await must carry Future ABI layout facts: $required_mir_await_layout_guard" >&2
        exit 1
    fi
done

if [[ ! -x "$PGY" ]]; then
    if [[ "$PGY_BIN_WAS_EXPLICIT" -eq 1 ]]; then
        echo "[memory-concurrency] missing explicit compiler binary: $PGY" >&2
        exit 1
    fi
    echo "[memory-concurrency] SKIP executable probe; source model is gated"
    exit 0
fi

bash "$ROOT_DIR/tests/async_model_positioning_smoke.sh"
bash "$ROOT_DIR/tests/parallel_core_contract_smoke.sh"

BACKENDS="${PGY_MEMORY_CONCURRENCY_BACKENDS:-c llvm}"
if [[ " $BACKENDS " == *" llvm "* ]]; then
    PGY_BIN="$PGY" bash "$ROOT_DIR/tests/compare_backends.sh" \
        tests/cases/backend_compare/parallel_channel_sum \
        tests/cases/backend_compare/parallel_channel_dual \
        tests/cases/backend_compare/triple_paradigm
else
    echo "[memory-concurrency] skipping backend compare for backends=$BACKENDS"
fi

echo "[memory-concurrency] beta model ok"
