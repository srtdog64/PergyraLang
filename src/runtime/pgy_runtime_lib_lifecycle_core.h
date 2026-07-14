#ifndef PGY_RUNTIME_LIB_LIFECYCLE_CORE_H
#define PGY_RUNTIME_LIB_LIFECYCLE_CORE_H

/* External lifecycle-state twin for the LLVM-linked runtime object. */
#ifndef PGY_LIFECYCLE_MAP_CAP
#define PGY_LIFECYCLE_MAP_CAP 256
#endif

typedef struct {
    const void *key;
    int32_t     state;
} PgyLifecycleEntryExt;

static PgyLifecycleEntryExt *
pgy_runtime_lifecycle_slot_ext(const void *inst, int create)
{
    static PgyLifecycleEntryExt entries[PGY_LIFECYCLE_MAP_CAP];
    size_t cap = (size_t)PGY_LIFECYCLE_MAP_CAP;
    uintptr_t h;
    size_t start;
    size_t probe;

    if (inst == NULL)
        return NULL;
    h = (uintptr_t)inst;
    h ^= h >> 7;
    h *= (uintptr_t)2654435761u;
    h ^= h >> 11;
    start = (size_t)h % cap;
    for (probe = 0; probe < cap; probe++) {
        size_t idx = (start + probe) % cap;
        if (entries[idx].key == inst)
            return &entries[idx];
        if (entries[idx].key == NULL) {
            if (!create)
                return NULL;
            entries[idx].key = inst;
            entries[idx].state = 0;
            return &entries[idx];
        }
    }
    return NULL;
}

void
pgy_runtime_lifecycle_set_export(const void *inst, int32_t state)
{
    PgyLifecycleEntryExt *e = pgy_runtime_lifecycle_slot_ext(inst, 1);
    if (e != NULL)
        e->state = state;
}

void
pgy_runtime_lifecycle_guard_export(const void *inst, int32_t valid_mask,
                                   int32_t to_state, const char *op,
                                   const char *subject)
{
    PgyLifecycleEntryExt *e = pgy_runtime_lifecycle_slot_ext(inst, 1);
    int32_t state = (e != NULL) ? e->state : 0;

    if (state < 0 || state >= 32 || ((valid_mask >> state) & 1) == 0) {
        fprintf(stderr, "%s lifecycle op=%s subject=%s state=%d permitted_mask=0x%x\n",
                PGY_RUNTIME_PANIC_PREFIX,
                op != NULL ? op : "<op>",
                subject != NULL ? subject : "<subject>",
                (int)state, (unsigned)valid_mask);
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INVALID_LIFECYCLE_STATE,
                          PGY_RUNTIME_PANIC_REASON_INVALID_LIFECYCLE_STATE);
    }
    if (e != NULL && to_state >= 0)
        e->state = to_state;
}

#endif /* PGY_RUNTIME_LIB_LIFECYCLE_CORE_H */
