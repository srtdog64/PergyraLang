typedef struct
{
    Symbol              **symbols;
    bool                 *states;
    QubitSemanticState   *sem_states;
    int32_t              *pool_ids;
    size_t                count;
} ResourceConsumeSnapshot;

static ResourceConsumeSnapshot
snapshot_resource_states_from_scope(Scope *scope)
{
    ResourceConsumeSnapshot snap = {0};

    while (scope != NULL) {
        for (size_t i = 0; i < scope->symbol_count; i++) {
            Symbol *sym = scope->symbols[i];
            if (sym == NULL || !type_is_qubit(sym->type))
                continue;

            Symbol **new_symbols = realloc(snap.symbols,
                (snap.count + 1) * sizeof(Symbol *));
            bool *new_states = realloc(snap.states,
                (snap.count + 1) * sizeof(bool));
            QubitSemanticState *new_sem = realloc(snap.sem_states,
                (snap.count + 1) * sizeof(QubitSemanticState));
            int32_t *new_pools = realloc(snap.pool_ids,
                (snap.count + 1) * sizeof(int32_t));
            if (new_symbols == NULL || new_states == NULL
                || new_sem == NULL || new_pools == NULL) {
                free(new_symbols);
                free(new_states);
                free(new_sem);
                free(new_pools);
                snap.symbols = NULL;
                snap.states = NULL;
                snap.sem_states = NULL;
                snap.pool_ids = NULL;
                snap.count = 0;
                return snap;
            }

            snap.symbols = new_symbols;
            snap.states = new_states;
            snap.sem_states = new_sem;
            snap.pool_ids = new_pools;
            snap.symbols[snap.count] = sym;
            snap.states[snap.count] = sym->is_consumed;
            snap.sem_states[snap.count] = sym->qubit_info.semantic_state;
            snap.pool_ids[snap.count] = sym->qubit_info.entangle_pool_id;
            snap.count++;
        }
        scope = scope->parent;
    }

    return snap;
}

static void
restore_resource_states(const ResourceConsumeSnapshot *snap)
{
    if (snap == NULL)
        return;
    for (size_t i = 0; i < snap->count; i++) {
        if (snap->symbols[i] != NULL) {
            snap->symbols[i]->is_consumed = snap->states[i];
            snap->symbols[i]->qubit_info.semantic_state = snap->sem_states[i];
            snap->symbols[i]->qubit_info.entangle_pool_id = snap->pool_ids[i];
        }
    }
}

static ResourceConsumeSnapshot
snapshot_resource_states(SemanticContext *ctx)
{
    return snapshot_resource_states_from_scope(ctx != NULL ? ctx->scope : NULL);
}

static void
merge_resource_states_or(ResourceConsumeSnapshot *dst,
                         const ResourceConsumeSnapshot *src)
{
    if (dst == NULL || src == NULL)
        return;
    size_t count = dst->count < src->count ? dst->count : src->count;
    for (size_t i = 0; i < count; i++) {
        dst->states[i] = dst->states[i] || src->states[i];
        if (src->sem_states[i] > dst->sem_states[i])
            dst->sem_states[i] = src->sem_states[i];
        if (dst->pool_ids[i] < 0)
            dst->pool_ids[i] = src->pool_ids[i];
    }
}

static void
destroy_resource_snapshot(ResourceConsumeSnapshot *snap)
{
    if (snap == NULL)
        return;
    free(snap->symbols);
    free(snap->states);
    free(snap->sem_states);
    free(snap->pool_ids);
    snap->symbols = NULL;
    snap->states = NULL;
    snap->sem_states = NULL;
    snap->pool_ids = NULL;
    snap->count = 0;
}
