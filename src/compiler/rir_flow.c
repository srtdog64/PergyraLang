#include "rir.h"
#include "rir_internal.h"

#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "hir.h"
#include "rir_flow_state.h"

static RIRResourceKind
rir_scope_resource_kind(const RIRScope *scope, const char *name)
{
    if (scope == NULL || name == NULL)
        return RIR_RESOURCE_UNKNOWN;

    for (size_t i = 0; i < scope->state_summary_count; i++) {
        if (scope->state_summaries[i].name != NULL
            && strcmp(scope->state_summaries[i].name, name) == 0) {
            return scope->state_summaries[i].resource_kind;
        }
    }

    for (size_t i = 0; i < scope->fact_count; i++) {
        if (scope->facts[i].name != NULL
            && strcmp(scope->facts[i].name, name) == 0) {
            return scope->facts[i].resource_kind;
        }
    }

    return RIR_RESOURCE_UNKNOWN;
}

static unsigned int
rir_refine_flow_semantics(unsigned int flags)
{
    if ((flags & RIR_FLOW_AUTHORITY) != 0
        && ((flags & RIR_FLOW_INVALIDATION) != 0
            || (flags & RIR_FLOW_WORLD_HANDOFF) != 0)) {
        flags |= RIR_FLOW_AUTHORITY_LOSS;
    }
    if ((flags & RIR_FLOW_PROJECTION) != 0
        && (flags & RIR_FLOW_INVALIDATION) != 0) {
        flags |= RIR_FLOW_PROJECTION_INVALIDATION;
    }
    return flags;
}

static unsigned int
rir_flow_semantics_for_scope(const RIRScope *scope)
{
    unsigned int flags = RIR_FLOW_NONE;

    if (scope == NULL)
        return flags;

    for (size_t i = 0; i < scope->fact_count; i++) {
        if (scope->facts[i].kind == RIR_FACT_AUTHORITY
            || scope->facts[i].kind == RIR_FACT_CAPABILITY) {
            flags |= RIR_FLOW_AUTHORITY;
        }
    }

    for (size_t i = 0; i < scope->op_count; i++) {
        const RIROp *op = &scope->ops[i];
        switch (op->kind) {
            case RIR_OP_AUTHORIZE:
                flags |= RIR_FLOW_AUTHORITY;
                break;
            case RIR_OP_PROJECT_REFRESH:
            case RIR_OP_PROJECT_PUBLISH:
                flags |= RIR_FLOW_PROJECTION;
                break;
            case RIR_OP_RELEASE:
            case RIR_OP_DETACH_EFFECT:
            case RIR_OP_UNLINK_RELATION:
            case RIR_OP_ABORT_INTENT:
            case RIR_OP_COMPENSATE_INTENT_STEP:
                flags |= RIR_FLOW_INVALIDATION;
                break;
            case RIR_OP_MOVE:
            case RIR_OP_CLAIM: {
                RIRResourceKind subject_kind = rir_scope_resource_kind(scope, op->subject);
                RIRResourceKind arg0_kind = rir_scope_resource_kind(scope, op->arg0);
                if (subject_kind == RIR_RESOURCE_ZONE_HANDLE
                    || subject_kind == RIR_RESOURCE_WORLD_HANDLE
                    || arg0_kind == RIR_RESOURCE_ZONE_HANDLE
                    || arg0_kind == RIR_RESOURCE_WORLD_HANDLE) {
                    flags |= RIR_FLOW_WORLD_HANDOFF;
                }
                if (subject_kind == RIR_RESOURCE_PROJECTION_OBJECT
                    || subject_kind == RIR_RESOURCE_PROJECTION_TOBJECT
                    || subject_kind == RIR_RESOURCE_ZONE_HANDLE
                    || subject_kind == RIR_RESOURCE_WORLD_HANDLE) {
                    flags |= RIR_FLOW_INVALIDATION;
                }
                break;
            }
            default:
                break;
        }
    }

    return rir_refine_flow_semantics(flags);
}

