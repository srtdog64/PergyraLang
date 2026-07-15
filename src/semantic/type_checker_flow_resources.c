#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "type_checker_flow_resources.h"
#include "type_checker_flow_universe.h"
#include "type_checker_ownership_internal.h"

static bool
flow_snapshot_count_fits(size_t count)
{
    return count <= SIZE_MAX / sizeof(Symbol *)
        && count <= SIZE_MAX / sizeof(bool)
        && count <= SIZE_MAX / sizeof(uint8_t)
        && count <= SIZE_MAX / sizeof(SlotState)
        && count <= SIZE_MAX / sizeof(QubitSemanticState)
        && count <= SIZE_MAX / sizeof(int32_t);
}

static bool
flow_snapshot_tracks_classify(const Symbol *sym, SemanticContext *ctx)
{
    OwnershipTypeClass ownership;

    if (type_is_resource_handle(sym->type))
        return true;

    ownership = semantic_classify_ownership_type(sym->type, ctx);
    return ownership == OWNERSHIP_TYPE_MOVE_ONLY
        || ownership == OWNERSHIP_TYPE_BORROW_TRACKED
        || ownership == OWNERSHIP_TYPE_SUBJECT_IDENTITY
        || ownership == OWNERSHIP_TYPE_ANCHORED_HANDLE;
}

static bool
flow_snapshot_tracks_symbol(Symbol *sym, SemanticContext *ctx)
{
    bool tracks;
    /* PGY_SEMPERF_VERIFY_CLASSIFY_MEMO=1 recomputes on every memo hit and
     * fails loud on divergence -- the witness that classification really is
     * pointer-stable (no silent tracked-set drift). */
    static int verify_memo = -1;

    if (sym == NULL || sym->type == NULL)
        return false;

    if (sym->flow_tracks_memo != 0
        && sym->flow_tracks_memo_type == sym->type) {
        if (verify_memo < 0)
            verify_memo =
                getenv("PGY_SEMPERF_VERIFY_CLASSIFY_MEMO") != NULL ? 1 : 0;
        if (verify_memo == 1) {
            bool recomputed = flow_snapshot_tracks_classify(sym, ctx);
            if (recomputed != (sym->flow_tracks_memo == 2)) {
                fprintf(stderr,
                        "pgy: flow-snapshot classify memo diverged for"
                        " symbol '%s' (memo=%u recomputed=%d)\n",
                        sym->name != NULL ? sym->name : "(unnamed)",
                        (unsigned)sym->flow_tracks_memo,
                        (int)recomputed);
                abort();
            }
        }
        return sym->flow_tracks_memo == 2;
    }

    tracks = flow_snapshot_tracks_classify(sym, ctx);
    sym->flow_tracks_memo_type = sym->type;
    sym->flow_tracks_memo = tracks ? 2 : 1;
    return tracks;
}

