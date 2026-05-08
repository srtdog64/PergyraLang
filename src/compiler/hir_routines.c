#include "hir_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "../common/string_compat.h"

#include "hir_analysis.h"
#include "hir_cfg.h"
#include "hir_lower_cfg.h"

static bool
hir_routines_next_capacity(size_t *capacity, size_t initial, size_t elem_size)
{
    if (capacity == NULL || initial == 0 || elem_size == 0)
        return false;

    size_t current = *capacity;
    size_t next_capacity = current == 0 ? initial : current * 2;
    if (current != 0 && next_capacity < current)
        return false;
    if (next_capacity > SIZE_MAX / elem_size)
        return false;

    *capacity = next_capacity;
    return true;
}

static bool
append_decl(HIRDecl **decls, size_t *count, size_t *capacity, HIRDecl decl)
{
    if (decls == NULL || count == NULL || capacity == NULL)
        return false;
    if (*count == *capacity) {
        size_t next_capacity = *capacity;
        if (!hir_routines_next_capacity(&next_capacity, 16, sizeof(HIRDecl)))
            return false;
        HIRDecl *grown = realloc(*decls, next_capacity * sizeof(HIRDecl));
        if (grown == NULL)
            return false;
        *decls = grown;
        *capacity = next_capacity;
    }
    (*decls)[*count] = decl;
    (*count)++;
    return true;
}

static HIRPhase
hir_phase_for_kind(HIRTopLevelKind kind)
{
    switch (kind) {
        case HIR_TOPLEVEL_EXTERN:
            return HIR_PHASE_EXTERN;
        case HIR_TOPLEVEL_TYPE:
            return HIR_PHASE_TYPE;
        case HIR_TOPLEVEL_ABILITY:
        case HIR_TOPLEVEL_ROLE:
            return HIR_PHASE_CAPABILITY;
        case HIR_TOPLEVEL_PARTY:
        case HIR_TOPLEVEL_SYSTEMIC:
        case HIR_TOPLEVEL_WORLD:
        case HIR_TOPLEVEL_RELATION:
        case HIR_TOPLEVEL_EFFECT:
        case HIR_TOPLEVEL_ZONE:
        case HIR_TOPLEVEL_EVENT:
            return HIR_PHASE_DOMAIN;
        case HIR_TOPLEVEL_FUNCTION:
        case HIR_TOPLEVEL_INTENT:
            return HIR_PHASE_ROUTINE;
        case HIR_TOPLEVEL_EXECUTABLE:
            return HIR_PHASE_EXECUTABLE;
        default:
            return HIR_PHASE_EXECUTABLE;
    }
}

static ASTNode *
hir_routine_body(ASTNode *node)
{
    if (node == NULL)
        return NULL;
    if (node->type == AST_FUNC_DECL)
        return node->data.func_decl.body;
    return NULL;
}

static bool
hir_cfg_successor_in_range(const HIRRoutine *routine, size_t successor)
{
    return routine != NULL && successor < routine->cfg.block_count;
}

static bool
hir_cfg_block_targets(const HIRBasicBlock *block, size_t target)
{
    if (block == NULL)
        return false;
    return (block->has_succ_true && block->succ_true == target)
        || (block->has_succ_false && block->succ_false == target);
}

static bool
hir_cfg_predecessors_contain(const HIRBasicBlock *block, size_t predecessor)
{
    if (block == NULL)
        return false;
    for (size_t i = 0; i < block->predecessor_count; i++) {
        if (block->predecessors[i] == predecessor)
            return true;
    }
    return false;
}

