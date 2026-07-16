
/* --- Qubit operations --- */

int32_t ClaimQubit(void)
{
    pthread_mutex_lock(&pgy_qubit_rt_mutex);
    if (!pgy_qubit_rng_init_rt) {
        pthread_mutex_lock(&pgy_runtime_lib_rng_mutex);
        srand((unsigned)time(NULL));
        pthread_mutex_unlock(&pgy_runtime_lib_rng_mutex);
        pgy_qubit_rng_init_rt = true;
    }
    if (pgy_qubit_next_rt >= PGY_QUBIT_RT_MAX) {
        pthread_mutex_unlock(&pgy_qubit_rt_mutex);
        return -1;
    }

    int32_t id = pgy_qubit_next_rt++;
    pgy_qubits_rt[id].state    = 2;
    pgy_qubits_rt[id].pool_id  = -1;
    pgy_qubits_rt[id].measured = false;
    pthread_mutex_unlock(&pgy_qubit_rt_mutex);
    return id;
}

int32_t Measure(int32_t id)
{
    if (id < 0 || id >= PGY_QUBIT_RT_MAX)
        return -1;

    pthread_mutex_lock(&pgy_qubit_rt_mutex);
    PgyQubit_RT *q = &pgy_qubits_rt[id];
    if (q->measured) {
        int32_t state = q->state;
        pthread_mutex_unlock(&pgy_qubit_rt_mutex);
        return state;
    }

    if (q->state == 2) {
        pthread_mutex_lock(&pgy_runtime_lib_rng_mutex);
        q->state = rand() % 2;
        pthread_mutex_unlock(&pgy_runtime_lib_rng_mutex);
    }
    q->measured = true;

    /* Propagate collapse to entire entanglement pool */
    if (q->pool_id >= 0) {
        PgyEntanglementPool_RT *pool = &pgy_qubit_pools_rt[q->pool_id];
        for (int32_t i = 0; i < pool->count; i++) {
            int32_t mid = pool->members[i];
            if (mid != id && !pgy_qubits_rt[mid].measured) {
                pgy_qubits_rt[mid].state    = q->state;
                pgy_qubits_rt[mid].measured = true;
            }
        }
    }

    int32_t state = q->state;
    pthread_mutex_unlock(&pgy_qubit_rt_mutex);
    return state;
}

void Entangle(int32_t a, int32_t b)
{
    if (a < 0 || a >= PGY_QUBIT_RT_MAX || b < 0 || b >= PGY_QUBIT_RT_MAX)
        return;

    pthread_mutex_lock(&pgy_qubit_rt_mutex);
    int32_t pa = pgy_qubits_rt[a].pool_id;
    int32_t pb = pgy_qubits_rt[b].pool_id;

    if (pa >= 0 && pb >= 0) {
        if (pa != pb)
            rt_pool_merge(pa, pb);
    } else if (pa >= 0) {
        rt_pool_add(pa, b);
    } else if (pb >= 0) {
        rt_pool_add(pb, a);
    } else {
        int32_t new_pool = rt_alloc_pool();
        if (new_pool >= 0) {
            rt_pool_add(new_pool, a);
            rt_pool_add(new_pool, b);
        }
    }
    pthread_mutex_unlock(&pgy_qubit_rt_mutex);
}

void H(int32_t id)
{
    if (id < 0 || id >= PGY_QUBIT_RT_MAX) return;
    pthread_mutex_lock(&pgy_qubit_rt_mutex);
    pgy_qubits_rt[id].state    = 2;
    pgy_qubits_rt[id].measured = false;
    pthread_mutex_unlock(&pgy_qubit_rt_mutex);
}

bool IntoClassical(int32_t id)
{
    if (id < 0 || id >= PGY_QUBIT_RT_MAX) return false;
    pthread_mutex_lock(&pgy_qubit_rt_mutex);
    bool result = pgy_qubits_rt[id].state == 1;
    pthread_mutex_unlock(&pgy_qubit_rt_mutex);
    return result;
}

int32_t QubitState(int32_t id)
{
    if (id < 0 || id >= PGY_QUBIT_RT_MAX)
        return -1;
    pthread_mutex_lock(&pgy_qubit_rt_mutex);
    int32_t state = pgy_qubits_rt[id].state;
    pthread_mutex_unlock(&pgy_qubit_rt_mutex);
    return state;
}

