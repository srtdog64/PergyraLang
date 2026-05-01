typedef struct
{
    Symbol              **symbols;
    bool                 *states;
    bool                 *used_states;
    SlotState            *slot_states;
    QubitSemanticState   *sem_states;
    int32_t              *pool_ids;
    size_t                count;
    size_t                capacity;
} ResourceConsumeSnapshot;

static void destroy_resource_snapshot(ResourceConsumeSnapshot *snap);

static bool
flow_snapshot_tracks_symbol(const Symbol *sym, SemanticContext *ctx)
{
    OwnershipTypeClass ownership;

    if (sym == NULL || sym->type == NULL)
        return false;
    if (type_is_resource_handle(sym->type))
        return true;

    ownership = semantic_classify_ownership_type(sym->type, ctx);
    return ownership == OWNERSHIP_TYPE_MOVE_ONLY
        || ownership == OWNERSHIP_TYPE_BORROW_TRACKED
        || ownership == OWNERSHIP_TYPE_SUBJECT_IDENTITY
        || ownership == OWNERSHIP_TYPE_ANCHORED_HANDLE;
}

static ResourceConsumeSnapshot
snapshot_resource_states_from_scope(Scope *scope, SemanticContext *ctx)
{
    ResourceConsumeSnapshot snap = {0};

    while (scope != NULL) {
        for (size_t i = 0; i < scope->symbol_count; i++) {
            Symbol *sym = scope->symbols[i];
            if (!flow_snapshot_tracks_symbol(sym, ctx))
                continue;

            if (snap.count == snap.capacity) {
                size_t next_capacity = snap.capacity == 0 ? 8 : snap.capacity * 2;
                Symbol **new_symbols = calloc(next_capacity, sizeof(Symbol *));
                bool *new_states = calloc(next_capacity, sizeof(bool));
                bool *new_used_states = calloc(next_capacity, sizeof(bool));
                SlotState *new_slot_states = calloc(next_capacity, sizeof(SlotState));
                QubitSemanticState *new_sem = calloc(next_capacity, sizeof(QubitSemanticState));
                int32_t *new_pools = calloc(next_capacity, sizeof(int32_t));
                if (new_symbols == NULL || new_states == NULL
                    || new_used_states == NULL || new_slot_states == NULL
                    || new_sem == NULL || new_pools == NULL) {
                    free(new_symbols);
                    free(new_states);
                    free(new_used_states);
                    free(new_slot_states);
                    free(new_sem);
                    free(new_pools);
                    destroy_resource_snapshot(&snap);
                    return snap;
                }
                if (snap.count > 0) {
                    memcpy(new_symbols, snap.symbols, snap.count * sizeof(Symbol *));
                    memcpy(new_states, snap.states, snap.count * sizeof(bool));
                    memcpy(new_used_states, snap.used_states, snap.count * sizeof(bool));
                    memcpy(new_slot_states, snap.slot_states, snap.count * sizeof(SlotState));
                    memcpy(new_sem, snap.sem_states, snap.count * sizeof(QubitSemanticState));
                    memcpy(new_pools, snap.pool_ids, snap.count * sizeof(int32_t));
                }
                free(snap.symbols);
                free(snap.states);
                free(snap.used_states);
                free(snap.slot_states);
                free(snap.sem_states);
                free(snap.pool_ids);
                snap.symbols = new_symbols;
                snap.states = new_states;
                snap.used_states = new_used_states;
                snap.slot_states = new_slot_states;
                snap.sem_states = new_sem;
                snap.pool_ids = new_pools;
                snap.capacity = next_capacity;
            }
            if (snap.symbols == NULL || snap.states == NULL
                || snap.used_states == NULL || snap.slot_states == NULL
                || snap.sem_states == NULL || snap.pool_ids == NULL) {
                destroy_resource_snapshot(&snap);
                return snap;
            }
            snap.symbols[snap.count] = sym;
            snap.states[snap.count] = sym->is_consumed;
            snap.used_states[snap.count] = sym->is_used;
            snap.slot_states[snap.count] = sym->slot_info.state;
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
            snap->symbols[i]->is_used = snap->used_states[i];
            snap->symbols[i]->slot_info.state = snap->slot_states[i];
            snap->symbols[i]->qubit_info.semantic_state = snap->sem_states[i];
            snap->symbols[i]->qubit_info.entangle_pool_id = snap->pool_ids[i];
        }
    }
}

