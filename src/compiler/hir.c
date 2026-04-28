#include "hir.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "../common/string_compat.h"
#include "../common/arena.h"

#include "hir_analysis.h"
#include "hir_cfg.h"
#include "hir_lower_cfg.h"

static bool
append_ast(ASTNode ***items, size_t *count, ASTNode *node)
{
    ASTNode **grown = realloc(*items, (*count + 1) * sizeof(ASTNode *));
    if (grown == NULL)
        return false;
    grown[*count] = node;
    *items = grown;
    (*count)++;
    return true;
}

static bool
append_item(HIRTopLevelItem **items, size_t *count, HIRTopLevelItem item)
{
    HIRTopLevelItem *grown = realloc(*items, (*count + 1) * sizeof(HIRTopLevelItem));
    if (grown == NULL)
        return false;
    grown[*count] = item;
    *items = grown;
    (*count)++;
    return true;
}

static bool
append_decl(HIRDecl **decls, size_t *count, HIRDecl decl)
{
    HIRDecl *grown = realloc(*decls, (*count + 1) * sizeof(HIRDecl));
    if (grown == NULL)
        return false;
    grown[*count] = decl;
    *decls = grown;
    (*count)++;
    return true;
}

static bool
append_index_unique(size_t **items, size_t *count, size_t value)
{
    for (size_t i = 0; i < *count; i++) {
        if ((*items)[i] == value)
            return true;
    }

    size_t *grown = realloc(*items, (*count + 1) * sizeof(size_t));
    if (grown == NULL)
        return false;
    grown[*count] = value;
    *items = grown;
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
hir_append_hidden_method_routine(HIRProgram *hir,
                                 size_t decl_id,
                                 const char *owner_name,
                                 ASTNodeType owner_ast_type,
                                 ASTNode *method)
{
    HIRRoutine routine;
    HIRRoutine *grown;

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

    if (!hir_lower_func_body_cfg(method->data.func_decl.body, &routine))
        goto oom_free_calls;
    if (!hir_finalize_cfg(&routine))
        goto oom_free_calls;
    if (!hir_compute_cfg_dominance(&routine))
        goto oom_free_calls;
    if (!hir_compute_cfg_dominance_frontier(&routine))
        goto oom_free_calls;
    if (!hir_compute_cfg_dom_tree(&routine))
        goto oom_free_calls;
    if (!hir_compute_cfg_loops(&routine))
        goto oom_free_calls;
    if (!hir_collect_cfg_local_defs(&routine))
        goto oom_free_calls;
    if (!hir_compute_cfg_phi_candidates(&routine))
        goto oom_free_calls;
    if (!hir_materialize_phi_nodes(&routine))
        goto oom_free_calls;
    hir_finalize_cfg_summary(&routine);
    if (!hir_collect_func_signature_refs(method,
                                         &routine.signature_type_refs,
                                         &routine.signature_type_ref_count)) {
        free((void *)routine.signature_type_refs);
        pgy_arena_destroy(&routine.scratch);
        return false;
    }
    if (!hir_collect_direct_calls(method->data.func_decl.body,
                                  &routine.direct_calls,
                                  &routine.direct_call_count))
        goto oom_free_calls;

    grown = realloc(hir->routines, (hir->routine_count + 1) * sizeof(HIRRoutine));
    if (grown == NULL)
        goto oom_free_calls;
    grown[hir->routine_count] = routine;
    hir->routines = grown;
    hir->routine_count++;
    return true;

oom_free_calls:
    free((void *)routine.signature_type_refs);
    free((void *)routine.direct_calls);
    /* Any scratch blocks the dominance/loop pass allocated on the stack
     * routine must be released here ??the routine never reaches the
     * hir->routines[] array so hir_destroy() will not see it. */
    pgy_arena_destroy(&routine.scratch);
    return false;
}

static bool
hir_decl_method_slice(ASTNode *decl, ASTNode ***methods_out, size_t *method_count_out,
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
    const char *owner_name;

    if (hir == NULL || role_decl == NULL || role_decl->type != AST_ROLE_DECL)
        return true;

    owner_name = role_decl->data.role_decl.name;
    if (owner_name == NULL)
        return true;

    for (size_t i = 0; i < role_decl->data.role_decl.impl_count; i++) {
        ASTNode *impl = role_decl->data.role_decl.impl_abilities[i];
        if (impl == NULL || impl->type != AST_IMPL_ABILITY)
            continue;

        for (size_t j = 0; j < impl->data.impl_ability.method_count; j++) {
            ASTNode *method = impl->data.impl_ability.methods[j];
            if (!hir_append_hidden_method_routine(hir,
                                                  decl_id,
                                                  owner_name,
                                                  AST_ROLE_DECL,
                                                  method)) {
                return false;
            }
        }
    }

    return true;
}

static bool
hir_append_decl_and_routine(HIRProgram *hir, HIRTopLevelItem item, char **error_message)
{
    HIRDecl decl;
    memset(&decl, 0, sizeof(decl));
    decl.id = hir->decl_count;
    decl.kind = item.kind;
    decl.phase = hir_phase_for_kind(item.kind);
    decl.ast = item.ast;
    decl.name = item.name;
    if (!append_decl(&hir->decls, &hir->decl_count, decl))
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
            if (!hir_lower_func_body_cfg(item.ast->data.func_decl.body, &routine))
                goto oom_free_calls;
            if (!hir_finalize_cfg(&routine))
                goto oom_free_calls;
            if (!hir_compute_cfg_dominance(&routine))
                goto oom_free_calls;
            if (!hir_compute_cfg_dominance_frontier(&routine))
                goto oom_free_calls;
            if (!hir_compute_cfg_dom_tree(&routine))
                goto oom_free_calls;
            if (!hir_compute_cfg_loops(&routine))
                goto oom_free_calls;
            if (!hir_collect_cfg_local_defs(&routine))
                goto oom_free_calls;
            if (!hir_compute_cfg_phi_candidates(&routine))
                goto oom_free_calls;
            if (!hir_materialize_phi_nodes(&routine))
                goto oom_free_calls;
            hir_finalize_cfg_summary(&routine);
            if (!hir_collect_func_signature_refs(item.ast,
                                                 &routine.signature_type_refs,
                                                 &routine.signature_type_ref_count)) {
                free((void *)routine.signature_type_refs);
                pgy_arena_destroy(&routine.scratch);
                goto oom;
            }
            if (!hir_collect_direct_calls(item.ast->data.func_decl.body,
                                          &routine.direct_calls,
                                          &routine.direct_call_count)) {
                free((void *)routine.signature_type_refs);
                free((void *)routine.direct_calls);
                pgy_arena_destroy(&routine.scratch);
                goto oom;
            }
        } else if (item.ast != NULL && item.ast->type == AST_INTENT_DECL) {
            ASTNode *intent = item.ast;
            if (!hir_collect_intent_signature_refs(intent,
                                                   &routine.signature_type_refs,
                                                   &routine.signature_type_ref_count)) {
                free((void *)routine.signature_type_refs);
                goto oom;
            }
            if (intent->data.intent_decl.priority_expr != NULL
                && !hir_collect_direct_calls(intent->data.intent_decl.priority_expr,
                                             &routine.direct_calls,
                                             &routine.direct_call_count)) {
                free((void *)routine.signature_type_refs);
                free((void *)routine.direct_calls);
                goto oom;
            }
            if (intent->data.intent_decl.success_expr != NULL
                && !hir_collect_direct_calls(intent->data.intent_decl.success_expr,
                                             &routine.direct_calls,
                                             &routine.direct_call_count)) {
                free((void *)routine.signature_type_refs);
                free((void *)routine.direct_calls);
                goto oom;
            }
            if (intent->data.intent_decl.failure_expr != NULL
                && !hir_collect_direct_calls(intent->data.intent_decl.failure_expr,
                                             &routine.direct_calls,
                                             &routine.direct_call_count)) {
                free((void *)routine.signature_type_refs);
                free((void *)routine.direct_calls);
                goto oom;
            }
            for (size_t i = 0; i < intent->data.intent_decl.step_count; i++) {
                ASTNode *step = intent->data.intent_decl.steps[i];
                if (step == NULL || step->type != AST_INTENT_STEP)
                    continue;
                if (!hir_collect_direct_calls(step->data.intent_step.using_expr,
                                              &routine.direct_calls,
                                              &routine.direct_call_count))
                    goto oom_free_calls;
                if (!hir_collect_direct_calls(step->data.intent_step.pre_expr,
                                              &routine.direct_calls,
                                              &routine.direct_call_count))
                    goto oom_free_calls;
                if (!hir_collect_direct_calls(step->data.intent_step.guard_expr,
                                              &routine.direct_calls,
                                              &routine.direct_call_count))
                    goto oom_free_calls;
                if (!hir_collect_direct_calls(step->data.intent_step.post_expr,
                                              &routine.direct_calls,
                                              &routine.direct_call_count))
                    goto oom_free_calls;
                if (!hir_collect_direct_calls(step->data.intent_step.invariant_expr,
                                              &routine.direct_calls,
                                              &routine.direct_call_count))
                    goto oom_free_calls;
                if (!hir_collect_direct_calls(step->data.intent_step.expect_expr,
                                              &routine.direct_calls,
                                              &routine.direct_call_count))
                    goto oom_free_calls;
                for (size_t j = 0; j < step->data.intent_step.on_expr_count; j++) {
                    if (!hir_collect_direct_calls(step->data.intent_step.on_exprs[j],
                                                  &routine.direct_calls,
                                                  &routine.direct_call_count))
                        goto oom_free_calls;
                }
                for (size_t j = 0; j < step->data.intent_step.compensate_expr_count; j++) {
                    if (!hir_collect_direct_calls(step->data.intent_step.compensate_exprs[j],
                                                  &routine.direct_calls,
                                                  &routine.direct_call_count))
                        goto oom_free_calls;
                }
            }
            routine.has_control_flow = intent->data.intent_decl.step_count > 1
                                       || routine.direct_call_count > 0;
        }

        HIRRoutine *grown = realloc(hir->routines,
                                    (hir->routine_count + 1) * sizeof(HIRRoutine));
        if (grown == NULL) {
oom_free_calls:
            free((void *)routine.signature_type_refs);
            free((void *)routine.direct_calls);
            pgy_arena_destroy(&routine.scratch);
            goto oom;
        }
        grown[hir->routine_count] = routine;
        hir->routines = grown;
        hir->routine_count++;
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

static const char *
hir_node_name(ASTNode *node)
{
    if (node == NULL)
        return "(null)";

    switch (node->type) {
        case AST_FUNC_DECL:
            return node->data.func_decl.name;
        case AST_CLASS_DECL:
            return node->data.class_decl.name;
        case AST_TYPE_ALIAS:
            return node->data.type_alias.name;
        case AST_EXTERN_BLOCK:
            return node->data.extern_block.abi;
        case AST_ABILITY_DECL:
            return node->data.ability_decl.name;
        case AST_ROLE_DECL:
            return node->data.role_decl.name;
        case AST_PARTY_DECL:
            return node->data.party_decl.name;
        case AST_ROSTER_DECL:
            return node->data.roster_decl.name;
        case AST_WORLD_DECL:
            return node->data.world_decl.name;
        case AST_INTENT_DECL:
            return node->data.intent_decl.name;
        case AST_RELATION_DECL:
            return node->data.relation_decl.name;
        case AST_EFFECT_DECL:
            return node->data.effect_decl.name;
        case AST_ZONE_DECL:
            return node->data.zone_decl.name;
        case AST_EVENT_DECL:
            return node->data.event_decl.name;
        case AST_LET_DECL:
            return node->data.let_decl.name;
        default:
            return NULL;
    }
}

static ssize_t
hir_find_routine_index_by_name(const HIRProgram *hir, const char *name)
{
    if (hir == NULL || name == NULL)
        return -1;

    for (size_t i = 0; i < hir->routine_count; i++) {
        if (hir->routines[i].name != NULL && strcmp(hir->routines[i].name, name) == 0)
            return (ssize_t)i;
    }

    return -1;
}

const char *
hir_top_level_kind_name(HIRTopLevelKind kind)
{
    switch (kind) {
        case HIR_TOPLEVEL_EXTERN: return "extern";
        case HIR_TOPLEVEL_TYPE: return "type";
        case HIR_TOPLEVEL_ABILITY: return "ability";
        case HIR_TOPLEVEL_ROLE: return "role";
        case HIR_TOPLEVEL_PARTY: return "party";
        case HIR_TOPLEVEL_SYSTEMIC: return "roster";
        case HIR_TOPLEVEL_WORLD: return "world";
        case HIR_TOPLEVEL_RELATION: return "relation";
        case HIR_TOPLEVEL_EFFECT: return "effect";
        case HIR_TOPLEVEL_ZONE: return "zone";
        case HIR_TOPLEVEL_EVENT: return "event";
        case HIR_TOPLEVEL_INTENT: return "intent";
        case HIR_TOPLEVEL_FUNCTION: return "function";
        case HIR_TOPLEVEL_EXECUTABLE: return "executable";
        default: return "unknown";
    }
}

const char *
hir_phase_name(HIRPhase phase)
{
    switch (phase) {
        case HIR_PHASE_EXTERN: return "extern";
        case HIR_PHASE_TYPE: return "type";
        case HIR_PHASE_CAPABILITY: return "capability";
        case HIR_PHASE_DOMAIN: return "domain";
        case HIR_PHASE_ROUTINE: return "routine";
        case HIR_PHASE_EXECUTABLE: return "executable";
        default: return "unknown";
    }
}

static bool
hir_classify_top_level(HIRProgram *hir, ASTNode *node, char **error_message)
{
    HIRTopLevelItem item;
    memset(&item, 0, sizeof(item));
    item.ast = node;
    item.name = hir_node_name(node);

    switch (node->type) {
        case AST_EXTERN_BLOCK:
            item.kind = HIR_TOPLEVEL_EXTERN;
            if (!append_ast(&hir->externs, &hir->extern_count, node))
                goto oom;
            break;
        case AST_CLASS_DECL:
        case AST_TYPE_ALIAS:
        case AST_ENUM_DECL:
            item.kind = HIR_TOPLEVEL_TYPE;
            if (!append_ast(&hir->types, &hir->type_count, node))
                goto oom;
            break;
        case AST_ABILITY_DECL:
            item.kind = HIR_TOPLEVEL_ABILITY;
            if (!append_ast(&hir->abilities, &hir->ability_count, node))
                goto oom;
            break;
        case AST_ROLE_DECL:
            item.kind = HIR_TOPLEVEL_ROLE;
            if (!append_ast(&hir->roles, &hir->role_count, node))
                goto oom;
            break;
        case AST_PARTY_DECL:
            item.kind = HIR_TOPLEVEL_PARTY;
            if (!append_ast(&hir->parties, &hir->party_count, node))
                goto oom;
            break;
        case AST_ROSTER_DECL:
            item.kind = HIR_TOPLEVEL_SYSTEMIC;
            if (!append_ast(&hir->rosters, &hir->roster_count, node))
                goto oom;
            break;
        case AST_WORLD_DECL:
            item.kind = HIR_TOPLEVEL_WORLD;
            if (!append_ast(&hir->worlds, &hir->world_count, node))
                goto oom;
            break;
        case AST_INTENT_DECL:
            item.kind = HIR_TOPLEVEL_INTENT;
            if (!append_ast(&hir->intents, &hir->intent_count, node))
                goto oom;
            break;
        case AST_RELATION_DECL:
            item.kind = HIR_TOPLEVEL_RELATION;
            if (!append_ast(&hir->relations, &hir->relation_count, node))
                goto oom;
            break;
        case AST_EFFECT_DECL:
            item.kind = HIR_TOPLEVEL_EFFECT;
            if (!append_ast(&hir->effects, &hir->effect_count, node))
                goto oom;
            break;
        case AST_ZONE_DECL:
            item.kind = HIR_TOPLEVEL_ZONE;
            if (!append_ast(&hir->zones, &hir->zone_count, node))
                goto oom;
            break;
        case AST_EVENT_DECL:
            item.kind = HIR_TOPLEVEL_EVENT;
            if (!append_ast(&hir->events, &hir->event_count, node))
                goto oom;
            break;
        case AST_FUNC_DECL:
            item.kind = HIR_TOPLEVEL_FUNCTION;
            if (!append_ast(&hir->functions, &hir->function_count, node))
                goto oom;
            if (node->data.func_decl.name != NULL
                && strcmp(node->data.func_decl.name, "Main") == 0) {
                hir->has_main_function = true;
            }
            break;

        case AST_LET_DECL:
        case AST_WITH_STMT:
        case AST_PARALLEL_BLOCK:
        case AST_FOR_LOOP:
        case AST_WHILE_LOOP:
        case AST_IF_STMT:
        case AST_RETURN:
        case AST_BREAK:
        case AST_CONTINUE:
        case AST_SELECT_STMT:
        case AST_MATCH_STMT:
        case AST_BINARY:
        case AST_UNARY:
        case AST_CALL:
        case AST_MEMBER_ACCESS:
        case AST_ARRAY_ACCESS:
        case AST_ASSIGNMENT:
        case AST_AWAIT_EXPR:
        case AST_CHANNEL_SEND:
        case AST_CHANNEL_RECV:
        case AST_NUMBER:
        case AST_STRING:
        case AST_BOOLEAN:
        case AST_IDENTIFIER:
        case AST_ASYNC_BLOCK:
        case AST_SPAWN_EXPR:
        case AST_TASK_GROUP:
        case AST_EVENT_SUBSCRIBE:
        case AST_EVENT_UNSUBSCRIBE:
        case AST_EVENT_INVOKE:
        case AST_LAMBDA_EXPR:
        case AST_BLOCK:
            item.kind = HIR_TOPLEVEL_EXECUTABLE;
            if (!append_ast(&hir->executables, &hir->executable_count, node))
                goto oom;
            break;

        case AST_IMPORT_DECL:
        case AST_USE_DECL:
            /* Already resolved by driver ??skip */
            break;
        case AST_UNSAFE_BLOCK:
        case AST_DEFER_STMT:
        case AST_BIND_STMT:
            item.kind = HIR_TOPLEVEL_EXECUTABLE;
            if (!append_ast(&hir->executables, &hir->executable_count, node))
                goto oom;
            break;

        default:
            if (error_message != NULL) {
                char message[128];
                snprintf(message, sizeof(message),
                         "Unsupported top-level AST node for HIR lowering: %d",
                         (int)node->type);
                *error_message = pergyra_strdup(message);
            }
            return false;
    }

    if (!append_item(&hir->items, &hir->item_count, item))
        goto oom;
    if (!hir_append_decl_and_routine(hir, item, error_message))
        goto oom;

    return true;

oom:
    if (error_message != NULL)
        *error_message = pergyra_strdup("Out of memory");
    return false;
}

static bool
hir_append_synthetic_executable_routine(HIRProgram *hir, char **error_message)
{
    ASTNode *func;
    ASTNode *body;
    HIRTopLevelItem item;

    if (hir == NULL || hir->executable_count == 0 || hir->synthetic_executable_func != NULL)
        return true;

    func = ast_create_function("__pgy_top_level_exec");
    body = ast_create_block();
    if (func == NULL || body == NULL) {
        ast_destroy(func);
        ast_destroy(body);
        if (error_message != NULL)
            *error_message = pergyra_strdup("Out of memory");
        return false;
    }

    for (size_t i = 0; i < hir->executable_count; i++)
        ast_add_statement(body, hir->executables[i]);
    func->data.func_decl.body = body;
    hir->synthetic_executable_func = func;

    memset(&item, 0, sizeof(item));
    item.kind = HIR_TOPLEVEL_EXECUTABLE;
    item.ast = func;
    item.name = func->data.func_decl.name;

    if (!append_item(&hir->items, &hir->item_count, item)) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("Out of memory");
        return false;
    }
    if (!hir_append_decl_and_routine(hir, item, error_message))
        return false;

    return true;
}