static bool
hir_validate_cfg_shape(const HIRRoutine *routine)
{
    if (routine == NULL || !routine->has_cfg)
        return true;
    if (routine->cfg.blocks == NULL || routine->cfg.block_count == 0)
        return false;
    if (routine->cfg.entry_block >= routine->cfg.block_count)
        return false;

    for (size_t i = 0; i < routine->cfg.block_count; i++) {
        const HIRBasicBlock *block = &routine->cfg.blocks[i];
        if (block->id != i)
            return false;

        switch (block->terminator_kind) {
        case HIR_BLOCK_FALLTHROUGH:
            /* CFG lowering must close every open block before analysis. */
            return false;
        case HIR_BLOCK_GOTO:
            if (!block->has_succ_true || block->has_succ_false)
                return false;
            if (!hir_cfg_successor_in_range(routine, block->succ_true))
                return false;
            break;
        case HIR_BLOCK_BRANCH:
            if (block->terminator_condition == NULL)
                return false;
            if (!block->has_succ_true || !block->has_succ_false)
                return false;
            if (!hir_cfg_successor_in_range(routine, block->succ_true)
                || !hir_cfg_successor_in_range(routine, block->succ_false)) {
                return false;
            }
            break;
        case HIR_BLOCK_RETURN:
        case HIR_BLOCK_UNREACHABLE:
            if (block->has_succ_true || block->has_succ_false)
                return false;
            break;
        default:
            return false;
        }
    }

    return true;
}

static bool
hir_validate_cfg_predecessors(const HIRRoutine *routine)
{
    if (routine == NULL || !routine->has_cfg)
        return true;
    if (!hir_validate_cfg_shape(routine))
        return false;

    for (size_t i = 0; i < routine->cfg.block_count; i++) {
        const HIRBasicBlock *block = &routine->cfg.blocks[i];
        for (size_t p = 0; p < block->predecessor_count; p++) {
            size_t predecessor = block->predecessors[p];
            if (predecessor >= routine->cfg.block_count)
                return false;
            if (!hir_cfg_block_targets(&routine->cfg.blocks[predecessor], i))
                return false;
        }

        if (block->has_succ_true
            && !hir_cfg_predecessors_contain(&routine->cfg.blocks[block->succ_true], i)) {
            return false;
        }
        if (block->has_succ_false
            && !hir_cfg_predecessors_contain(&routine->cfg.blocks[block->succ_false], i)) {
            return false;
        }
    }

    return true;
}

static bool
hir_finish_cfg_routine(HIRRoutine *routine)
{
    if (!hir_validate_cfg_shape(routine))
        return false;
    if (!hir_finalize_cfg(routine))
        return false;
    if (!hir_validate_cfg_predecessors(routine))
        return false;
    if (!hir_compute_cfg_dominance(routine))
        return false;
    if (!hir_compute_cfg_dominance_frontier(routine))
        return false;
    if (!hir_compute_cfg_dom_tree(routine))
        return false;
    if (!hir_compute_cfg_loops(routine))
        return false;
    if (!hir_collect_cfg_local_defs(routine))
        return false;
    if (!hir_compute_cfg_phi_candidates(routine))
        return false;
    if (!hir_materialize_phi_nodes(routine))
        return false;
    hir_finalize_cfg_summary(routine);
    return true;
}

static bool
hir_finish_func_routine(HIRRoutine *routine, ASTNode *func)
{
    if (!hir_lower_func_body_cfg(func->data.func_decl.body, routine))
        return false;
    if (!hir_finish_cfg_routine(routine))
        return false;
    if (!hir_collect_func_signature_refs(func,
                                         &routine->signature_type_refs,
                                         &routine->signature_type_ref_count,
                                         &routine->signature_type_ref_capacity)) {
        return false;
    }
    return hir_collect_direct_calls(func->data.func_decl.body,
                                    &routine->direct_calls,
                                    &routine->direct_call_count,
                                    &routine->direct_call_capacity);
}

static bool
hir_append_routine(HIRProgram *hir, HIRRoutine *routine)
{
    if (hir == NULL || routine == NULL)
        return false;
    if (hir->routine_count == hir->routine_capacity) {
        size_t next_capacity = hir->routine_capacity;
        if (!hir_routines_next_capacity(&next_capacity, 16, sizeof(HIRRoutine)))
            return false;
        HIRRoutine *grown = realloc(hir->routines, next_capacity * sizeof(HIRRoutine));
        if (grown == NULL)
            return false;
        hir->routines = grown;
        hir->routine_capacity = next_capacity;
    }
    hir->routines[hir->routine_count] = *routine;
    hir->routine_count++;
    return true;
}