ResourceConsumeSnapshot
snapshot_resource_states_from_scope(Scope *scope, SemanticContext *ctx)
{
    ResourceConsumeSnapshot snap = {0};
    snap.valid = true;

    while (scope != NULL) {
        for (size_t i = 0; i < scope->symbol_count; i++) {
            Symbol *sym = scope->symbols[i];
            if (!flow_snapshot_tracks_symbol(sym, ctx))
                continue;

            if (snap.count == snap.capacity) {
                size_t next_capacity;
                if (snap.capacity > SIZE_MAX / 2) {
                    destroy_resource_snapshot(&snap);
                    snap.valid = false;
                    return snap;
                }
                next_capacity = snap.capacity == 0 ? 8 : snap.capacity * 2;
                if (!flow_snapshot_count_fits(next_capacity)) {
                    destroy_resource_snapshot(&snap);
                    snap.valid = false;
                    return snap;
                }
                Symbol **new_symbols = calloc(next_capacity, sizeof(Symbol *));
                size_t *new_symbol_indices = calloc(next_capacity, sizeof(size_t));
                bool *new_states = calloc(next_capacity, sizeof(bool));
                bool *new_used_states = calloc(next_capacity, sizeof(bool));
                uint8_t *new_access_masks = calloc(next_capacity,
                                                    sizeof(uint8_t));
                SlotState *new_slot_states = calloc(next_capacity, sizeof(SlotState));
                QubitSemanticState *new_sem = calloc(next_capacity,
                                                     sizeof(QubitSemanticState));
                int32_t *new_pools = calloc(next_capacity, sizeof(int32_t));
                if (new_symbols == NULL || new_symbol_indices == NULL
                    || new_states == NULL
                    || new_used_states == NULL || new_access_masks == NULL
                    || new_slot_states == NULL
                    || new_sem == NULL || new_pools == NULL) {
                    free(new_symbols);
                    free(new_symbol_indices);
                    free(new_states);
                    free(new_used_states);
                    free(new_access_masks);
                    free(new_slot_states);
                    free(new_sem);
                    free(new_pools);
                    destroy_resource_snapshot(&snap);
                    snap.valid = false;
                    return snap;
                }
                if (snap.count > 0) {
                    memcpy(new_symbols, snap.symbols, snap.count * sizeof(Symbol *));
                    memcpy(new_symbol_indices, snap.symbol_indices,
                           snap.count * sizeof(size_t));
                    memcpy(new_states, snap.states, snap.count * sizeof(bool));
                    memcpy(new_used_states, snap.used_states, snap.count * sizeof(bool));
                    memcpy(new_access_masks, snap.access_masks,
                           snap.count * sizeof(uint8_t));
                    memcpy(new_slot_states, snap.slot_states,
                           snap.count * sizeof(SlotState));
                    memcpy(new_sem, snap.sem_states,
                           snap.count * sizeof(QubitSemanticState));
                    memcpy(new_pools, snap.pool_ids, snap.count * sizeof(int32_t));
                }
                free(snap.symbols);
                free(snap.symbol_indices);
                free(snap.states);
                free(snap.used_states);
                free(snap.access_masks);
                free(snap.slot_states);
                free(snap.sem_states);
                free(snap.pool_ids);
                snap.symbols = new_symbols;
                snap.symbol_indices = new_symbol_indices;
                snap.states = new_states;
                snap.used_states = new_used_states;
                snap.access_masks = new_access_masks;
                snap.slot_states = new_slot_states;
                snap.sem_states = new_sem;
                snap.pool_ids = new_pools;
                snap.capacity = next_capacity;
            }
            if (snap.symbols == NULL || snap.symbol_indices == NULL
                || snap.states == NULL
                || snap.used_states == NULL || snap.access_masks == NULL
                || snap.slot_states == NULL
                || snap.sem_states == NULL || snap.pool_ids == NULL) {
                destroy_resource_snapshot(&snap);
                snap.valid = false;
                return snap;
            }
            snap.symbols[snap.count] = sym;
            snap.symbol_indices[snap.count] =
                resource_flow_universe_bind(ctx, sym);
            if (ctx != NULL && ctx->current_function_decl != NULL
                && snap.symbol_indices[snap.count]
                    == RESOURCE_FLOW_INDEX_NONE) {
                destroy_resource_snapshot(&snap);
                snap.valid = false;
                return snap;
            }
            snap.states[snap.count] = sym->is_consumed;
            snap.used_states[snap.count] = sym->is_used;
            snap.access_masks[snap.count] = sym->slot_flow_access_mask;
            snap.slot_states[snap.count] = sym->slot_info.state;
            snap.sem_states[snap.count] = sym->qubit_info.semantic_state;
            snap.pool_ids[snap.count] = sym->qubit_info.entangle_pool_id;
            snap.count++;
        }
        scope = scope->parent;
    }

    return snap;
}

ResourceConsumeSnapshot
snapshot_resource_states(SemanticContext *ctx)
{
    return snapshot_resource_states_from_scope(ctx != NULL ? ctx->scope : NULL,
        ctx);
}