static void
hir_destroy_synthetic_executable_func(ASTNode *func)
{
    ASTNode *body;

    if (func == NULL || func->type != AST_FUNC_DECL)
        return;

    body = func->data.func_decl.body;
    free(func->data.func_decl.name);
    if (body != NULL && body->type == AST_BLOCK) {
        free(body->data.block.statements);
        free(body);
    }
    free(func);
}

HIRProgram *
hir_lower(ASTNode *annotated_ast, char **error_message)
{
    if (error_message != NULL)
        *error_message = NULL;

    if (annotated_ast == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("Cannot lower null AST");
        return NULL;
    }
    if (annotated_ast->type != AST_PROGRAM) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("HIR lowering requires AST_PROGRAM root");
        return NULL;
    }

    HIRProgram *hir = calloc(1, sizeof(HIRProgram));
    if (hir == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("Out of memory");
        return NULL;
    }

    for (size_t i = 0; i < annotated_ast->data.program.count; i++) {
        if (!hir_classify_top_level(hir,
                                    annotated_ast->data.program.statements[i],
                                    error_message)) {
            hir_destroy(hir);
            return NULL;
        }
    }

    if (!hir_append_synthetic_executable_routine(hir, error_message)) {
        hir_destroy(hir);
        return NULL;
    }

    for (size_t i = 0; i < hir->routine_count; i++) {
        HIRRoutine *routine = &hir->routines[i];
        for (size_t j = 0; j < routine->direct_call_count; j++) {
            ssize_t callee = hir_find_routine_index_by_name(hir, routine->direct_calls[j]);
            if (callee >= 0
                && !append_index_unique(&routine->callee_routine_ids,
                                        &routine->callee_routine_count,
                                        (size_t)callee)) {
                if (error_message != NULL)
                    *error_message = pergyra_strdup("Out of memory");
                hir_destroy(hir);
                return NULL;
            }
        }
    }

    bool changed = true;
    while (changed) {
        changed = false;
        for (size_t i = 0; i < hir->routine_count; i++) {
            HIRRoutine *routine = &hir->routines[i];
            bool is_root = (routine->kind == HIR_TOPLEVEL_INTENT)
                           || (routine->kind == HIR_TOPLEVEL_EXECUTABLE)
                           || routine->is_exported
                           || (routine->name != NULL
                               && strcmp(routine->name, "Main") == 0);
            if (!routine->is_entry_reachable && is_root) {
                routine->is_entry_reachable = true;
                changed = true;
            }
            if (!routine->is_entry_reachable)
                continue;
            for (size_t j = 0; j < routine->callee_routine_count; j++) {
                size_t callee = routine->callee_routine_ids[j];
                if (callee < hir->routine_count && !hir->routines[callee].is_entry_reachable) {
                    hir->routines[callee].is_entry_reachable = true;
                    changed = true;
                }
            }
        }
    }

    return hir;
}