static void
hir_discard_routine(HIRRoutine *routine)
{
    if (routine == NULL)
        return;
    free((void *)routine->signature_type_refs);
    free((void *)routine->direct_calls);
    pgy_arena_destroy(&routine->scratch);
}

static bool
hir_append_hidden_method_routine(HIRProgram *hir,
                                 size_t decl_id,
                                 const char *owner_name,
                                 ASTNodeType owner_ast_type,
                                 ASTNode *method)
{
    HIRRoutine routine;

    if (hir == NULL || owner_name == NULL || method == NULL || method->type != AST_FUNC_DECL)
        return true;

    memset(&routine, 0, sizeof(routine));
    pgy_arena_init(&routine.scratch, 0);
    routine.decl_id = decl_id;
    routine.kind = HIR_TOPLEVEL_FUNCTION;
    routine.name = method->data.func_decl.name;
    routine.owner_name = owner_name;
    routine.owner_ast_type = owner_ast_type;
    routine.ast = method;
    routine.body = hir_routine_body(method);
    routine.is_hosted = true;
    routine.is_action_like = method->data.func_decl.is_action;
    routine.is_exported = method->is_exported;
    routine.has_control_flow = hir_ast_contains_control_flow(routine.body);

    if (!hir_finish_func_routine(&routine, method))
        goto oom;
    if (!hir_append_routine(hir, &routine))
        goto oom;
    return true;

oom:
    hir_discard_routine(&routine);
    return false;
}

static bool
hir_decl_method_slice(ASTNode *decl,
                      ASTNode ***methods_out,
                      size_t *method_count_out,
                      const char **owner_name_out)
{
    if (methods_out != NULL)
        *methods_out = NULL;
    if (method_count_out != NULL)
        *method_count_out = 0;
    if (owner_name_out != NULL)
        *owner_name_out = NULL;

    if (decl == NULL)
        return false;

    switch (decl->type) {
    case AST_CLASS_DECL:
        if (methods_out != NULL)
            *methods_out = decl->data.class_decl.methods;
        if (method_count_out != NULL)
            *method_count_out = decl->data.class_decl.method_count;
        if (owner_name_out != NULL)
            *owner_name_out = decl->data.class_decl.name;
        return true;
    case AST_ENUM_DECL:
        if (methods_out != NULL)
            *methods_out = decl->data.enum_decl.methods;
        if (method_count_out != NULL)
            *method_count_out = decl->data.enum_decl.method_count;
        if (owner_name_out != NULL)
            *owner_name_out = decl->data.enum_decl.name;
        return true;
    case AST_PARTY_DECL:
        if (methods_out != NULL)
            *methods_out = decl->data.party_decl.methods;
        if (method_count_out != NULL)
            *method_count_out = decl->data.party_decl.method_count;
        if (owner_name_out != NULL)
            *owner_name_out = decl->data.party_decl.name;
        return true;
    case AST_ROSTER_DECL:
        if (methods_out != NULL)
            *methods_out = decl->data.roster_decl.methods;
        if (method_count_out != NULL)
            *method_count_out = decl->data.roster_decl.method_count;
        if (owner_name_out != NULL)
            *owner_name_out = decl->data.roster_decl.name;
        return true;
    case AST_WORLD_DECL:
        if (methods_out != NULL)
            *methods_out = decl->data.world_decl.methods;
        if (method_count_out != NULL)
            *method_count_out = decl->data.world_decl.method_count;
        if (owner_name_out != NULL)
            *owner_name_out = decl->data.world_decl.name;
        return true;
    case AST_RELATION_DECL:
        if (methods_out != NULL)
            *methods_out = decl->data.relation_decl.methods;
        if (method_count_out != NULL)
            *method_count_out = decl->data.relation_decl.method_count;
        if (owner_name_out != NULL)
            *owner_name_out = decl->data.relation_decl.name;
        return true;
    case AST_EFFECT_DECL:
        if (methods_out != NULL)
            *methods_out = decl->data.effect_decl.methods;
        if (method_count_out != NULL)
            *method_count_out = decl->data.effect_decl.method_count;
        if (owner_name_out != NULL)
            *owner_name_out = decl->data.effect_decl.name;
        return true;
    case AST_ZONE_DECL:
        if (methods_out != NULL)
            *methods_out = decl->data.zone_decl.methods;
        if (method_count_out != NULL)
            *method_count_out = decl->data.zone_decl.method_count;
        if (owner_name_out != NULL)
            *owner_name_out = decl->data.zone_decl.name;
        return true;
    default:
        return false;
    }
}