static unsigned int
rir_flow_semantics_for_op(const RIRScope *scope, const RIROp *op)
{
    RIRResourceKind subject_kind;
    RIRResourceKind arg0_kind;

    if (scope == NULL || op == NULL)
        return RIR_FLOW_NONE;

    switch (op->kind) {
        case RIR_OP_AUTHORIZE:
            return rir_refine_flow_semantics(RIR_FLOW_AUTHORITY);
        case RIR_OP_PROJECT_REFRESH:
        case RIR_OP_PROJECT_PUBLISH:
            return rir_refine_flow_semantics(RIR_FLOW_PROJECTION);
        case RIR_OP_RELEASE:
        case RIR_OP_DETACH_EFFECT:
        case RIR_OP_UNLINK_RELATION:
        case RIR_OP_ABORT_INTENT:
        case RIR_OP_COMPENSATE_INTENT_STEP:
            return rir_refine_flow_semantics(RIR_FLOW_INVALIDATION);
        case RIR_OP_MOVE:
        case RIR_OP_CLAIM:
            subject_kind = rir_scope_resource_kind(scope, op->subject);
            arg0_kind = rir_scope_resource_kind(scope, op->arg0);
            if (subject_kind == RIR_RESOURCE_ZONE_HANDLE
                || subject_kind == RIR_RESOURCE_WORLD_HANDLE
                || arg0_kind == RIR_RESOURCE_ZONE_HANDLE
                || arg0_kind == RIR_RESOURCE_WORLD_HANDLE) {
                return rir_refine_flow_semantics(RIR_FLOW_WORLD_HANDOFF | RIR_FLOW_INVALIDATION);
            }
            if (subject_kind == RIR_RESOURCE_PROJECTION_OBJECT
                || subject_kind == RIR_RESOURCE_PROJECTION_TOBJECT) {
                return rir_refine_flow_semantics(RIR_FLOW_INVALIDATION);
            }
            return RIR_FLOW_NONE;
        default:
            return RIR_FLOW_NONE;
    }
}

static bool
rir_normalize_scope(RIRScope *scope)
{
    if (scope == NULL)
        return true;

    free(scope->state_summaries);
    scope->state_summaries = NULL;
    scope->state_summary_count = 0;
    scope->has_state_errors = false;
    scope->conservative_semantics = RIR_FLOW_NONE;

    for (size_t i = 0; i < scope->fact_count; i++) {
        if (!scope_ensure_state_summary(scope, &scope->facts[i]))
            return false;
    }

    for (size_t i = 0; i < scope->op_count; i++) {
        RIROp *op = &scope->ops[i];
        RIRStateSummary *summary = scope_find_state_summary(scope, op->subject);
        if (summary == NULL)
            continue;
        rir_apply_op_to_summary(scope, summary, op);
    }

    scope->conservative_semantics = rir_flow_semantics_for_scope(scope);

    return true;
}

bool
rir_normalize_scope_shared(RIRScope *scope)
{
    return rir_normalize_scope(scope);
}

static RIRScopeKind
rir_scope_kind_from_hir(const HIRRoutine *routine)
{
    if (routine == NULL)
        return RIR_SCOPE_FUNCTION;
    if (routine->kind == HIR_TOPLEVEL_INTENT)
        return RIR_SCOPE_INTENT;
    if (routine->is_hosted || routine->is_action_like)
        return RIR_SCOPE_METHOD;
    return RIR_SCOPE_FUNCTION;
}

static RIRScope *
rir_find_matching_scope(RIRProgram *rir, const HIRRoutine *routine)
{
    RIRScopeKind wanted_kind;
    if (rir == NULL || routine == NULL || routine->name == NULL)
        return NULL;
    wanted_kind = rir_scope_kind_from_hir(routine);
    for (size_t i = 0; i < rir->scope_count; i++) {
        RIRScope *scope = &rir->scopes[i];
        if (scope->kind == wanted_kind
            && scope->name != NULL
            && strcmp(scope->name, routine->name) == 0) {
            return scope;
        }
    }
    return NULL;
}

static bool
rir_collect_block_ops(const HIRBasicBlock *block, RIROp **ops_out, size_t *op_count_out)
{
    RIRScope temp_scope;
    if (ops_out == NULL || op_count_out == NULL)
        return false;
    *ops_out = NULL;
    *op_count_out = 0;
    if (block == NULL)
        return true;
    memset(&temp_scope, 0, sizeof(temp_scope));
    for (size_t i = 0; i < block->statement_count; i++) {
        if (!rir_walk_block_node(&temp_scope, block->statements[i])) {
            free(temp_scope.ops);
            return false;
        }
    }
    if (!rir_walk_block_node(&temp_scope, block->terminator_condition)
        || !rir_walk_block_node(&temp_scope, block->terminator_value)) {
        free(temp_scope.ops);
        return false;
    }
    *ops_out = temp_scope.ops;
    *op_count_out = temp_scope.op_count;
    return true;
}

