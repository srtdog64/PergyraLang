/* =================================================================
 * QubitSlot — Quantum Resource Simulation
 *
 * Demonstrates that Pergyra's Slot model can express quantum
 * resource semantics: no-cloning, measurement collapse, entanglement.
 *
 * States: 0 = |0>, 1 = |1>, 2 = superposition, -1 = collapsed
 * ================================================================= */

#define PGY_QUBIT_MAX 64

typedef struct {
    int32_t state;       /* 0=|0>, 1=|1>, 2=superposition, -1=released */
    int32_t pool_id;     /* entanglement pool id, -1 if none */
    bool    measured;
} PgyQubit;

/* Entanglement pool — N-qubit group that collapses together */
typedef struct {
    int32_t members[PGY_QUBIT_MAX];
    int32_t count;
    bool    active;
} PgyEntanglementPool;

static PgyQubit _pgy_qubits[PGY_QUBIT_MAX];
static int32_t  _pgy_qubit_next = 0;
static bool     _pgy_qubit_rng_init = false;
static pthread_mutex_t _pgy_qubit_mutex = PTHREAD_MUTEX_INITIALIZER;

static PgyEntanglementPool _pgy_qubit_pools[PGY_QUBIT_MAX];
static int32_t _pgy_qubit_pool_next = 0;

/* --- Pool helpers --- */

static inline int32_t
_pgy_alloc_pool(void)
{
    if (_pgy_qubit_pool_next >= PGY_QUBIT_MAX) return -1;
    int32_t id = _pgy_qubit_pool_next++;
    _pgy_qubit_pools[id].count = 0;
    _pgy_qubit_pools[id].active = true;
    return id;
}

static inline void
_pgy_pool_add(int32_t pool_id, int32_t qubit_id)
{
    if (pool_id < 0 || pool_id >= PGY_QUBIT_MAX) return;
    PgyEntanglementPool *pool = &_pgy_qubit_pools[pool_id];
    if (pool->count >= PGY_QUBIT_MAX) return;
    /* Avoid duplicate */
    for (int32_t i = 0; i < pool->count; i++)
        if (pool->members[i] == qubit_id) return;
    pool->members[pool->count++] = qubit_id;
    _pgy_qubits[qubit_id].pool_id = pool_id;
}

static inline void
_pgy_pool_remove(int32_t pool_id, int32_t qubit_id)
{
    if (pool_id < 0 || pool_id >= PGY_QUBIT_MAX) return;
    PgyEntanglementPool *pool = &_pgy_qubit_pools[pool_id];
    for (int32_t i = 0; i < pool->count; i++) {
        if (pool->members[i] == qubit_id) {
            pool->members[i] = pool->members[pool->count - 1];
            pool->count--;
            return;
        }
    }
}

static inline void
_pgy_pool_merge(int32_t dst_pool, int32_t src_pool)
{
    if (dst_pool == src_pool) return;
    if (dst_pool < 0 || src_pool < 0) return;
    PgyEntanglementPool *src = &_pgy_qubit_pools[src_pool];
    for (int32_t i = 0; i < src->count; i++) {
        int32_t qid = src->members[i];
        _pgy_pool_add(dst_pool, qid);
        _pgy_qubits[qid].pool_id = dst_pool;
    }
    src->count = 0;
    src->active = false;
}

/* --- Qubit operations --- */

/* ClaimQubit() → allocate a qubit in superposition */
static inline int32_t
ClaimQubit(void)
{
    pthread_mutex_lock(&_pgy_qubit_mutex);
    if (!_pgy_qubit_rng_init) {
        pthread_mutex_lock(&pgy_runtime_rng_mutex);
        srand((unsigned)time(NULL));
        pthread_mutex_unlock(&pgy_runtime_rng_mutex);
        _pgy_qubit_rng_init = true;
    }
    if (_pgy_qubit_next >= PGY_QUBIT_MAX) {
        pthread_mutex_unlock(&_pgy_qubit_mutex);
        return -1;
    }
    int32_t id = _pgy_qubit_next++;
    _pgy_qubits[id].state    = 2; /* superposition */
    _pgy_qubits[id].pool_id  = -1;
    _pgy_qubits[id].measured = false;
    pthread_mutex_unlock(&_pgy_qubit_mutex);
    return id;
}

/* Measure(qubit) → collapse superposition, return 0 or 1.
 * Propagates collapse to ALL members of the entanglement pool. */
