int32_t ClaimQubit(void)
{
    if (!pgy_qubit_rng_init_rt) {
        srand((unsigned)time(NULL));
        pgy_qubit_rng_init_rt = true;
    }
    if (pgy_qubit_next_rt >= PGY_QUBIT_RT_MAX)
        return -1;

    int32_t id = pgy_qubit_next_rt++;
    pgy_qubits_rt[id].state    = 2;
    pgy_qubits_rt[id].pool_id  = -1;
    pgy_qubits_rt[id].measured = false;
    return id;
}

int32_t Measure(int32_t id)
{
    if (id < 0 || id >= PGY_QUBIT_RT_MAX)
        return -1;

    PgyQubit_RT *q = &pgy_qubits_rt[id];
    if (q->measured)
        return q->state;

    if (q->state == 2)
        q->state = rand() % 2;
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

    return q->state;
}

void Entangle(int32_t a, int32_t b)
{
    if (a < 0 || a >= PGY_QUBIT_RT_MAX || b < 0 || b >= PGY_QUBIT_RT_MAX)
        return;

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
}

void H(int32_t id)
{
    if (id < 0 || id >= PGY_QUBIT_RT_MAX) return;
    pgy_qubits_rt[id].state    = 2;
    pgy_qubits_rt[id].measured = false;
}

bool IntoClassical(int32_t id)
{
    if (id < 0 || id >= PGY_QUBIT_RT_MAX) return false;
    return pgy_qubits_rt[id].state == 1;
}

int32_t QubitState(int32_t id)
{
    if (id < 0 || id >= PGY_QUBIT_RT_MAX)
        return -1;
    return pgy_qubits_rt[id].state;
}

bool IsCollapsed(int32_t id)
{
    if (id < 0 || id >= PGY_QUBIT_RT_MAX)
        return true;
    return pgy_qubits_rt[id].measured;
}

void ReleaseQubit(int32_t id)
{
    if (id < 0 || id >= PGY_QUBIT_RT_MAX)
        return;
    if (pgy_qubits_rt[id].pool_id >= 0)
        rt_pool_remove(pgy_qubits_rt[id].pool_id, id);
    pgy_qubits_rt[id].state    = -1;
    pgy_qubits_rt[id].pool_id  = -1;
    pgy_qubits_rt[id].measured = true;
}

/* =================================================================
 * Thread pool runtime (real pthread-based concurrency)
 *
 * These are non-inline exports of the pgy_parallel.h functions.
 * The LLVM backend links against these symbols.
 * ================================================================= */

#include "runtime/pgy_parallel.h"

/* Force non-inline symbol exports for the linker */
void pgy_pool_init_export(size_t n)    { pgy_pool_init(n); }
void pgy_pool_shutdown_export(void)    { pgy_pool_shutdown(); }

PgyTaskHandle pgy_spawn_export(void *(*fn)(void *), void *arg)
{
    return pgy_spawn(fn, arg);
}

PgyTaskHandle pgy_async_spawn_export(void *(*fn)(void *), void *arg)
{
    return pgy_async_spawn(fn, arg);
}

void pgy_async_detach_export(PgyTaskHandle h)
{
    pgy_async_detach(h);
}

void *pgy_await_export(PgyTaskHandle h)
{
    return pgy_await(h);
}

PgyTaskHandle pgy_spawn_blocking_export(void *(*fn)(void *), void *arg)
{
    return pgy_spawn_blocking(fn, arg);
}

bool pgy_task_cancel_export(PgyTaskHandle h)
{
    return pgy_task_cancel(h);
}

bool pgy_task_is_cancelled_export(void)
{
    return pgy_task_is_cancelled();
}