static bool
rir_prepare_flow_blocks(RIRScope *scope, const HIRRoutine *hir_routine)
{
    if (scope == NULL || hir_routine == NULL || !hir_routine->has_cfg)
        return true;
    rir_free_flow_blocks(scope);
    scope->flow_blocks = calloc(hir_routine->cfg.block_count, sizeof(RIRFlowBlock));
    if (scope->flow_blocks == NULL)
        return false;
    scope->flow_block_count = hir_routine->cfg.block_count;
    for (size_t i = 0; i < hir_routine->cfg.block_count; i++) {
        RIRFlowBlock *flow = &scope->flow_blocks[i];
        const HIRBasicBlock *hir_block = &hir_routine->cfg.blocks[i];
        flow->block_id = i;
        flow->is_reachable = hir_block->is_reachable;
        flow->is_join = hir_block->predecessor_count > 1;
        flow->entry_semantics = RIR_FLOW_NONE;
        flow->exit_semantics = RIR_FLOW_NONE;
        flow->fact_count = scope->state_summary_count;
        if (flow->fact_count == 0)
            continue;
        flow->facts = calloc(flow->fact_count, sizeof(RIRFlowFact));
        if (flow->facts == NULL)
            return false;
        for (size_t j = 0; j < flow->fact_count; j++) {
            flow->facts[j].name = scope->state_summaries[j].name;
            flow->facts[j].slot_anchor = scope->state_summaries[j].slot_anchor;
            flow->facts[j].entry_state = RIR_STATE_UNINIT;
            flow->facts[j].exit_state = RIR_STATE_UNINIT;
            flow->facts[j].merged_from_join = false;
            flow->facts[j].widened_by_loop = false;
            flow->facts[j].entry_conflict = false;
            flow->facts[j].has_merge_conflict = false;
        }
    }
    return true;
}

