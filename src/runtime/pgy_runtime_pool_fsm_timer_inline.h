/* =================================================================
 * Object Pool ??fixed-capacity reusable slot pool
 * use pool;
 * ================================================================= */

typedef struct
{
    void    *data;          /* flat array of items */
    uint8_t *alive;         /* alive flags */
    size_t   item_size;
    size_t   capacity;
    size_t   count;
} PgyPool;

static inline PgyPool pgy_pool_create(size_t item_size, size_t capacity)
{
    PgyPool p;
    p.item_size = item_size;
    p.capacity = capacity;
    p.count = 0;
    if (item_size == 0 || (capacity > 0 && item_size > SIZE_MAX / capacity)
        || capacity > SIZE_MAX / sizeof(uint8_t)) {
        p.data = NULL;
        p.alive = NULL;
        p.capacity = 0;
        return p;
    }
    p.data = calloc(capacity, item_size);
    p.alive = (uint8_t *)calloc(capacity, sizeof(uint8_t));
    if (p.data == NULL || p.alive == NULL) {
        free(p.data);
        free(p.alive);
        p.data = NULL;
        p.alive = NULL;
        p.capacity = 0;
    }
    return p;
}

static inline int32_t pgy_pool_spawn(PgyPool *p, const void *item)
{
    if (p == NULL || item == NULL || p->data == NULL || p->alive == NULL)
        return -1;
    for (size_t i = 0; i < p->capacity; i++) {
        if (!p->alive[i]) {
            memcpy((char *)p->data + i * p->item_size, item, p->item_size);
            p->alive[i] = 1;
            p->count++;
            return (int32_t)i;
        }
    }
    return -1; /* pool full */
}

static inline void pgy_pool_despawn(PgyPool *p, int32_t index)
{
    if (p == NULL || p->alive == NULL)
        return;
    if (index >= 0 && (size_t)index < p->capacity && p->alive[index]) {
        p->alive[index] = 0;
        p->count--;
    }
}

static inline void *pgy_pool_get(PgyPool *p, int32_t index)
{
    if (p == NULL || p->data == NULL || p->alive == NULL)
        return NULL;
    if (index < 0 || (size_t)index >= p->capacity || !p->alive[index])
        return NULL;
    return (char *)p->data + (size_t)index * p->item_size;
}

static inline bool pgy_pool_alive(PgyPool *p, int32_t index)
{
    if (p == NULL || p->alive == NULL)
        return false;
    return index >= 0 && (size_t)index < p->capacity && p->alive[index];
}

static inline int32_t pgy_pool_count(PgyPool *p) { return p != NULL ? (int32_t)p->count : 0; }
static inline int32_t pgy_pool_capacity(PgyPool *p) { return p != NULL ? (int32_t)p->capacity : 0; }

/* =================================================================
 * FSM ??Finite State Machine
 * use fsm;
 * ================================================================= */

#define PGY_FSM_MAX_STATES 32

typedef struct
{
    int32_t current;
    int32_t transitions[PGY_FSM_MAX_STATES][PGY_FSM_MAX_STATES]; /* transition[from][input] = to */
    char   *state_names[PGY_FSM_MAX_STATES];
    size_t  state_count;
} PgyFsm;

static inline PgyFsm pgy_fsm_new(void)
{
    PgyFsm f;
    memset(&f, 0, sizeof(f));
    f.current = 0;
    for (int i = 0; i < PGY_FSM_MAX_STATES; i++)
        for (int j = 0; j < PGY_FSM_MAX_STATES; j++)
            f.transitions[i][j] = -1;
    return f;
}

static inline int32_t pgy_fsm_add_state(PgyFsm *f, const char *name)
{
    if (f->state_count >= PGY_FSM_MAX_STATES) return -1;
    int32_t id = (int32_t)f->state_count;
    f->state_names[id] = pgy_runtime_strdup(name ? name : "");
    f->state_count++;
    return id;
}

static inline void pgy_fsm_add_transition(PgyFsm *f, int32_t from, int32_t input, int32_t to)
{
    if (from >= 0 && from < PGY_FSM_MAX_STATES && input >= 0 && input < PGY_FSM_MAX_STATES)
        f->transitions[from][input] = to;
}

static inline bool pgy_fsm_step(PgyFsm *f, int32_t input)
{
    if (f->current < 0 || f->current >= PGY_FSM_MAX_STATES) return false;
    int32_t next = f->transitions[f->current][input];
    if (next < 0) return false;
    f->current = next;
    return true;
}

static inline int32_t pgy_fsm_current(PgyFsm *f) { return f->current; }

static inline const char *pgy_fsm_current_name(PgyFsm *f)
{
    if (f->current < 0 || (size_t)f->current >= f->state_count) return "";
    return f->state_names[f->current] ? f->state_names[f->current] : "";
}

/* =================================================================
 * Timer / Cooldown
 * use timer;
 * ================================================================= */

typedef struct
{
    int32_t duration;
    int32_t remaining;
    bool    done;
} PgyTimer;

static inline PgyTimer pgy_timer_new(int32_t duration)
{
    PgyTimer t;
    t.duration = duration;
    t.remaining = duration;
    t.done = false;
    return t;
}

static inline void pgy_timer_tick(PgyTimer *t, int32_t delta)
{
    if (t->done) return;
    t->remaining -= delta;
    if (t->remaining <= 0) {
        t->remaining = 0;
        t->done = true;
    }
}

static inline bool pgy_timer_done(PgyTimer *t) { return t->done; }
static inline int32_t pgy_timer_remaining(PgyTimer *t) { return t->remaining; }

static inline void pgy_timer_reset(PgyTimer *t)
{
    t->remaining = t->duration;
    t->done = false;
}

typedef struct
{
    int32_t cooldown;
    int32_t remaining;
} PgyCooldown;

static inline PgyCooldown pgy_cooldown_new(int32_t cooldown)
{
    PgyCooldown c;
    c.cooldown = cooldown;
    c.remaining = 0;
    return c;
}

static inline void pgy_cooldown_tick(PgyCooldown *c, int32_t delta)
{
    c->remaining = (c->remaining > delta) ? c->remaining - delta : 0;
}

static inline bool pgy_cooldown_ready(PgyCooldown *c) { return c->remaining <= 0; }

static inline void pgy_cooldown_trigger(PgyCooldown *c)
{
    c->remaining = c->cooldown;
}