bool IsCollapsed(int32_t id)
{
    if (id < 0 || id >= PGY_QUBIT_RT_MAX)
        return true;
    pthread_mutex_lock(&pgy_qubit_rt_mutex);
    bool measured = pgy_qubits_rt[id].measured;
    pthread_mutex_unlock(&pgy_qubit_rt_mutex);
    return measured;
}

void ReleaseQubit(int32_t id)
{
    if (id < 0 || id >= PGY_QUBIT_RT_MAX)
        return;
    pthread_mutex_lock(&pgy_qubit_rt_mutex);
    if (pgy_qubits_rt[id].pool_id >= 0)
        rt_pool_remove(pgy_qubits_rt[id].pool_id, id);
    pgy_qubits_rt[id].state    = -1;
    pgy_qubits_rt[id].pool_id  = -1;
    pgy_qubits_rt[id].measured = true;
    pthread_mutex_unlock(&pgy_qubit_rt_mutex);
}

/* =================================================================
 * Rc/Weak runtime exports for LLVM.
 *
 * The source-level C backend uses inline typed structs from pgy_runtime.h.
 * LLVM stores Rc<T>/Weak<T> as opaque pointer-sized handles and calls these
 * exported symbols. Layout and lifecycle match the inline implementation:
 * { strong_count: u32, weak_count: u32, alive: bool, value: T }.
 * ================================================================= */

#define PGY_RC_EXPORT_DEFINE(SuffixName, CType)                                  \
typedef struct {                                                                 \
    uint32_t strong_count;                                                        \
    uint32_t weak_count;                                                          \
    bool     alive;                                                               \
    CType    value;                                                               \
} PgyRcExportControl_##SuffixName;                                                \
                                                                                  \
void *                                                                            \
pgy_rc_new_##SuffixName(CType value)                                              \
{                                                                                 \
    PgyRcExportControl_##SuffixName *ctrl =                                       \
        (PgyRcExportControl_##SuffixName *)malloc(sizeof(*ctrl));                 \
    if (ctrl == NULL) {                                                           \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM,                            \
                          PGY_RUNTIME_PANIC_REASON_ALLOCATION_FAILED);            \
    }                                                                             \
    ctrl->strong_count = 1;                                                       \
    ctrl->weak_count = 0;                                                         \
    ctrl->alive = true;                                                           \
    ctrl->value = value;                                                          \
    return ctrl;                                                                  \
}                                                                                 \
                                                                                  \
void *                                                                            \
pgy_rc_clone_##SuffixName(void *handle)                                           \
{                                                                                 \
    PgyRcExportControl_##SuffixName *ctrl =                                       \
        (PgyRcExportControl_##SuffixName *)handle;                                \
    if (ctrl == NULL || !ctrl->alive || ctrl->strong_count == 0) {                \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,             \
                          "RcClone on invalid Rc");                              \
    }                                                                             \
    ctrl->strong_count++;                                                         \
    return ctrl;                                                                  \
}                                                                                 \
                                                                                  \
CType *                                                                           \
pgy_rc_get_##SuffixName(void *handle)                                             \
{                                                                                 \
    PgyRcExportControl_##SuffixName *ctrl =                                       \
        (PgyRcExportControl_##SuffixName *)handle;                                \
    if (ctrl == NULL || !ctrl->alive || ctrl->strong_count == 0) {                \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,             \
                          "RcGet on invalid Rc");                                \
    }                                                                             \
    return &ctrl->value;                                                          \
}                                                                                 \
                                                                                  \
void *                                                                            \
pgy_rc_downgrade_##SuffixName(void *handle)                                       \
{                                                                                 \
    PgyRcExportControl_##SuffixName *ctrl =                                       \
        (PgyRcExportControl_##SuffixName *)handle;                                \
    if (ctrl == NULL || !ctrl->alive || ctrl->strong_count == 0) {                \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,             \
                          "RcDowngrade on invalid Rc");                          \
    }                                                                             \
    ctrl->weak_count++;                                                           \
    return ctrl;                                                                  \
}                                                                                 \
                                                                                  \