static inline int32_t
Measure(int32_t id)
{
    if (id < 0 || id >= PGY_QUBIT_MAX) return -1;
    pthread_mutex_lock(&_pgy_qubit_mutex);
    PgyQubit *q = &_pgy_qubits[id];

    if (q->measured) {
        int32_t state = q->state;
        pthread_mutex_unlock(&_pgy_qubit_mutex);
        return state;
    }

    /* Collapse superposition */
    if (q->state == 2) {
        pthread_mutex_lock(&pgy_runtime_rng_mutex);
        q->state = rand() % 2;
        pthread_mutex_unlock(&pgy_runtime_rng_mutex);
    }
    q->measured = true;

    /* Propagate to entire entanglement pool */
    if (q->pool_id >= 0) {
        PgyEntanglementPool *pool = &_pgy_qubit_pools[q->pool_id];
        for (int32_t i = 0; i < pool->count; i++) {
            int32_t mid = pool->members[i];
            if (mid != id && !_pgy_qubits[mid].measured) {
                _pgy_qubits[mid].state = q->state;
                _pgy_qubits[mid].measured = true;
            }
        }
    }

    int32_t state = q->state;
    pthread_mutex_unlock(&_pgy_qubit_mutex);
    return state;
}

/* Entangle(a, b) → merge entanglement pools.
 * Supports N-qubit entanglement (GHZ states). */
static inline void
Entangle(int32_t a, int32_t b)
{
    if (a < 0 || a >= PGY_QUBIT_MAX || b < 0 || b >= PGY_QUBIT_MAX) return;
    pthread_mutex_lock(&_pgy_qubit_mutex);
    int32_t pa = _pgy_qubits[a].pool_id;
    int32_t pb = _pgy_qubits[b].pool_id;

    if (pa >= 0 && pb >= 0) {
        if (pa != pb)
            _pgy_pool_merge(pa, pb);
    } else if (pa >= 0) {
        _pgy_pool_add(pa, b);
    } else if (pb >= 0) {
        _pgy_pool_add(pb, a);
    } else {
        int32_t new_pool = _pgy_alloc_pool();
        if (new_pool >= 0) {
            _pgy_pool_add(new_pool, a);
            _pgy_pool_add(new_pool, b);
        }
    }
    pthread_mutex_unlock(&_pgy_qubit_mutex);
}

/* H(qubit) → apply Hadamard gate (set to superposition) */
static inline void
H(int32_t id)
{
    if (id < 0 || id >= PGY_QUBIT_MAX) return;
    pthread_mutex_lock(&_pgy_qubit_mutex);
    _pgy_qubits[id].state = 2;
    _pgy_qubits[id].measured = false;
    pthread_mutex_unlock(&_pgy_qubit_mutex);
}

/* IntoClassical(qubit) → convert collapsed qubit to classical Bool.
 * Returns: 0→false, 1→true. Only valid after Measure(). */
static inline bool
IntoClassical(int32_t id)
{
    if (id < 0 || id >= PGY_QUBIT_MAX) return false;
    pthread_mutex_lock(&_pgy_qubit_mutex);
    bool result = _pgy_qubits[id].state == 1;
    pthread_mutex_unlock(&_pgy_qubit_mutex);
    return result;
}

/* QubitState(q) → 0=|0>, 1=|1>, 2=superposition */
static inline int32_t
QubitState(int32_t id)
{
    if (id < 0 || id >= PGY_QUBIT_MAX) return -1;
    pthread_mutex_lock(&_pgy_qubit_mutex);
    int32_t state = _pgy_qubits[id].state;
    pthread_mutex_unlock(&_pgy_qubit_mutex);
    return state;
}

/* IsCollapsed(q) → true if already measured */
static inline bool
IsCollapsed(int32_t id)
{
    if (id < 0 || id >= PGY_QUBIT_MAX) return true;
    pthread_mutex_lock(&_pgy_qubit_mutex);
    bool measured = _pgy_qubits[id].measured;
    pthread_mutex_unlock(&_pgy_qubit_mutex);
    return measured;
}

/* ReleaseQubit(q) → release quantum resource, remove from pool */
static inline void
ReleaseQubit(int32_t id)
{
    if (id < 0 || id >= PGY_QUBIT_MAX) return;
    pthread_mutex_lock(&_pgy_qubit_mutex);
    /* Remove from entanglement pool */
    if (_pgy_qubits[id].pool_id >= 0)
        _pgy_pool_remove(_pgy_qubits[id].pool_id, id);
    _pgy_qubits[id].state = -1;
    _pgy_qubits[id].pool_id = -1;
    _pgy_qubits[id].measured = true;
    pthread_mutex_unlock(&_pgy_qubit_mutex);
}
