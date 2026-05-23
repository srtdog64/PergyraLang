/* =================================================================
 * QubitSlot runtime - N-qubit entanglement pool model
 *
 * Matches pgy_runtime.h's pool_id / PgyEntanglementPool design.
 * Supports GHZ states via pool merge on Entangle().
 * ================================================================= */

#define PGY_QUBIT_RT_MAX 64

typedef struct {
    int32_t state;       /* 0=|0>, 1=|1>, 2=superposition, -1=released */
    int32_t pool_id;     /* entanglement pool id, -1 if none */
    bool    measured;
} PgyQubit_RT;

typedef struct {
    int32_t members[PGY_QUBIT_RT_MAX];
    int32_t count;
    bool    active;
} PgyEntanglementPool_RT;

static PgyQubit_RT              pgy_qubits_rt[PGY_QUBIT_RT_MAX];
static int32_t                  pgy_qubit_next_rt = 0;
static bool                     pgy_qubit_rng_init_rt = false;

static PgyEntanglementPool_RT   pgy_qubit_pools_rt[PGY_QUBIT_RT_MAX];
static int32_t                  pgy_qubit_pool_next_rt = 0;

/* --- Pool helpers --- */

static int32_t
rt_alloc_pool(void)
{
    if (pgy_qubit_pool_next_rt >= PGY_QUBIT_RT_MAX) return -1;
    int32_t id = pgy_qubit_pool_next_rt++;
    pgy_qubit_pools_rt[id].count  = 0;
    pgy_qubit_pools_rt[id].active = true;
    return id;
}

static void
rt_pool_add(int32_t pool_id, int32_t qubit_id)
{
    if (pool_id < 0 || pool_id >= PGY_QUBIT_RT_MAX) return;
    PgyEntanglementPool_RT *pool = &pgy_qubit_pools_rt[pool_id];
    if (pool->count >= PGY_QUBIT_RT_MAX) return;
    for (int32_t i = 0; i < pool->count; i++)
        if (pool->members[i] == qubit_id) return;
    pool->members[pool->count++] = qubit_id;
    pgy_qubits_rt[qubit_id].pool_id = pool_id;
}

static void
rt_pool_remove(int32_t pool_id, int32_t qubit_id)
{
    if (pool_id < 0 || pool_id >= PGY_QUBIT_RT_MAX) return;
    PgyEntanglementPool_RT *pool = &pgy_qubit_pools_rt[pool_id];
    for (int32_t i = 0; i < pool->count; i++) {
        if (pool->members[i] == qubit_id) {
            pool->members[i] = pool->members[pool->count - 1];
            pool->count--;
            return;
        }
    }
}

static void
rt_pool_merge(int32_t dst_pool, int32_t src_pool)
{
    if (dst_pool == src_pool) return;
    if (dst_pool < 0 || src_pool < 0) return;
    PgyEntanglementPool_RT *src = &pgy_qubit_pools_rt[src_pool];
    for (int32_t i = 0; i < src->count; i++) {
        int32_t qid = src->members[i];
        rt_pool_add(dst_pool, qid);
    }
    src->count  = 0;
    src->active = false;
}