void                                                                              \
pgy_rc_drop_##SuffixName(void **handle)                                           \
{                                                                                 \
    PgyRcExportControl_##SuffixName *ctrl = handle != NULL                        \
        ? (PgyRcExportControl_##SuffixName *)(*handle) : NULL;                    \
    if (ctrl == NULL || ctrl->strong_count == 0) {                                \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,             \
                          "RcDrop on invalid Rc");                               \
    }                                                                             \
    ctrl->strong_count--;                                                         \
    if (ctrl->strong_count == 0)                                                   \
        ctrl->alive = false;                                                      \
    if (!ctrl->alive && ctrl->weak_count == 0)                                    \
        free(ctrl);                                                               \
    if (handle != NULL)                                                           \
        *handle = NULL;                                                           \
}                                                                                 \
                                                                                  \
void *                                                                            \
pgy_weak_upgrade_##SuffixName(void *handle)                                       \
{                                                                                 \
    PgyRcExportControl_##SuffixName *ctrl =                                       \
        (PgyRcExportControl_##SuffixName *)handle;                                \
    if (ctrl == NULL || !ctrl->alive || ctrl->strong_count == 0) {                \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,             \
                          "WeakUpgrade on expired Weak");                        \
    }                                                                             \
    ctrl->strong_count++;                                                         \
    return ctrl;                                                                  \
}                                                                                 \
                                                                                  \
void                                                                              \
pgy_weak_drop_##SuffixName(void **handle)                                         \
{                                                                                 \
    PgyRcExportControl_##SuffixName *ctrl = handle != NULL                        \
        ? (PgyRcExportControl_##SuffixName *)(*handle) : NULL;                    \
    if (ctrl == NULL || ctrl->weak_count == 0) {                                  \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,             \
                          "WeakDrop on invalid Weak");                           \
    }                                                                             \
    ctrl->weak_count--;                                                           \
    if (!ctrl->alive && ctrl->weak_count == 0)                                    \
        free(ctrl);                                                               \
    if (handle != NULL)                                                           \
        *handle = NULL;                                                           \
}

PGY_RC_EXPORT_DEFINE(Int,    int32_t)
PGY_RC_EXPORT_DEFINE(Long,   int64_t)
PGY_RC_EXPORT_DEFINE(Float,  float)
PGY_RC_EXPORT_DEFINE(Double, double)
PGY_RC_EXPORT_DEFINE(Bool,   bool)
PGY_RC_EXPORT_DEFINE(String, char *)

#undef PGY_RC_EXPORT_DEFINE

/* =================================================================
 * Thread pool runtime (real pthread-based concurrency)
 *
 * These are non-inline exports of the pgy_parallel.h functions.
 * The LLVM backend links against these symbols.
 * ================================================================= */

#include "runtime/pgy_parallel.h"
#include "runtime/pgy_lane_scheduler.h"

/* Force non-inline symbol exports for the linker */
void pgy_pool_init_export(size_t n)    { pgy_pool_init(n); }
void pgy_pool_shutdown_export(void)    { pgy_pool_shutdown(); }

void pgy_async_detach_export(PgyTaskHandle h)
{
    pgy_lane_detach(h);
}

void *pgy_await_export(PgyTaskHandle h)
{
    return pgy_lane_await(h);
}

PgyTaskHandle pgy_lane_spawn_dispatch_export(int32_t lane,
                                             void *(*fn)(void *),
                                             void *arg)
{
    return pgy_lane_spawn_dispatch((PgyExecutionLane)lane, fn, arg);
}

bool pgy_task_cancel_export(PgyTaskHandle h)
{
    return pgy_lane_cancel(h);
}

bool pgy_task_is_cancelled_export(void)
{
    return pgy_task_is_cancelled();
}

/* Auto-chunked index fan-out (docs/186 P-B3). The LLVM join emitter calls
 * these three; the C emitter calls the same inline functions directly, so
 * chunk-count policy and split arithmetic have exactly one home. */
size_t pgy_parallel_chunk_count_export(size_t n)
{
    return pgy_parallel_chunk_count(n);
}

void *pgy_parallel_chunk_ctxs_alloc_export(size_t chunk_count)
{
    return pgy_parallel_chunk_ctxs_alloc(chunk_count);
}

PgyTaskHandle pgy_parallel_spawn_chunk_at_export(void *cctxs, size_t k,
                                                 size_t chunk_count,
                                                 void *body, void *ctxs,
                                                 size_t elem_size, size_t n)
{
    return pgy_parallel_spawn_chunk_at(cctxs, k, chunk_count,
                                       (void *(*)(void *))body, ctxs,
                                       elem_size, n);
}