static bool
rir_enrich_scope_with_hir_flow(RIRScope *scope, const HIRRoutine *hir_routine)
{
    RIROp **block_ops = NULL;
    size_t *block_op_counts = NULL;
    bool changed;
    size_t limit;
    unsigned int baseline_semantics;

    if (scope == NULL || hir_routine == NULL || !hir_routine->has_cfg)
        return true;
    if (!rir_prepare_flow_blocks(scope, hir_routine))
        return false;
    baseline_semantics = rir_refine_flow_semantics(scope->conservative_semantics);

    block_ops = calloc(hir_routine->cfg.block_count, sizeof(RIROp *));
    block_op_counts = calloc(hir_routine->cfg.block_count, sizeof(size_t));
    if (block_ops == NULL || block_op_counts == NULL)
        goto oom;

    for (size_t i = 0; i < hir_routine->cfg.block_count; i++) {
        if (!rir_collect_block_ops(&hir_routine->cfg.blocks[i], &block_ops[i], &block_op_counts[i]))
            goto oom;
    }

    limit = hir_routine->cfg.block_count * 4 + 1;
    do {
        changed = false;
        for (size_t order = 0; order < hir_routine->cfg.block_count; order++) {
            const HIRBasicBlock *hir_block = NULL;
            RIRFlowBlock *flow = NULL;
            size_t block_id = SIZE_MAX;
            unsigned int merged_semantics = baseline_semantics;
            unsigned int exit_semantics = baseline_semantics;

            for (size_t i = 0; i < hir_routine->cfg.block_count; i++) {
                if (hir_routine->cfg.blocks[i].rpo_index == order) {
                    block_id = i;
                    break;
                }
            }
            if (block_id == SIZE_MAX)
                continue;
            hir_block = &hir_routine->cfg.blocks[block_id];
            flow = &scope->flow_blocks[block_id];
            if (!hir_block->is_reachable)
                continue;

            if (block_id != hir_routine->cfg.entry_block) {
                for (size_t p = 0; p < hir_block->predecessor_count; p++) {
                    size_t pred = hir_block->predecessors[p];
                    if (pred >= scope->flow_block_count || !scope->flow_blocks[pred].is_reachable)
                        continue;
                    merged_semantics |= scope->flow_blocks[pred].exit_semantics;
                }
            }

            for (size_t op_i = 0; op_i < block_op_counts[block_id]; op_i++)
                exit_semantics |= rir_flow_semantics_for_op(scope, &block_ops[block_id][op_i]);

            merged_semantics = rir_refine_flow_semantics(merged_semantics);
            exit_semantics = rir_refine_flow_semantics(exit_semantics);

            if (flow->entry_semantics != merged_semantics || flow->exit_semantics != exit_semantics)
                changed = true;
            flow->entry_semantics = merged_semantics;
            flow->exit_semantics = exit_semantics;

            if (hir_block->predecessor_count > 1 && exit_semantics != RIR_FLOW_NONE)
                scope->has_flow_sensitive_merge = true;

            if (flow->facts == NULL)
                continue;

            for (size_t fact_i = 0; fact_i < scope->state_summary_count; fact_i++) {
                RIRResourceState merged = RIR_STATE_UNINIT;
                bool merge_conflict = false;
                bool reachable_pred = false;

                if (block_id == hir_routine->cfg.entry_block) {
                    merged = scope->state_summaries[fact_i].initial_state;
                } else {
                    for (size_t p = 0; p < hir_block->predecessor_count; p++) {
                        size_t pred = hir_block->predecessors[p];
                        if (pred >= scope->flow_block_count || !scope->flow_blocks[pred].is_reachable)
                            continue;
                        merged = rir_merge_states_for_kind(scope->state_summaries[fact_i].resource_kind,
                                                           merged,
                                                           scope->flow_blocks[pred].facts[fact_i].exit_state,
                                                           &merge_conflict);
                        reachable_pred = true;
                    }
                    if (!reachable_pred)
                        merged = scope->state_summaries[fact_i].initial_state;
                }

                if (flow->facts[fact_i].entry_state != merged
                    || flow->facts[fact_i].entry_conflict != merge_conflict
                    || flow->facts[fact_i].widened_by_loop != (hir_block->is_loop_header
                                                               && hir_block->predecessor_count > 1)
                    || flow->facts[fact_i].merged_from_join != (hir_block->predecessor_count > 1)) {
                    changed = true;
                }
                flow->facts[fact_i].entry_state = merged;
                flow->facts[fact_i].merged_from_join = hir_block->predecessor_count > 1;
                flow->facts[fact_i].widened_by_loop = hir_block->is_loop_header
                                                      && hir_block->predecessor_count > 1;
                flow->facts[fact_i].entry_conflict = merge_conflict;
                if (flow->facts[fact_i].merged_from_join)
                    scope->has_flow_sensitive_merge = true;
                if (merge_conflict)
                    scope->has_state_errors = true;

                {
                    RIRResourceState exit_state = merged;
                    bool had_error = merge_conflict;
                    for (size_t op_i = 0; op_i < block_op_counts[block_id]; op_i++) {
                        const RIROp *op = &block_ops[block_id][op_i];
                        if (op->subject == NULL
                            || strcmp(op->subject, scope->state_summaries[fact_i].name) != 0)
                            continue;
                        rir_apply_op_to_state(scope->state_summaries[fact_i].resource_kind,
                                              &exit_state,
                                              &had_error,
                                              op->kind);
                    }
                    if (flow->facts[fact_i].exit_state != exit_state
                        || flow->facts[fact_i].has_merge_conflict != had_error) {
                        changed = true;
                    }
                    flow->facts[fact_i].exit_state = exit_state;
                    flow->facts[fact_i].has_merge_conflict = had_error;
                }
            }
        }
    } while (changed && --limit > 0);

    if (limit == 0)
        scope->has_state_errors = true;

    for (size_t i = 0; i < hir_routine->cfg.block_count; i++)
        free(block_ops[i]);
    free(block_ops);
    free(block_op_counts);
    return true;

oom:
    if (block_ops != NULL) {
        for (size_t i = 0; i < hir_routine->cfg.block_count; i++)
            free(block_ops[i]);
    }
    free(block_ops);
    free(block_op_counts);
    rir_free_flow_blocks(scope);
    return false;
}

bool
rir_enrich_with_hir_flow(RIRProgram *rir, const HIRProgram *hir, char **error_message)
{
    if (error_message != NULL)
        *error_message = NULL;
    if (rir == NULL || hir == NULL)
        return true;

    for (size_t i = 0; i < hir->routine_count; i++) {
        const HIRRoutine *hir_routine = &hir->routines[i];
        RIRScope *scope = rir_find_matching_scope(rir, hir_routine);
        if (scope == NULL)
            continue;
        if (!rir_enrich_scope_with_hir_flow(scope, hir_routine)) {
            if (error_message != NULL)
                *error_message = pergyra_strdup("out of memory");
            return false;
        }
    }

    return true;
}