static ResourceConsumeSnapshot
snapshot_resource_states(SemanticContext *ctx)
{
    return snapshot_resource_states_from_scope(ctx != NULL ? ctx->scope : NULL,
        ctx);
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
        dst->used_states[i] = dst->used_states[i] || src->used_states[i];
        if (src->slot_states[i] > dst->slot_states[i])
            dst->slot_states[i] = src->slot_states[i];
        if (src->sem_states[i] > dst->sem_states[i])
            dst->sem_states[i] = src->sem_states[i];
        if (dst->pool_ids[i] < 0)
            dst->pool_ids[i] = src->pool_ids[i];
    }
}

static bool
resource_snapshot_entry_unavailable(const ResourceConsumeSnapshot *snap,
                                    size_t index)
{
    return snap != NULL
        && index < snap->count
        && (snap->states[index]
            || snap->slot_states[index] == SLOT_STATE_RELEASED);
}

static bool
resource_snapshot_entry_used(const ResourceConsumeSnapshot *snap, size_t index)
{
    return snap != NULL
        && index < snap->count
        && snap->used_states[index];
}

static bool
resource_snapshot_find_symbol(const ResourceConsumeSnapshot *snap,
                              const Symbol *symbol,
                              size_t *index_out)
{
    if (snap == NULL || symbol == NULL)
        return false;
    for (size_t i = 0; i < snap->count; i++) {
        if (snap->symbols[i] == symbol) {
            if (index_out != NULL)
                *index_out = i;
            return true;
        }
    }
    return false;
}

static bool
resource_snapshot_has_parallel_conflict(const ResourceConsumeSnapshot *base,
                                        const ResourceConsumeSnapshot *joined,
                                        const ResourceConsumeSnapshot *task,
                                        const Symbol **symbol_out)
{
    if (base == NULL || joined == NULL || task == NULL)
        return false;

    for (size_t i = 0; i < task->count; i++) {
        size_t base_idx = 0;
        size_t joined_idx = 0;
        Symbol *sym = task->symbols[i];
        bool task_unavailable;
        bool task_used;
        bool base_unavailable = false;
        bool base_used = false;
        bool joined_unavailable;
        bool joined_used;
        bool task_unavailable_delta;
        bool task_used_delta;
        bool joined_unavailable_delta;
        bool joined_used_delta;
        if (sym == NULL)
            continue;
        task_unavailable = resource_snapshot_entry_unavailable(task, i);
        task_used = resource_snapshot_entry_used(task, i);
        if (!task_unavailable && !task_used)
            continue;

        if (resource_snapshot_find_symbol(base, sym, &base_idx)) {
            base_unavailable =
                resource_snapshot_entry_unavailable(base, base_idx);
            base_used = resource_snapshot_entry_used(base, base_idx);
        }
        if (!resource_snapshot_find_symbol(joined, sym, &joined_idx))
            continue;

        joined_unavailable =
            resource_snapshot_entry_unavailable(joined, joined_idx);
        joined_used = resource_snapshot_entry_used(joined, joined_idx);

        task_unavailable_delta = task_unavailable && !base_unavailable;
        task_used_delta = task_used && !base_used;
        joined_unavailable_delta = joined_unavailable && !base_unavailable;
        joined_used_delta = joined_used && !base_used;

        if ((task_unavailable_delta
                && (joined_unavailable_delta || joined_used_delta))
            || (task_used_delta && joined_unavailable_delta)) {
            if (symbol_out != NULL)
                *symbol_out = sym;
            return true;
        }
    }

    return false;
}

static void
destroy_resource_snapshot(ResourceConsumeSnapshot *snap)
{
    if (snap == NULL)
        return;
    free(snap->symbols);
    free(snap->states);
    free(snap->used_states);
    free(snap->slot_states);
    free(snap->sem_states);
    free(snap->pool_ids);
    snap->symbols = NULL;
    snap->states = NULL;
    snap->used_states = NULL;
    snap->slot_states = NULL;
    snap->sem_states = NULL;
    snap->pool_ids = NULL;
    snap->count = 0;
    snap->capacity = 0;
}
