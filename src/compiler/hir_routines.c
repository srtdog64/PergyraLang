#include "hir_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "../common/string_compat.h"

#include "hir_analysis.h"
#include "hir_lower_cfg.h"

static bool
hir_routines_next_capacity(size_t *capacity, size_t initial, size_t elem_size)
{
    if (capacity == NULL || initial == 0 || elem_size == 0)
        return false;

    size_t current = *capacity;
    size_t next_capacity = initial;
    if (current != 0) {
        if (current > SIZE_MAX / 2)
            return false;
        next_capacity = current * 2;
    }
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
        return ast_func_body(node);
    return NULL;
}

static bool
hir_finish_func_routine(HIRRoutine *routine, ASTNode *func)
{
    if (!hir_lower_func_body_cfg(ast_func_body(func), routine))
        return false;
    if (!hir_finish_cfg_routine(routine))
        return false;
    if (!hir_collect_func_signature_refs(func,
                                         &routine->signature_type_refs,
                                         &routine->signature_type_ref_count,
                                         &routine->signature_type_ref_capacity)) {
        return false;
    }
    return hir_collect_direct_calls(ast_func_body(func), routine);
}

static bool
hir_append_routine(HIRProgram *hir, HIRRoutine *routine)
{
    if (hir == NULL || routine == NULL)
        return false;
    if (hir->routine_count >= UINT32_MAX)
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
    routine->routine_id = (uint32_t)(hir->routine_count + 1);
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
    free(routine->direct_call_decl_ids);
    for (size_t i = 0; i < routine->resource_flow_symbol_count; i++)
        free((void *)routine->resource_flow_symbols[i].name);
    free(routine->resource_flow_symbols);
    free(routine->loop_flow_summaries);
    free(routine->loop_flow_states);
    pgy_arena_set_last_consumer(&routine->scratch, "hir-routine-analysis");
    pgy_arena_set_release_point(&routine->scratch, "hir-routine-discard");
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
    pgy_arena_init_named(&routine.scratch, 0, "hir-hidden-method-scratch");
    routine.decl_id = decl_id;
    routine.source_syntax_id = ast_node_stable_id(method);
    routine.kind = HIR_TOPLEVEL_FUNCTION;
    routine.name = ast_declaration_name(method);
    routine.owner_name = owner_name;
    routine.owner_ast_type = owner_ast_type;
    routine.parameter_count = ast_func_param_count(method);
    routine.ast = method;
    routine.body = hir_routine_body(method);
    routine.is_hosted = true;
    routine.is_action_like = ast_func_is_action(method);
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

static ASTNode **
hir_domain_methods(ASTNode *decl, size_t *method_count_out)
{
    if (method_count_out != NULL)
        *method_count_out = 0;
    if (decl == NULL)
        return NULL;
    switch (decl->type) {
    case AST_WORLD_DECL:
        return ast_world_methods(decl, method_count_out);
    case AST_RELATION_DECL:
        return ast_relation_methods(decl, method_count_out);
    case AST_EFFECT_DECL:
        return ast_effect_methods(decl, method_count_out);
    case AST_ZONE_DECL:
        return ast_zone_methods(decl, method_count_out);
    default:
        return NULL;
    }
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
            *methods_out = ast_class_methods(decl, method_count_out);
        if (method_count_out != NULL)
            (void) ast_class_methods(decl, method_count_out);
        if (owner_name_out != NULL)
            *owner_name_out = ast_class_name(decl);
        return true;
    case AST_ENUM_DECL:
        if (methods_out != NULL)
            *methods_out = ast_enum_methods(decl, method_count_out);
        if (method_count_out != NULL)
            (void) ast_enum_methods(decl, method_count_out);
        if (owner_name_out != NULL)
            *owner_name_out = ast_enum_name(decl);
        return true;
    case AST_PARTY_DECL:
        if (methods_out != NULL)
            *methods_out = ast_party_methods(decl, NULL);
        if (method_count_out != NULL)
            *method_count_out = ast_party_method_count(decl);
        if (owner_name_out != NULL)
            *owner_name_out = ast_party_name(decl);
        return true;
    case AST_ROSTER_DECL:
        if (methods_out != NULL)
            *methods_out = ast_roster_methods(decl, NULL);
        if (method_count_out != NULL)
            *method_count_out = ast_roster_method_count(decl);
        if (owner_name_out != NULL)
            *owner_name_out = ast_roster_name(decl);
        return true;
    case AST_WORLD_DECL:
        if (methods_out != NULL)
            *methods_out = hir_domain_methods(decl, method_count_out);
        else if (method_count_out != NULL)
            (void) hir_domain_methods(decl, method_count_out);
        if (owner_name_out != NULL)
            *owner_name_out = ast_world_name(decl);
        return true;
    case AST_RELATION_DECL:
        if (methods_out != NULL)
            *methods_out = hir_domain_methods(decl, method_count_out);
        else if (method_count_out != NULL)
            (void) hir_domain_methods(decl, method_count_out);
        if (owner_name_out != NULL)
            *owner_name_out = ast_relation_name(decl);
        return true;
    case AST_EFFECT_DECL:
        if (methods_out != NULL)
            *methods_out = hir_domain_methods(decl, method_count_out);
        else if (method_count_out != NULL)
            (void) hir_domain_methods(decl, method_count_out);
        if (owner_name_out != NULL)
            *owner_name_out = ast_effect_name(decl);
        return true;
    case AST_ZONE_DECL:
        if (methods_out != NULL)
            *methods_out = hir_domain_methods(decl, method_count_out);
        else if (method_count_out != NULL)
            (void) hir_domain_methods(decl, method_count_out);
        if (owner_name_out != NULL)
            *owner_name_out = ast_zone_name(decl);
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

    const char *owner_name = ast_role_name(role_decl);
    if (owner_name == NULL)
        return true;

    for (size_t i = 0; i < ast_role_impl_count(role_decl); i++) {
        ASTNode *impl = ast_role_impl(role_decl, i);
        if (impl == NULL)
            continue;
        if (impl->type == AST_OVERRIDE_FUNC) {
            if (!hir_append_hidden_method_routine(
                    hir, decl_id, owner_name, AST_ROLE_DECL,
                    ast_override_func_decl(impl)))
                return false;
            continue;
        }
        if (impl->type != AST_IMPL_ABILITY)
            continue;
        for (size_t j = 0; j < ast_impl_ability_method_count(impl); j++) {
            if (!hir_append_hidden_method_routine(hir,
                                                  decl_id,
                                                  owner_name,
                                                  AST_ROLE_DECL,
                                                  ast_impl_ability_method(impl, j))) {
                return false;
            }
        }
    }

    return true;
}

static bool
hir_collect_intent_calls(ASTNode *intent, HIRRoutine *routine)
{
    ASTNode *priority_expr = ast_intent_decl_priority_expr(intent);
    ASTNode *success_expr = ast_intent_decl_success_expr(intent);
    ASTNode *failure_expr = ast_intent_decl_failure_expr(intent);
    ASTNode **steps;
    size_t step_count;

    if (!hir_collect_intent_signature_refs(intent,
                                           &routine->signature_type_refs,
                                           &routine->signature_type_ref_count,
                                           &routine->signature_type_ref_capacity)) {
        return false;
    }
    steps = ast_intent_decl_steps(intent, &step_count);
    if (priority_expr != NULL
        && !hir_collect_direct_calls(priority_expr, routine)) {
        return false;
    }
    if (success_expr != NULL
        && !hir_collect_direct_calls(success_expr, routine)) {
        return false;
    }
    if (failure_expr != NULL
        && !hir_collect_direct_calls(failure_expr, routine)) {
        return false;
    }

    for (size_t i = 0; i < step_count; i++) {
        ASTNode *step = steps[i];
        if (step == NULL || step->type != AST_INTENT_STEP)
            continue;
        if (!hir_collect_direct_calls(ast_intent_step_using_expr(step), routine))
            return false;
        if (!hir_collect_direct_calls(ast_intent_step_pre_expr(step), routine))
            return false;
        if (!hir_collect_direct_calls(ast_intent_step_guard_expr(step), routine))
            return false;
        if (!hir_collect_direct_calls(ast_intent_step_post_expr(step), routine))
            return false;
        if (!hir_collect_direct_calls(ast_intent_step_invariant_expr(step), routine))
            return false;
        if (!hir_collect_direct_calls(ast_intent_step_expect_expr(step), routine))
            return false;
        for (size_t j = 0; j < ast_intent_step_on_expr_count(step); j++) {
            if (!hir_collect_direct_calls(ast_intent_step_on_exprs(step, NULL)[j],
                                          routine))
                return false;
        }
        for (size_t j = 0; j < ast_intent_step_compensate_expr_count(step); j++) {
            if (!hir_collect_direct_calls(ast_intent_step_compensate_exprs(step, NULL)[j],
                                          routine))
                return false;
        }
    }
    routine->has_control_flow = step_count > 1
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
    decl.source_syntax_id = ast_node_stable_id(item.ast);
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
        pgy_arena_init_named(&routine.scratch, 0, "hir-routine-scratch");
        routine.decl_id = decl.id;
        routine.source_syntax_id = ast_node_stable_id(item.ast);
        routine.kind = item.kind;
        routine.name = item.name;
        routine.parameter_count = item.ast != NULL
            && item.ast->type == AST_FUNC_DECL
            ? ast_func_param_count(item.ast) : 0;
        routine.ast = item.ast;
        routine.body = hir_routine_body(item.ast);
        routine.is_hosted = (item.ast != NULL
                             && item.ast->type == AST_FUNC_DECL
                             && ast_declaration_name(item.ast) != NULL
                             && strchr(ast_declaration_name(item.ast), '.') != NULL);
        routine.is_action_like = ast_func_is_action(item.ast);
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