void
merge_resource_states_or(ResourceConsumeSnapshot *dst,
                         const ResourceConsumeSnapshot *src)
{
    if (dst == NULL || src == NULL)
        return;
    if (!dst->valid || !src->valid) {
        dst->valid = false;
        return;
    }
    if (dst->count != src->count) {
        dst->valid = false;
        return;
    }
    if ((dst->count > 0
            && (dst->symbols == NULL || dst->states == NULL
                || dst->used_states == NULL || dst->access_masks == NULL
                || dst->slot_states == NULL || dst->sem_states == NULL
                || dst->pool_ids == NULL))
        || (src->count > 0
            && (src->symbols == NULL || src->states == NULL
                || src->used_states == NULL || src->access_masks == NULL
                || src->slot_states == NULL || src->sem_states == NULL
                || src->pool_ids == NULL))) {
        dst->valid = false;
        return;
    }

    for (size_t i = 0; i < dst->count; i++) {
        size_t src_index = SIZE_MAX;
        for (size_t j = 0; j < src->count; j++) {
            bool identity_matches;
            if (src->symbol_indices != NULL && dst->symbol_indices != NULL
                && src->symbol_indices[j] != RESOURCE_FLOW_INDEX_NONE
                && dst->symbol_indices[i] != RESOURCE_FLOW_INDEX_NONE) {
                identity_matches =
                    src->symbol_indices[j] == dst->symbol_indices[i];
            } else {
                identity_matches = src->symbols[j] == dst->symbols[i];
            }
            if (identity_matches) {
                src_index = j;
                break;
            }
        }
        if (src_index == SIZE_MAX) {
            dst->valid = false;
            return;
        }

        dst->states[i] = dst->states[i] || src->states[src_index];
        dst->used_states[i] = dst->used_states[i]
            || src->used_states[src_index];
        dst->access_masks[i] |= src->access_masks[src_index];
        if (src->slot_states[src_index] > dst->slot_states[i])
            dst->slot_states[i] = src->slot_states[src_index];
        if (src->sem_states[src_index] > dst->sem_states[i])
            dst->sem_states[i] = src->sem_states[src_index];
        if (dst->pool_ids[i] < 0)
            dst->pool_ids[i] = src->pool_ids[src_index];
    }
}

static bool
resource_snapshot_entry_writes(const ResourceConsumeSnapshot *snap,
                               size_t index)
{
    return snap != NULL
        && index < snap->count
        && (snap->access_masks[index]
            & (PGY_SLOT_FLOW_ACCESS_WRITE
               | PGY_SLOT_FLOW_ACCESS_RELEASE)) != 0;
}

static bool
resource_snapshot_entry_reads(const ResourceConsumeSnapshot *snap,
                              size_t index)
{
    return snap != NULL
        && index < snap->count
        && (snap->access_masks[index] & PGY_SLOT_FLOW_ACCESS_READ) != 0;
}