static bool
hir_append_role_impl_method_routines(HIRProgram *hir, size_t decl_id, ASTNode *role_decl)
{
    if (hir == NULL || role_decl == NULL || role_decl->type != AST_ROLE_DECL)
        return true;

    const char *owner_name = role_decl->data.role_decl.name;
    if (owner_name == NULL)
        return true;

    for (size_t i = 0; i < role_decl->data.role_decl.impl_count; i++) {
        ASTNode *impl = role_decl->data.role_decl.impl_abilities[i];
        if (impl == NULL || impl->type != AST_IMPL_ABILITY)
            continue;
        for (size_t j = 0; j < impl->data.impl_ability.method_count; j++) {
            if (!hir_append_hidden_method_routine(hir,
                                                  decl_id,
                                                  owner_name,
                                                  AST_ROLE_DECL,
                                                  impl->data.impl_ability.methods[j])) {
                return false;
            }
        }
    }

    return true;
}

static bool
hir_collect_intent_calls(ASTNode *intent, HIRRoutine *routine)
{
    if (!hir_collect_intent_signature_refs(intent,
                                           &routine->signature_type_refs,
                                           &routine->signature_type_ref_count,
                                           &routine->signature_type_ref_capacity)) {
        return false;
    }
    if (intent->data.intent_decl.priority_expr != NULL
        && !hir_collect_direct_calls(intent->data.intent_decl.priority_expr,
                                     &routine->direct_calls,
                                     &routine->direct_call_count,
                                     &routine->direct_call_capacity)) {
        return false;
    }
    if (intent->data.intent_decl.success_expr != NULL
        && !hir_collect_direct_calls(intent->data.intent_decl.success_expr,
                                     &routine->direct_calls,
                                     &routine->direct_call_count,
                                     &routine->direct_call_capacity)) {
        return false;
    }
    if (intent->data.intent_decl.failure_expr != NULL
        && !hir_collect_direct_calls(intent->data.intent_decl.failure_expr,
                                     &routine->direct_calls,
                                     &routine->direct_call_count,
                                     &routine->direct_call_capacity)) {
        return false;
    }

    for (size_t i = 0; i < intent->data.intent_decl.step_count; i++) {
        ASTNode *step = intent->data.intent_decl.steps[i];
        if (step == NULL || step->type != AST_INTENT_STEP)
            continue;
        if (!hir_collect_direct_calls(step->data.intent_step.using_expr,
                                      &routine->direct_calls,
                                      &routine->direct_call_count,
                                      &routine->direct_call_capacity))
            return false;
        if (!hir_collect_direct_calls(step->data.intent_step.pre_expr,
                                      &routine->direct_calls,
                                      &routine->direct_call_count,
                                      &routine->direct_call_capacity))
            return false;
        if (!hir_collect_direct_calls(step->data.intent_step.guard_expr,
                                      &routine->direct_calls,
                                      &routine->direct_call_count,
                                      &routine->direct_call_capacity))
            return false;
        if (!hir_collect_direct_calls(step->data.intent_step.post_expr,
                                      &routine->direct_calls,
                                      &routine->direct_call_count,
                                      &routine->direct_call_capacity))
            return false;
        if (!hir_collect_direct_calls(step->data.intent_step.invariant_expr,
                                      &routine->direct_calls,
                                      &routine->direct_call_count,
                                      &routine->direct_call_capacity))
            return false;
        if (!hir_collect_direct_calls(step->data.intent_step.expect_expr,
                                      &routine->direct_calls,
                                      &routine->direct_call_count,
                                      &routine->direct_call_capacity))
            return false;
        for (size_t j = 0; j < step->data.intent_step.on_expr_count; j++) {
            if (!hir_collect_direct_calls(step->data.intent_step.on_exprs[j],
                                          &routine->direct_calls,
                                          &routine->direct_call_count,
                                          &routine->direct_call_capacity))
                return false;
        }
        for (size_t j = 0; j < step->data.intent_step.compensate_expr_count; j++) {
            if (!hir_collect_direct_calls(step->data.intent_step.compensate_exprs[j],
                                          &routine->direct_calls,
                                          &routine->direct_call_count,
                                          &routine->direct_call_capacity))
                return false;
        }
    }
    routine->has_control_flow = intent->data.intent_decl.step_count > 1
                                || routine->direct_call_count > 0;
    if (!hir_lower_intent_cfg(intent, routine))
        return false;
    return hir_finish_cfg_routine(routine);
}