void
hir_destroy(HIRProgram *hir)
{
    if (hir == NULL)
        return;

    free(hir->items);
    free(hir->decls);
    if (hir->routines != NULL) {
        for (size_t i = 0; i < hir->routine_count; i++) {
            if (hir->routines[i].cfg.blocks != NULL) {
                for (size_t j = 0; j < hir->routines[i].cfg.block_count; j++) {
                    free(hir->routines[i].cfg.blocks[j].statements);
                    free(hir->routines[i].cfg.blocks[j].predecessors);
                    free(hir->routines[i].cfg.blocks[j].dom_tree_children);
                    free((void *)hir->routines[i].cfg.blocks[j].local_defs);
                    free(hir->routines[i].cfg.blocks[j].dominance_frontier);
                    free((void *)hir->routines[i].cfg.blocks[j].phi_candidates);
                    if (hir->routines[i].cfg.blocks[j].phi_nodes != NULL) {
                        for (size_t k = 0; k < hir->routines[i].cfg.blocks[j].phi_node_count; k++)
                            free(hir->routines[i].cfg.blocks[j].phi_nodes[k].incoming_predecessors);
                    }
                    free(hir->routines[i].cfg.blocks[j].phi_nodes);
                }
            }
            free(hir->routines[i].cfg.blocks);
            free((void *)hir->routines[i].signature_type_refs);
            free((void *)hir->routines[i].direct_calls);
            free(hir->routines[i].callee_routine_ids);
            pgy_arena_destroy(&hir->routines[i].scratch);
        }
    }
    free(hir->routines);
    free(hir->externs);
    free(hir->types);
    free(hir->abilities);
    free(hir->roles);
    free(hir->parties);
    free(hir->rosters);
    free(hir->worlds);
    free(hir->relations);
    free(hir->effects);
    free(hir->zones);
    free(hir->subjects);
    free(hir->events);
    free(hir->intents);
    free(hir->functions);
    free(hir->executables);
    hir_destroy_synthetic_executable_func(hir->synthetic_executable_func);
    free(hir);
}