static bool
resource_snapshot_entry_releases(const ResourceConsumeSnapshot *snap,
                                 size_t index)
{
    return snap != NULL
        && index < snap->count
        && (snap->access_masks[index] & PGY_SLOT_FLOW_ACCESS_RELEASE) != 0;
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
resource_snapshot_entry_writer_like(const ResourceConsumeSnapshot *snap,
                                    size_t index)
{
    return resource_snapshot_entry_writes(snap, index)
        || resource_snapshot_entry_unavailable(snap, index);
}

static bool
resource_snapshot_entry_reader_delta(const ResourceConsumeSnapshot *base,
                                     const ResourceConsumeSnapshot *snap,
                                     Symbol *sym,
                                     size_t snap_index)
{
    size_t base_index = 0;
    bool base_reads = false;

    if (!resource_snapshot_entry_reads(snap, snap_index))
        return false;
    if (resource_snapshot_find_symbol(base, sym, &base_index))
        base_reads = resource_snapshot_entry_reads(base, base_index);
    return !base_reads;
}

static bool
resource_snapshot_entry_writer_delta(const ResourceConsumeSnapshot *base,
                                     const ResourceConsumeSnapshot *snap,
                                     Symbol *sym,
                                     size_t snap_index)
{
    size_t base_index = 0;
    bool base_writes = false;

    if (!resource_snapshot_entry_writer_like(snap, snap_index))
        return false;
    if (resource_snapshot_find_symbol(base, sym, &base_index))
        base_writes = resource_snapshot_entry_writer_like(base, base_index);
    return !base_writes;
}

static bool
resource_snapshot_current_reader_delta(const ResourceConsumeSnapshot *base,
                                       const ResourceConsumeSnapshot *current,
                                       Symbol *sym)
{
    size_t current_index = 0;

    if (current == NULL || !current->valid)
        return false;
    if (!resource_snapshot_find_symbol(current, sym, &current_index))
        return false;
    return resource_snapshot_entry_reader_delta(base, current, sym,
                                                current_index);
}

static bool
resource_snapshot_current_writer_delta(const ResourceConsumeSnapshot *base,
                                       const ResourceConsumeSnapshot *current,
                                       Symbol *sym)
{
    size_t current_index = 0;

    if (current == NULL || !current->valid)
        return false;
    if (!resource_snapshot_find_symbol(current, sym, &current_index))
        return false;
    return resource_snapshot_entry_writer_delta(base, current, sym,
                                                current_index);
}

void
resource_snapshot_record_parallel_boundary_witness(
    const ResourceConsumeSnapshot *base,
    const ResourceConsumeSnapshot *joined,
    const ResourceConsumeSnapshot *task,
    SemanticContext *ctx)
{
    if (base == NULL || task == NULL || ctx == NULL)
        return;
    if (!base->valid || !task->valid)
        return;

    for (size_t i = 0; i < task->count; i++) {
        Symbol *sym = task->symbols[i];
        bool task_reads;
        bool task_writes;
        bool current_reads;
        bool current_writes;
        bool accepted;

        if (sym == NULL)
            continue;

        task_reads = resource_snapshot_entry_reader_delta(base, task, sym, i);
        task_writes = resource_snapshot_entry_writer_delta(base, task, sym, i);
        if (!task_reads && !task_writes)
            continue;

        current_reads =
            resource_snapshot_current_reader_delta(base, joined, sym);
        current_writes =
            resource_snapshot_current_writer_delta(base, joined, sym);

        if (task_writes) {
            accepted = pgy_boundary_witness_guard_accepts(
                current_reads ? 1u : 0u,
                current_writes,
                PGY_BOUNDARY_WITNESS_OP_ACQ_WRITE);
            semantic_boundary_witness_record_acq_write(ctx, accepted);
            if (accepted && resource_snapshot_entry_releases(task, i))
                semantic_boundary_witness_record_release(ctx);
        } else {
            accepted = pgy_boundary_witness_guard_accepts(
                current_reads ? 1u : 0u,
                current_writes,
                PGY_BOUNDARY_WITNESS_OP_ACQ_READ);
            semantic_boundary_witness_record_acq_read(ctx, accepted);
        }
    }
}

bool
resource_snapshot_has_parallel_conflict(const ResourceConsumeSnapshot *base,
                                        const ResourceConsumeSnapshot *joined,
                                        const ResourceConsumeSnapshot *task,
                                        const Symbol **symbol_out)
{
    if (base == NULL || joined == NULL || task == NULL)
        return false;
    if (!base->valid || !joined->valid || !task->valid)
        return false;

    for (size_t i = 0; i < task->count; i++) {
        size_t base_idx = 0;
        size_t joined_idx = 0;
        Symbol *sym = task->symbols[i];
        bool task_unavailable;
        bool task_used;
        bool task_writes;
        bool base_unavailable = false;
        bool base_used = false;
        bool base_writes = false;
        bool joined_unavailable;
        bool joined_used;
        bool joined_writes;
        bool task_unavailable_delta;
        bool task_used_delta;
        bool task_write_delta;
        bool joined_unavailable_delta;
        bool joined_used_delta;
        bool joined_write_delta;
        if (sym == NULL)
            continue;
        task_unavailable = resource_snapshot_entry_unavailable(task, i);
        task_used = resource_snapshot_entry_used(task, i);
        task_writes = resource_snapshot_entry_writes(task, i);
        if (!task_unavailable && !task_used && !task_writes)
            continue;

        if (resource_snapshot_find_symbol(base, sym, &base_idx)) {
            base_unavailable =
                resource_snapshot_entry_unavailable(base, base_idx);
            base_used = resource_snapshot_entry_used(base, base_idx);
            base_writes = resource_snapshot_entry_writes(base, base_idx);
        }
        if (!resource_snapshot_find_symbol(joined, sym, &joined_idx))
            continue;

        joined_unavailable =
            resource_snapshot_entry_unavailable(joined, joined_idx);
        joined_used = resource_snapshot_entry_used(joined, joined_idx);
        joined_writes = resource_snapshot_entry_writes(joined, joined_idx);

        task_unavailable_delta = task_unavailable && !base_unavailable;
        task_used_delta = task_used && !base_used;
        task_write_delta = task_writes && !base_writes;
        joined_unavailable_delta = joined_unavailable && !base_unavailable;
        joined_used_delta = joined_used && !base_used;
        joined_write_delta = joined_writes && !base_writes;

        if ((task_write_delta && joined_write_delta)
            || (task_unavailable_delta
                && (joined_unavailable_delta || joined_used_delta))
            || (task_used_delta && joined_unavailable_delta)) {
            if (symbol_out != NULL)
                *symbol_out = sym;
            return true;
        }
    }

    return false;
}

bool
resource_snapshot_has_parallel_race_risk(const ResourceConsumeSnapshot *base,
                                         const ResourceConsumeSnapshot *joined,
                                         const ResourceConsumeSnapshot *task,
                                         const Symbol **symbol_out)
{
    if (base == NULL || joined == NULL || task == NULL)
        return false;
    if (!base->valid || !joined->valid || !task->valid)
        return false;

    for (size_t i = 0; i < task->count; i++) {
        size_t base_idx = 0;
        size_t joined_idx = 0;
        Symbol *sym = task->symbols[i];
        bool base_reads = false;
        bool base_writes = false;
        bool task_reads;
        bool task_writes;
        bool joined_reads;
        bool joined_writes;
        bool task_read_delta;
        bool task_write_delta;
        bool joined_read_delta;
        bool joined_write_delta;

        if (sym == NULL)
            continue;
        task_reads = resource_snapshot_entry_reads(task, i);
        task_writes = resource_snapshot_entry_writes(task, i)
            || resource_snapshot_entry_unavailable(task, i);
        if (!task_reads && !task_writes)
            continue;

        if (resource_snapshot_find_symbol(base, sym, &base_idx)) {
            base_reads = resource_snapshot_entry_reads(base, base_idx);
            base_writes = resource_snapshot_entry_writes(base, base_idx)
                || resource_snapshot_entry_unavailable(base, base_idx);
        }
        if (!resource_snapshot_find_symbol(joined, sym, &joined_idx))
            continue;

        joined_reads = resource_snapshot_entry_reads(joined, joined_idx);
        joined_writes = resource_snapshot_entry_writes(joined, joined_idx)
            || resource_snapshot_entry_unavailable(joined, joined_idx);

        task_read_delta = task_reads && !base_reads;
        task_write_delta = task_writes && !base_writes;
        joined_read_delta = joined_reads && !base_reads;
        joined_write_delta = joined_writes && !base_writes;

        if ((task_write_delta && joined_read_delta)
            || (task_read_delta && joined_write_delta)) {
            if (symbol_out != NULL)
                *symbol_out = sym;
            return true;
        }
    }

    return false;
}