bool
hir_append_decl_and_routine(HIRProgram *hir, HIRTopLevelItem item, char **error_message)
{
    HIRDecl decl;
    memset(&decl, 0, sizeof(decl));
    decl.id = hir->decl_count;
    decl.kind = item.kind;
    decl.phase = hir_phase_for_kind(item.kind);
    decl.ast = item.ast;
    decl.name = item.name;
    if (!append_decl(&hir->decls, &hir->decl_count, &hir->decl_capacity, decl))
        goto oom;

    if (item.kind == HIR_TOPLEVEL_FUNCTION
        || item.kind == HIR_TOPLEVEL_INTENT
        || (item.kind == HIR_TOPLEVEL_EXECUTABLE
            && item.ast != NULL
            && item.ast->type == AST_FUNC_DECL)) {
        HIRRoutine routine;
        memset(&routine, 0, sizeof(routine));
        pgy_arena_init(&routine.scratch, 0);
        routine.decl_id = decl.id;
        routine.kind = item.kind;
        routine.name = item.name;
        routine.ast = item.ast;
        routine.body = hir_routine_body(item.ast);
        routine.is_hosted = (item.ast != NULL
                             && item.ast->type == AST_FUNC_DECL
                             && item.ast->data.func_decl.name != NULL
                             && strchr(item.ast->data.func_decl.name, '.') != NULL);
        routine.is_action_like = (item.ast != NULL
                                  && item.ast->type == AST_FUNC_DECL
                                  && item.ast->data.func_decl.is_action);
        routine.is_exported = (item.ast != NULL && item.ast->is_exported);
        routine.has_control_flow = hir_ast_contains_control_flow(routine.body);

        if (item.ast != NULL && item.ast->type == AST_FUNC_DECL) {
            if (!hir_finish_func_routine(&routine, item.ast))
                goto oom_free_routine;
        } else if (item.ast != NULL && item.ast->type == AST_INTENT_DECL) {
            if (!hir_collect_intent_calls(item.ast, &routine))
                goto oom_free_routine;
        }

        if (!hir_append_routine(hir, &routine)) {
oom_free_routine:
            hir_discard_routine(&routine);
            goto oom;
        }
    }

    if (item.ast != NULL) {
        ASTNode **methods = NULL;
        size_t method_count = 0;
        const char *owner_name = NULL;
        if (hir_decl_method_slice(item.ast, &methods, &method_count, &owner_name)) {
            for (size_t i = 0; i < method_count; i++) {
                if (!hir_append_hidden_method_routine(hir,
                                                      decl.id,
                                                      owner_name,
                                                      item.ast->type,
                                                      methods[i])) {
                    goto oom;
                }
            }
        }
        if (!hir_append_role_impl_method_routines(hir, decl.id, item.ast))
            goto oom;
    }

    return true;

oom:
    if (error_message != NULL)
        *error_message = pergyra_strdup("Out of memory");
    return false;
}
