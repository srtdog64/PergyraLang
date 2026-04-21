/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Type Checker implementation
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include "../common/string_compat.h"
#include "type_checker_internal.h"
#include "type_checker_visibility.h"
#include "diag_payload.h"
#include "diag_codes.h"
#include "type_checker_generic_diag_internal.h"
#include "type_checker_ownership_internal.h"
#include "type_checker_ownership_diag_internal.h"
#include "type_checker_ownership_consumers_internal.h"
#include "type_checker_channel_transport_internal.h"
#include "type_checker_ownership_support_internal.h"
#include "slot_analyzer.h"

#define INITIAL_DIAG_CAPACITY 16

static bool
semantic_is_known_stdlib_use_module(const char *module_name);
static bool
callable_contract_is_externally_visible(ASTNode *node, SemanticContext *ctx);
static size_t
generic_params_required_count(GenericParams *params);
static char *
format_type_constraint_bounds(TypeConstraint *tc);
static char *
format_generic_subject_signature(const char *name, GenericParams *params);
static char *
format_effective_generic_type_list(const char *name, Type **types, size_t count);
static void
semantic_type_resolution_record_named_dependency(SemanticContext *ctx,
                                                 const ASTNode *consumer_site,
                                                 const char *consumer_name,
                                                 TypeResolutionNodeKind provider_kind,
                                                 const ASTNode *provider_site,
                                                 const char *provider_name,
                                                 const char *reason);
static void
semantic_type_resolution_record_type_ref_dependency(SemanticContext *ctx,
                                                    const ASTNode *consumer_site,
                                                    const char *consumer_name,
                                                    const ASTNode *provider_type_ref,
                                                    const char *reason);
static bool
type_resolution_find_path(TypeResolutionGraph *graph,
                          size_t current,
                          size_t goal,
                          bool *visited,
                          size_t *path,
                          size_t *path_len,
                          size_t path_cap);
static bool
type_resolution_find_cycle_visit(TypeResolutionGraph *graph,
                                 size_t current,
                                 unsigned char *color,
                                 size_t *stack,
                                 size_t *stack_len,
                                 size_t *cycle_path,
                                 size_t *cycle_len,
                                 size_t cycle_cap,
                                 size_t *closing_node);
static char *
type_resolution_format_cycle(TypeResolutionGraph *graph,
                             size_t *path,
                             size_t path_len,
                             size_t closing_node);
static bool
type_resolution_validate_graph(SemanticContext *ctx);
static bool
type_resolution_build_topo_order(TypeResolutionGraph *graph,
                                 size_t **out_order,
                                 size_t *out_count);
static void
semantic_type_resolution_precollect_action_contract(ASTNode *method,
                                                    SemanticContext *ctx,
                                                    const char *fallback_name);
static void
semantic_type_resolution_precollect_event_inventory(ASTNode *event_decl,
                                                    SemanticContext *ctx);
static void
semantic_type_resolution_precollect_enum_inventory(ASTNode *enum_decl,
                                                   SemanticContext *ctx);
static void
semantic_type_resolution_precollect_role_inventory(ASTNode *role_decl,
                                                   SemanticContext *ctx);
static ASTNode **
collect_effective_generic_arg_nodes(GenericParams *decl_params,
                                    GenericParams *provided_args,
                                    const ASTNode *site,
                                    SemanticContext *ctx,
                                    const char *owner_kind,
                                    const char *owner_name,
                                    size_t *out_count);
static void
semantic_stage_method_array(ASTNode **methods,
                            size_t method_count,
                            SemanticContext *ctx,
                            const char *fallback_name);
static void
semantic_stage_event_signature(ASTNode *event_decl,
                               SemanticContext *ctx);
static void
semantic_type_resolution_register_local_contract_node(SemanticContext *ctx,
                                                      const ASTNode *site,
                                                      const char *label);
static void
semantic_type_resolution_record_local_contract_dependency(SemanticContext *ctx,
                                                          const ASTNode *consumer_site,
                                                          const char *consumer_label,
                                                          const ASTNode *provider_site,
                                                          const char *provider_label,
                                                          const char *reason);
static char *
semantic_type_resolution_world_zone_slot_label(ASTNode *world_decl,
                                               const char *slot_name);
static char *
semantic_type_resolution_world_state_label(ASTNode *world_decl,
                                           const char *state_name);
static char *
semantic_type_resolution_zone_slot_label(ASTNode *zone_decl,
                                         const char *slot_name);
static char *
semantic_type_resolution_zone_layer_label(ASTNode *zone_decl,
                                          const char *slot_name);
static char *
semantic_type_resolution_zone_state_label(ASTNode *zone_decl,
                                          const char *state_name);
static char *
semantic_type_resolution_projection_path_label(ASTNode *zone_decl,
                                               const char *target_slot_name,
                                               const char *source_slot_name,
                                               const char *target_field_name,
                                               const char *source_field_name);
static char *
semantic_type_resolution_projection_slot_field_label(ASTNode *zone_decl,
                                                     const char *slot_name,
                                                     const char *field_path);
char *
semantic_assignment_target_path(ASTNode *expr);
static ASTNode *
semantic_type_resolution_projection_source_decl(ASTNode *zone_decl,
                                                const char *slot_name,
                                                SemanticContext *ctx);
static ASTNode *
semantic_world_find_zone_slot_local(ASTNode *world, const char *slot_name);
static ASTNode *
semantic_find_top_level_decl_by_label(ASTNode *program,
                                      const char *label,
                                      TypeResolutionNodeKind kind);
static ASTNode *
semantic_find_graph_host_decl(ASTNode *program,
                              const char *label);
static void
semantic_stage_world_local_contract_from_label(ASTNode *world_decl,
                                               const char *label,
                                               SemanticContext *ctx);
static void
semantic_stage_zone_local_contract_from_label(ASTNode *zone_decl,
                                              const char *label,
                                              SemanticContext *ctx);
static int
find_generic_param_index(GenericParams *gp, const char *param_name);
static bool
concrete_type_satisfies_bound(Type *concrete_type, ASTNode *bound_node,
                              SemanticContext *ctx);

/* Local printf-to-heap helper (same as transpiler's strdup_fmt) */
#include "type_checker_helpers.inc"
/* type_checker_visibility.inc was promoted to type_checker_visibility.{h,c}
 * (P1 axis 1).  See docs/92_inc_split_roadmap.md. */
#include "type_checker_module_contracts.inc"

const char *
qubit_state_name(QubitSemanticState state)
{
    switch (state) {
    case QUBIT_STATE_NONE:           return "NONE";
    case QUBIT_STATE_SUPERPOSITION:  return "SUPERPOSITION";
    case QUBIT_STATE_ENTANGLED:      return "ENTANGLED";
    case QUBIT_STATE_COLLAPSED:      return "COLLAPSED";
    case QUBIT_STATE_CLASSICAL:      return "CLASSICAL";
    default:                         return "UNKNOWN";
    }
}

QubitSemanticState
get_qubit_semantic_state(ASTNode *expr, SemanticContext *ctx)
{
    Symbol *sym = lookup_identifier_symbol(expr, ctx);
    if (sym == NULL || !type_is_qubit(sym->type))
        return QUBIT_STATE_NONE;
    return sym->qubit_info.semantic_state;
}

bool
set_qubit_semantic_state(ASTNode *expr, SemanticContext *ctx,
                         QubitSemanticState new_state)
{
    Symbol *sym = lookup_identifier_symbol(expr, ctx);
    if (sym == NULL || !type_is_qubit(sym->type))
        return false;
    sym->qubit_info.semantic_state = new_state;
    return true;
}

/* -----------------------------------------------------------------
 * Compile-time entanglement pool tracking
 * ----------------------------------------------------------------- */

int32_t
alloc_entangle_pool(SemanticContext *ctx)
{
    return ctx->next_entangle_pool++;
}

int32_t
get_qubit_entangle_pool(ASTNode *expr, SemanticContext *ctx)
{
    Symbol *sym = lookup_identifier_symbol(expr, ctx);
    if (sym == NULL || !type_is_qubit(sym->type))
        return -1;
    return sym->qubit_info.entangle_pool_id;
}

void
set_qubit_entangle_pool(ASTNode *expr, SemanticContext *ctx,
                        int32_t pool_id)
{
    Symbol *sym = lookup_identifier_symbol(expr, ctx);
    if (sym == NULL || !type_is_qubit(sym->type))
        return;
    sym->qubit_info.entangle_pool_id = pool_id;
}

void
merge_entangle_pools(SemanticContext *ctx,
                     int32_t dst_pool, int32_t src_pool)
{
    if (dst_pool == src_pool || dst_pool < 0 || src_pool < 0)
        return;
    /* Walk the entire scope chain and re-assign src → dst */
    for (Scope *s = ctx->scope; s != NULL; s = s->parent) {
        for (size_t i = 0; i < s->symbol_count; i++) {
            Symbol *sym = s->symbols[i];
            if (sym != NULL && type_is_qubit(sym->type)
                && sym->qubit_info.entangle_pool_id == src_pool) {
                sym->qubit_info.entangle_pool_id = dst_pool;
            }
        }
    }
}

#include "type_checker_resolution.inc"
static void
semantic_run_type_resolution_worklist(ASTNode *program,
                                      SemanticContext *ctx,
                                      size_t *topo_order,
                                      size_t topo_count)
{
    TypeResolutionGraph *graph;

    if (program == NULL || ctx == NULL || topo_order == NULL)
        return;

    graph = &ctx->type_resolution_graph;
    for (size_t i = topo_count; i > 0; i--) {
        size_t node_index = topo_order[i - 1];
        TypeResolutionNode *node;
        ASTNode *decl;
        ASTNode *host_decl;

        if (node_index >= graph->node_count)
            continue;
        node = &graph->nodes[node_index];
        if (node->kind == TYPE_RES_NODE_DECL || node->kind == TYPE_RES_NODE_ALIAS) {
            decl = semantic_find_top_level_decl_by_label(program,
                                                         node->label,
                                                         node->kind);
            if (decl == NULL)
                continue;

            semantic_stage_top_level_decl(decl, ctx);
            continue;
        }

        if (node->kind == TYPE_RES_NODE_LOCAL_CONTRACT
            || node->kind == TYPE_RES_NODE_PROJECTION_PATH) {
            host_decl = semantic_find_graph_host_decl(program, node->label);
            if (host_decl == NULL)
                continue;
            if (host_decl->type == AST_WORLD_DECL)
                semantic_stage_world_local_contract_from_label(host_decl,
                                                               node->label,
                                                               ctx);
            else if (host_decl->type == AST_ZONE_DECL)
                semantic_stage_zone_local_contract_from_label(host_decl,
                                                              node->label,
                                                              ctx);
        }
    }
}

static void
semantic_type_resolution_record_named_dependency(SemanticContext *ctx,
                                                 const ASTNode *consumer_site,
                                                 const char *consumer_name,
                                                 TypeResolutionNodeKind provider_kind,
                                                 const ASTNode *provider_site,
                                                 const char *provider_name,
                                                 const char *reason)
{
    TypeResolutionGraph *graph;
    size_t from;
    size_t to;
    bool *visited = NULL;
    size_t *path = NULL;
    size_t path_len = 0;

    if (ctx == NULL)
        return;

    graph = &ctx->type_resolution_graph;
    from = type_resolution_intern_node(graph,
                                       TYPE_RES_NODE_TYPE_REF,
                                       consumer_site,
                                       consumer_name != NULL ? consumer_name : "<type-ref>");
    to = type_resolution_intern_node(graph,
                                     provider_kind,
                                     provider_site,
                                     provider_name != NULL ? provider_name : "<provider>");

    if (from != (size_t)-1 && to != (size_t)-1 && graph->node_count > 0) {
        visited = calloc(graph->node_count, sizeof(bool));
        path = calloc(graph->node_count > 0 ? graph->node_count : 1, sizeof(size_t));
        if (visited != NULL && path != NULL) {
            bool has_cycle = (from == to)
                || type_resolution_find_path(graph,
                                             to,
                                             from,
                                             visited,
                                             path,
                                             &path_len,
                                             graph->node_count);
            if (has_cycle && consumer_site != NULL) {
                char *cycle_text = type_resolution_format_cycle(
                    graph,
                    path,
                    path_len,
                    from);
                semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_DEPENDENCY_CYCLE, PGY_CAUSE_TYPE_RESOLUTION_CYCLE, PGY_FIX_BREAK_CYCLE_VIA_INDIRECTION, consumer_site,
                    "Type resolution dependency cycle detected around '%s'.\n"
                    "Reason:\n"
                    "- resolving '%s' would feed back into itself through the current dependency graph\n"
                    "- cycle path: %s\n"
                    "Fix:\n"
                    "- break the alias/default/bound dependency loop so one side becomes concrete first\n"
                    "- or split the contract into acyclic declarations",
                    consumer_name != NULL ? consumer_name : "<type-ref>",
                    consumer_name != NULL ? consumer_name : "<type-ref>",
                    cycle_text != NULL ? cycle_text : "<cycle>");
                free(cycle_text);
            }
        }
        free(visited);
        free(path);
    }

    type_resolution_add_edge(graph, from, to, reason);
}

static char *
format_generic_subject_signature(const char *name, GenericParams *params)
{
    char *result;

    if (name == NULL)
        return tc_strdup_fmt("<generic>");
    if (params == NULL || params->count == 0)
        return tc_strdup_fmt("%s", name);

    result = tc_strdup_fmt("%s<", name);
    if (result == NULL)
        return tc_strdup_fmt("%s", name);

    for (size_t i = 0; i < params->count; i++) {
        GenericParam *gp = params->params[i];
        const char *param_name =
            (gp != NULL && gp->name != NULL) ? gp->name : "<type>";
        char *next = (i + 1 < params->count)
            ? tc_strdup_fmt("%s%s, ", result, param_name)
            : tc_strdup_fmt("%s%s>", result, param_name);
        free(result);
        result = next;
        if (result == NULL)
            return tc_strdup_fmt("%s", name);
    }

    return result;
}

static char *
format_effective_generic_type_list(const char *name, Type **types, size_t count)
{
    char *result;

    if (name == NULL)
        return tc_strdup_fmt("<generic>");
    if (types == NULL || count == 0)
        return tc_strdup_fmt("%s", name);

    result = tc_strdup_fmt("%s<", name);
    if (result == NULL)
        return tc_strdup_fmt("%s", name);

    for (size_t i = 0; i < count; i++) {
        const char *type_name =
            (types[i] != NULL && types[i]->name != NULL)
                ? types[i]->name : "<type>";
        char *next = (i + 1 < count)
            ? tc_strdup_fmt("%s%s, ", result, type_name)
            : tc_strdup_fmt("%s%s>", result, type_name);
        free(result);
        result = next;
        if (result == NULL)
            return tc_strdup_fmt("%s", name);
    }

    return result;
}

bool
identifier_is_borrowed_boundary_param(ASTNode *expr, SemanticContext *ctx)
{
    ASTNode *func_decl;
    const char *ident_name;

    if (expr == NULL || ctx == NULL
        || expr->type != AST_IDENTIFIER
        || expr->data.identifier.name == NULL) {
        return false;
    }

    func_decl = ctx->current_function_decl;
    if (func_decl == NULL || func_decl->type != AST_FUNC_DECL)
        return false;

    ident_name = expr->data.identifier.name;
    for (size_t i = 0; i < func_decl->data.func_decl.param_count; i++) {
        FuncParam *param = func_decl->data.func_decl.params[i];
        Type *param_type;

        if (param == NULL || param->name == NULL || param->type == NULL)
            continue;
        if (param->mode != PARAM_MODE_REF)
            continue;
        if (strcmp(param->name, ident_name) != 0)
            continue;

        param_type = resolve_type_node(param->type, ctx);
        return type_is_general_boundary_type(param_type, ctx);
    }

    return false;
}

static size_t
generic_params_required_count(GenericParams *params)
{
    size_t required = 0;
    if (params == NULL)
        return 0;
    for (size_t i = 0; i < params->count; i++) {
        GenericParam *param = params->params[i];
        if (param != NULL && param->default_type == NULL)
            required++;
    }
    return required;
}

static ASTNode **
collect_effective_generic_arg_nodes(GenericParams *decl_params,
                                    GenericParams *provided_args,
                                    const ASTNode *site,
                                    SemanticContext *ctx,
                                    const char *owner_kind,
                                    const char *owner_name,
                                    size_t *out_count)
{
    size_t decl_count;
    size_t provided_count;
    size_t required_count;
    ASTNode **effective = NULL;

    if (out_count != NULL)
        *out_count = 0;
    if (decl_params == NULL)
        return NULL;

    decl_count = decl_params->count;
    provided_count = provided_args != NULL ? provided_args->count : 0;
    required_count = generic_params_required_count(decl_params);

    if (decl_count == 0) {
        if (provided_count == 0)
            return NULL;
        if (ctx != NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_INFER_GENERIC,
                PGY_CAUSE_GENERIC_ARGS_INVALID, PGY_FIX_ALIGN_GENERIC_ARG_LIST,
                site,
                "%s '%s' does not accept generic type arguments.\n"
                "Reason:\n"
                "- this declaration has no generic parameters\n"
                "- supplied type arguments therefore have nowhere to bind\n"
                "Fix:\n"
                "- remove the generic arguments at the use site\n"
                "- or declare generic parameters on %s '%s'",
                owner_kind != NULL ? owner_kind : "declaration",
                owner_name != NULL ? owner_name : "<anonymous>",
                owner_kind != NULL ? owner_kind : "declaration",
                owner_name != NULL ? owner_name : "<anonymous>");
        }
        return NULL;
    }

    if (provided_count > decl_count) {
        if (ctx != NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_INFER_GENERIC,
                PGY_CAUSE_GENERIC_ARGS_INVALID, PGY_FIX_ALIGN_GENERIC_ARG_LIST,
                site,
                "%s '%s' accepts at most %llu generic argument(s), got %llu.\n"
                "Reason:\n"
                "- more type arguments were supplied than there are generic parameters\n"
                "- effective generic argument derivation cannot match extras safely\n"
                "Fix:\n"
                "- remove the extra generic argument(s)\n"
                "- or add matching generic parameters to %s '%s'",
                owner_kind != NULL ? owner_kind : "declaration",
                owner_name != NULL ? owner_name : "<anonymous>",
                (unsigned long long) decl_count, (unsigned long long) provided_count,
                owner_kind != NULL ? owner_kind : "declaration",
                owner_name != NULL ? owner_name : "<anonymous>");
        }
        return NULL;
    }

    if (provided_count < required_count) {
        if (ctx != NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_INFER_GENERIC,
                PGY_CAUSE_GENERIC_ARGS_INVALID, PGY_FIX_ALIGN_GENERIC_ARG_LIST,
                site,
                "%s '%s' requires at least %llu generic argument(s), got %llu.\n"
                "Reason:\n"
                "- some generic parameters have no default type argument\n"
                "- effective generic argument derivation therefore cannot close the contract\n"
                "Fix:\n"
                "- provide the missing generic argument(s)\n"
                "- or declare trailing default type arguments on %s '%s'",
                owner_kind != NULL ? owner_kind : "declaration",
                owner_name != NULL ? owner_name : "<anonymous>",
                (unsigned long long) required_count, (unsigned long long) provided_count,
                owner_kind != NULL ? owner_kind : "declaration",
                owner_name != NULL ? owner_name : "<anonymous>");
        }
        return NULL;
    }

    effective = calloc(decl_count > 0 ? decl_count : 1, sizeof(ASTNode *));
    if (effective == NULL)
        return NULL;

    for (size_t i = 0; i < decl_count; i++) {
        ASTNode *arg = NULL;
        if (provided_args != NULL && i < provided_args->count) {
            GenericParam *provided = provided_args->params[i];
            arg = provided != NULL ? provided->constraint : NULL;
            if (arg != NULL)
                semantic_type_resolution_record_type_ref_dependency(
                    ctx,
                    site,
                    owner_name != NULL ? owner_name : "<anonymous>",
                    arg,
                    "provided generic argument lookup");
        } else if (decl_params->params[i] != NULL) {
            arg = decl_params->params[i]->default_type;
            if (arg != NULL)
                semantic_type_resolution_record_type_ref_dependency(
                    ctx,
                    site,
                    owner_name != NULL ? owner_name : "<anonymous>",
                    arg,
                    "omitted default generic argument lookup");
        }

        if (arg == NULL) {
            if (ctx != NULL) {
                const char *param_name =
                    decl_params->params[i] != NULL && decl_params->params[i]->name != NULL
                        ? decl_params->params[i]->name
                        : "<type-param>";
                semantic_error_with_hints(ctx, PGY_CODE_SEM_INFER_GENERIC,
                    PGY_CAUSE_GENERIC_ARGS_INVALID, PGY_FIX_ALIGN_GENERIC_ARG_LIST,
                    site,
                    "%s '%s' is missing generic argument for parameter '%s'.\n"
                    "Reason:\n"
                    "- this parameter has no provided argument and no usable default\n"
                    "- effective generic argument derivation stopped at '%s'\n"
                    "Fix:\n"
                    "- provide a type argument for '%s'\n"
                    "- or declare a default type argument for '%s'",
                    owner_kind != NULL ? owner_kind : "declaration",
                    owner_name != NULL ? owner_name : "<anonymous>",
                    param_name,
                    param_name,
                    param_name,
                    param_name);
            }
            free(effective);
            return NULL;
        }

        effective[i] = arg;
    }

    if (out_count != NULL)
        *out_count = decl_count;
    return effective;
}

static bool
subject_type_has_ability(ASTNode *program, const char *type_name,
                         ASTNode *ability_ref);
static ASTNode *
subject_type_find_base_ability_impl(ASTNode *program, const char *type_name,
                                    const char *ability_name);

static int
semantic_find_labeled_loop_depth(SemanticContext *ctx, const char *label)
{
    if (ctx == NULL || label == NULL)
        return -1;

    for (int i = ctx->loop_depth - 1; i >= 0; i--) {
        if (ctx->loop_labels[i] != NULL
            && strcmp(ctx->loop_labels[i], label) == 0) {
            return i;
        }
    }

    return -1;
}

static int
find_generic_param_index(GenericParams *gp, const char *param_name)
{
    if (gp == NULL || param_name == NULL)
        return -1;

    for (size_t i = 0; i < gp->count; i++) {
        if (gp->params[i] != NULL
            && gp->params[i]->name != NULL
            && strcmp(gp->params[i]->name, param_name) == 0) {
            return (int)i;
        }
    }

    return -1;
}

static bool
concrete_type_satisfies_bound(Type *concrete_type, ASTNode *bound_node,
                              SemanticContext *ctx)
{
    Type *bound_type;
    const char *bound_name;
    Symbol *bound_sym;

    if (concrete_type == NULL || bound_node == NULL || ctx == NULL)
        return false;

    bound_type = resolve_type_node(bound_node, ctx);
    if (bound_type != NULL
        && bound_type != TYPE_UNKNOWN
        && type_satisfies_constraint(concrete_type, bound_type)) {
        return true;
    }

    if (ctx->program_root == NULL
        || bound_node->type != AST_TYPE
        || bound_node->data.type.name == NULL
        || concrete_type->name == NULL) {
        return false;
    }

    bound_name = bound_node->data.type.name;
    bound_sym = scope_lookup(ctx->scope, bound_name);
    if ((bound_sym != NULL && bound_sym->kind == SYMBOL_ABILITY)
        || (ctx->program_root != NULL
            && find_ability_decl_by_name(ctx->program_root, bound_name) != NULL)) {
        return subject_type_has_ability(ctx->program_root,
                                        concrete_type->name,
                                        bound_node);
    }

    return false;
}

static void
semantic_report_class_generic_bound_failure(SemanticContext *ctx,
                                            ASTNode *site,
                                            const char *class_name,
                                            const char *param_name,
                                            const char *bound_name,
                                            const char *bounds_text,
                                            const char *expected_text,
                                            const char *actual_text,
                                            const char *concrete_name,
                                            const char *site_label)
{
    semantic_error_with_hints(ctx, PGY_CODE_SEM_CLASS_CONTRACT_INVALID,
        PGY_CAUSE_CLASS_CONTRACT, PGY_FIX_SATISFY_GENERIC_BOUND_OR_WIDEN, site,
        "Type '%s' does not satisfy constraint '%s' for generic parameter '%s' in class '%s'.\n"
        "Reason:\n"
        "- class '%s' requires '%s: %s'\n"
        "- full bound set is '%s: %s'\n"
        "- expected type args are '%s'\n"
        "- actual type args are '%s'\n"
        "- %s type argument is '%s'\n"
        "Fix:\n"
        "- pass/specialize with a type that satisfies '%s'\n"
        "- or relax the class where-clause",
        concrete_name != NULL ? concrete_name : "<type>",
        bound_name != NULL ? bound_name : "<constraint>",
        param_name != NULL ? param_name : "<type-param>",
        class_name != NULL ? class_name : "<class>",
        class_name != NULL ? class_name : "<class>",
        param_name != NULL ? param_name : "<type-param>",
        bound_name != NULL ? bound_name : "<constraint>",
        param_name != NULL ? param_name : "<type-param>",
        bounds_text != NULL ? bounds_text : "<constraint>",
        expected_text != NULL ? expected_text : "<class>",
        actual_text != NULL ? actual_text : "<actual>",
        site_label != NULL ? site_label : "effective",
        concrete_name != NULL ? concrete_name : "<type>",
        bound_name != NULL ? bound_name : "<constraint>");
}

static void
validate_generic_param_default_bounds(GenericParams *gp,
                                      WhereClause *wc,
                                      SemanticContext *ctx,
                                      ASTNode *owner,
                                      const char *owner_kind,
                                      const char *owner_name)
{
    if (gp == NULL || wc == NULL || ctx == NULL)
        return;

    for (size_t ci = 0; ci < wc->count; ci++) {
        TypeConstraint *tc = wc->constraints[ci];
        int param_index;
        GenericParam *param;
        Type *default_type;

        if (tc == NULL || tc->type_param == NULL)
            continue;

        param_index = find_generic_param_index(gp, tc->type_param);
        if (param_index < 0 || (size_t)param_index >= gp->count) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_INFER_GENERIC,
                PGY_CAUSE_GENERIC_ARGS_INVALID, PGY_FIX_ALIGN_GENERIC_ARG_LIST,
                owner != NULL ? owner : (ASTNode *)wc,
                "%s '%s' could not validate default generic bound for unknown parameter '%s'.\n"
                "Reason:\n"
                "- where clause references '%s'\n"
                "- that parameter does not exist in the generic parameter list\n"
                "Fix:\n"
                "- change the where-clause to reference an existing generic parameter\n"
                "- or add generic parameter '%s' to %s '%s'",
                owner_kind != NULL ? owner_kind : "declaration",
                owner_name != NULL ? owner_name : "<anonymous>",
                tc->type_param,
                tc->type_param,
                tc->type_param,
                owner_kind != NULL ? owner_kind : "declaration",
                owner_name != NULL ? owner_name : "<anonymous>");
            continue;
        }

        param = gp->params[param_index];
        if (param == NULL || param->default_type == NULL)
            continue;

        semantic_type_resolution_record_type_ref_dependency(
            ctx,
            owner != NULL ? owner : param->default_type,
            tc->type_param != NULL ? tc->type_param : "<type-param>",
            param->default_type,
            "default-bound subject lookup");

        default_type = resolve_type_node(param->default_type, ctx);
        if (default_type == NULL || default_type == TYPE_UNKNOWN) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_INFER_GENERIC,
                PGY_CAUSE_GENERIC_ARGS_INVALID, PGY_FIX_ALIGN_GENERIC_ARG_LIST,
                owner != NULL ? owner : param->default_type,
                "Default generic type argument for parameter '%s' in %s '%s' could not be resolved.\n"
                "Reason:\n"
                "- default type argument participates in effective generic argument derivation\n"
                "- where-clause validation cannot proceed until that default materializes\n"
                "Fix:\n"
                "- provide a resolvable default type argument for '%s'\n"
                "- or remove the default and require the caller to pass the type argument explicitly",
                tc->type_param,
                owner_kind != NULL ? owner_kind : "declaration",
                owner_name != NULL ? owner_name : "<anonymous>",
                tc->type_param);
            continue;
        }

        for (size_t bi = 0; bi < tc->bound_count; bi++) {
            ASTNode *bound_node = tc->bounds[bi];
            char *bounds_text = format_type_constraint_bounds(tc);
            const char *bound_name =
                (bound_node != NULL
                 && bound_node->type == AST_TYPE
                 && bound_node->data.type.name != NULL)
                    ? bound_node->data.type.name
                    : "<constraint>";

            if (bound_node != NULL) {
                semantic_type_resolution_record_type_ref_dependency(
                    ctx,
                    owner != NULL ? owner : bound_node,
                    tc->type_param != NULL ? tc->type_param : "<type-param>",
                    bound_node,
                    "default-bound constraint lookup");
            }

            if (concrete_type_satisfies_bound(default_type, bound_node, ctx)) {
                free(bounds_text);
                continue;
            }

            semantic_error_with_hints(ctx, PGY_CODE_SEM_INFER_GENERIC,
                PGY_CAUSE_GENERIC_ARGS_INVALID, PGY_FIX_ALIGN_GENERIC_ARG_LIST,
                owner != NULL ? owner : param->default_type,
                "Default generic type argument '%s' does not satisfy constraint '%s' for parameter '%s' in %s '%s'.\n"
                "Reason:\n"
                "- %s '%s' declares '%s = %s'\n"
                "- where clause requires '%s: %s'\n"
                "- full bound set is '%s: %s'\n"
                "Fix:\n"
                "- change the default type argument to satisfy '%s'\n"
                "- or relax the where-clause on %s '%s'",
                default_type->name != NULL ? default_type->name : "<type>",
                bound_name,
                tc->type_param,
                owner_kind != NULL ? owner_kind : "declaration",
                owner_name != NULL ? owner_name : "<anonymous>",
                owner_kind != NULL ? owner_kind : "declaration",
                owner_name != NULL ? owner_name : "<anonymous>",
                tc->type_param,
                default_type->name != NULL ? default_type->name : "<type>",
                tc->type_param,
                bound_name,
                tc->type_param,
                bounds_text != NULL ? bounds_text : "<constraint>",
                bound_name,
                owner_kind != NULL ? owner_kind : "declaration",
                owner_name != NULL ? owner_name : "<anonymous>");
            free(bounds_text);
        }
    }
}

static void
validate_class_where_clause_instantiation(ASTNode *class_decl,
                                          Type *constructed_type,
                                          ASTNode *site,
                                          SemanticContext *ctx)
{
    WhereClause *wc;
    GenericParams *gp;
    char *expected_text = NULL;

    if (class_decl == NULL || class_decl->type != AST_CLASS_DECL
        || constructed_type == NULL
        || constructed_type->kind != TYPE_KIND_CONSTRUCTED
        || site == NULL || ctx == NULL) {
        return;
    }

    gp = class_decl->data.class_decl.generic_params;
    wc = class_decl->data.class_decl.where_clause;
    if (gp == NULL || gp->count == 0 || wc == NULL || wc->count == 0)
        return;
    expected_text = format_generic_subject_signature(
        class_decl->data.class_decl.name != NULL
            ? class_decl->data.class_decl.name : "<class>",
        gp);

    for (size_t ci = 0; ci < wc->count; ci++) {
        TypeConstraint *tc = wc->constraints[ci];
        int param_index;
        Type *concrete_type;

        if (tc == NULL || tc->type_param == NULL)
            continue;

        param_index = find_generic_param_index(gp, tc->type_param);
        if (param_index < 0
            || (size_t)param_index >= constructed_type->data.constructed.arg_count) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_CLASS_CONTRACT_INVALID, PGY_CAUSE_CLASS_CONTRACT, PGY_FIX_SATISFY_GENERIC_BOUND_OR_WIDEN, site,
                "Class '%s' could not validate where-clause parameter '%s' during instantiation.\n"
                "Reason:\n"
                "- instantiated type '%s' does not provide an effective type argument for '%s'\n"
                "- class where-clause validation cannot continue until every effective type argument resolves\n"
                "Fix:\n"
                "- pass/supply a type argument for '%s'\n"
                "- or fix the class generic parameter list/default arguments so '%s' is materialized",
                class_decl->data.class_decl.name != NULL
                    ? class_decl->data.class_decl.name : "<class>",
                tc->type_param,
                constructed_type->name != NULL ? constructed_type->name : "<constructed>",
                tc->type_param,
                tc->type_param,
                tc->type_param);
            continue;
        }

        concrete_type = constructed_type->data.constructed.args[param_index];
        if (concrete_type == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_CLASS_CONTRACT_INVALID, PGY_CAUSE_CLASS_CONTRACT, PGY_FIX_SATISFY_GENERIC_BOUND_OR_WIDEN, site,
                "Class '%s' could not resolve instantiated type argument for '%s'.\n"
                "Reason:\n"
                "- where-clause validation reached instantiation with no concrete type for '%s'\n"
                "- class specialization cannot be validated until every effective type argument resolves\n"
                "Fix:\n"
                "- pass a concrete type argument for '%s'\n"
                "- or fix the default type argument / imported type so it resolves",
                class_decl->data.class_decl.name != NULL
                    ? class_decl->data.class_decl.name : "<class>",
                tc->type_param,
                tc->type_param,
                tc->type_param);
            continue;
        }

        for (size_t bi = 0; bi < tc->bound_count; bi++) {
            ASTNode *bound_node = tc->bounds[bi];
            char *bounds_text = format_type_constraint_bounds(tc);
            const char *bound_name =
                (bound_node != NULL
                 && bound_node->type == AST_TYPE
                 && bound_node->data.type.name != NULL)
                    ? bound_node->data.type.name
                    : "<constraint>";

            if (bound_node != NULL) {
                semantic_type_resolution_record_type_ref_dependency(
                    ctx,
                    site,
                    class_decl->data.class_decl.name != NULL
                        ? class_decl->data.class_decl.name : "<class>",
                    bound_node,
                    "class instantiation where-bound lookup");
            }

            if (!concrete_type_satisfies_bound(concrete_type, bound_node, ctx)) {
                semantic_report_class_generic_bound_failure(
                    ctx,
                    site,
                    class_decl->data.class_decl.name != NULL
                        ? class_decl->data.class_decl.name : "<class>",
                    tc->type_param,
                    bound_name,
                    bounds_text,
                    expected_text,
                    constructed_type->name != NULL
                        ? constructed_type->name : "<constructed>",
                    concrete_type->name != NULL
                        ? concrete_type->name : "<type>",
                    "instantiated");
            }
            free(bounds_text);
        }
    }
    free(expected_text);
}

static void
validate_class_where_clause_specialization_ast(ASTNode *class_decl,
                                               ASTNode *specialized_type,
                                               ASTNode *site,
                                               SemanticContext *ctx)
{
    GenericParams *decl_params;
    WhereClause *wc;
    ASTNode **effective_args = NULL;
    size_t effective_count = 0;
    char *expected_text = NULL;

    if (class_decl == NULL || class_decl->type != AST_CLASS_DECL
        || specialized_type == NULL || specialized_type->type != AST_TYPE
        || site == NULL || ctx == NULL) {
        return;
    }

    decl_params = class_decl->data.class_decl.generic_params;
    wc = class_decl->data.class_decl.where_clause;
    if (decl_params == NULL || decl_params->count == 0
        || wc == NULL || wc->count == 0) {
        return;
    }
    expected_text = format_generic_subject_signature(
        class_decl->data.class_decl.name != NULL
            ? class_decl->data.class_decl.name : "<class>",
        decl_params);

    effective_args = collect_effective_generic_arg_nodes(
        decl_params,
        specialized_type->data.type.generic_args,
        specialized_type,
        ctx,
        "class",
        class_decl->data.class_decl.name != NULL
            ? class_decl->data.class_decl.name : "<class>",
        &effective_count);
    if (effective_args == NULL)
        return;

    for (size_t i = 0; i < effective_count; i++) {
        if (effective_args[i] != NULL) {
            semantic_type_resolution_record_type_ref_dependency(
                ctx,
                specialized_type,
                class_decl->data.class_decl.name != NULL
                    ? class_decl->data.class_decl.name : "<class>",
                effective_args[i],
                "class specialization effective argument lookup");
        }
    }

    for (size_t ci = 0; ci < wc->count; ci++) {
        TypeConstraint *tc = wc->constraints[ci];
        int param_index;
        Type *concrete_type;

        if (tc == NULL || tc->type_param == NULL)
            continue;

        param_index = find_generic_param_index(decl_params, tc->type_param);
        if (param_index < 0 || (size_t)param_index >= effective_count) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_CLASS_CONTRACT_INVALID, PGY_CAUSE_CLASS_CONTRACT, PGY_FIX_SATISFY_GENERIC_BOUND_OR_WIDEN, site,
                "Class '%s' could not validate where-clause parameter '%s' during specialization.\n"
                "Reason:\n"
                "- specialized type syntax did not materialize an effective type argument for '%s'\n"
                "- class where-clause validation cannot continue until every effective type argument resolves\n"
                "Fix:\n"
                "- provide/supply a type argument for '%s'\n"
                "- or fix the class generic parameter list/default arguments so '%s' is materialized",
                class_decl->data.class_decl.name != NULL
                    ? class_decl->data.class_decl.name : "<class>",
                tc->type_param,
                tc->type_param,
                tc->type_param,
                tc->type_param);
            continue;
        }

        concrete_type = resolve_type_node(effective_args[param_index], ctx);
        if (concrete_type == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_CLASS_CONTRACT_INVALID, PGY_CAUSE_CLASS_CONTRACT, PGY_FIX_SATISFY_GENERIC_BOUND_OR_WIDEN, site,
                "Class '%s' could not resolve specialized type argument for '%s'.\n"
                "Reason:\n"
                "- where-clause validation reached specialization with no concrete type for '%s'\n"
                "- specialization cannot be validated until every effective type argument resolves\n"
                "Fix:\n"
                "- pass a concrete type argument for '%s'\n"
                "- or fix the default type argument / imported type so it resolves",
                class_decl->data.class_decl.name != NULL
                    ? class_decl->data.class_decl.name : "<class>",
                tc->type_param,
                tc->type_param,
                tc->type_param);
            continue;
        }

        for (size_t bi = 0; bi < tc->bound_count; bi++) {
            ASTNode *bound_node = tc->bounds[bi];
            char *bounds_text = format_type_constraint_bounds(tc);
            const char *bound_name =
                (bound_node != NULL
                 && bound_node->type == AST_TYPE
                 && bound_node->data.type.name != NULL)
                    ? bound_node->data.type.name
                    : "<constraint>";

            if (bound_node != NULL) {
                semantic_type_resolution_record_type_ref_dependency(
                    ctx,
                    site,
                    class_decl->data.class_decl.name != NULL
                        ? class_decl->data.class_decl.name : "<class>",
                    bound_node,
                    "class specialization where-bound lookup");
            }

            if (!concrete_type_satisfies_bound(concrete_type, bound_node, ctx)) {
                semantic_report_class_generic_bound_failure(
                    ctx,
                    site,
                    class_decl->data.class_decl.name != NULL
                        ? class_decl->data.class_decl.name : "<class>",
                    tc->type_param,
                    bound_name,
                    bounds_text,
                    expected_text,
                    specialized_type->data.type.name != NULL
                        ? specialized_type->data.type.name : "<specialized>",
                    concrete_type->name != NULL
                        ? concrete_type->name : "<type>",
                    "specialized");
            }
            free(bounds_text);
        }
    }

    free(expected_text);
    free(effective_args);
}

void
propagate_collapse_to_pool(SemanticContext *ctx, int32_t pool_id)
{
    if (pool_id < 0)
        return;
    /* Walk the entire scope chain and collapse all members of this pool */
    for (Scope *s = ctx->scope; s != NULL; s = s->parent) {
        for (size_t i = 0; i < s->symbol_count; i++) {
            Symbol *sym = s->symbols[i];
            if (sym != NULL && type_is_qubit(sym->type)
                && sym->qubit_info.entangle_pool_id == pool_id
                && sym->qubit_info.semantic_state != QUBIT_STATE_COLLAPSED
                && sym->qubit_info.semantic_state != QUBIT_STATE_CLASSICAL) {
                sym->qubit_info.semantic_state = QUBIT_STATE_COLLAPSED;
            }
        }
    }
}

Type *
type_check_qubit_use(ASTNode *expr, SemanticContext *ctx)
{
    if (expr != NULL && expr->type == AST_IDENTIFIER) {
        Symbol *sym = lookup_identifier_symbol(expr, ctx);
        if (sym == NULL) {
            if (name_looks_qualified(expr->data.identifier.name)) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_UNDEFINED_SYMBOL, PGY_CAUSE_SYMBOL_UNDEFINED, PGY_FIX_IMPORT_OR_DECLARE_SYMBOL, expr,
                    "Undefined symbol '%s' (check namespace spelling or export visibility)",
                    expr->data.identifier.name);
            } else {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_UNDEFINED_SYMBOL, PGY_CAUSE_SYMBOL_UNDEFINED, PGY_FIX_IMPORT_OR_DECLARE_SYMBOL, expr,
                    "Undefined symbol '%s'",
                    expr->data.identifier.name);
            }
            return TYPE_UNKNOWN;
        }
        if (!type_is_qubit(sym->type)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
                PGY_CAUSE_TYPE_MOVABLE_HANDLE_REQUIRED, PGY_FIX_PROVIDE_MOVABLE_HANDLE,
                expr,
                "Expected a slot handle (movable) (currently QubitSlot), got '%s'.\n"
                "Reason:\n"
                "- this consumer path expects a move-only resource value\n"
                "- value '%s' has type '%s', which is not part of the current movable-resource subset\n"
                "Fix:\n"
                "- pass a QubitSlot value instead\n"
                "- or keep this value on the non-movable path",
                sym->type->name,
                expr->data.identifier.name != NULL ? expr->data.identifier.name : "<value>",
                sym->type->name);
            return TYPE_UNKNOWN;
        }
        if (sym->is_consumed) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_MOVE_FROM_RELEASED,
                PGY_CAUSE_MOVE_FROM_RELEASED, PGY_FIX_RECLAIM_OR_TRACE_EARLIER_MOVE,
                expr,
                "%s '%s' was moved or released and cannot be used again.\n"
                "Reason:\n"
                "- value '%s' was already consumed by an ownership transfer or release path\n"
                "- move-only values cannot be reused after consumption\n"
                "Fix:\n"
                "- create/acquire a fresh %s value\n"
                "- or keep ownership in one binding and avoid the earlier move",
                resource_handle_display_name(sym->type),
                expr->data.identifier.name,
                expr->data.identifier.name != NULL ? expr->data.identifier.name : "<value>",
                resource_handle_display_name(sym->type));
            return TYPE_UNKNOWN;
        }
        sym->is_used = true;
        return sym->type;
    }

    if (expr_is_movable_resource_boundary(expr)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_MOVE_TOKEN_MISUSE, PGY_CAUSE_MOVE_TOKEN_DIRECT_ACCESS, PGY_FIX_MATERIALIZE_TOKEN_TO_SLOT, expr,
            "Movable resources from recv/await must first be bound to a named variable before use.\n"
            "Reason:\n"
            "- transfer boundaries create a fresh move-only resource value\n"
            "- the ownership checker needs a stable binding to track later moves and releases\n"
            "Fix:\n"
            "- assign the recv/await result to a local variable first\n"
            "- then pass or consume that named binding");
        return TYPE_UNKNOWN;
    }

    return type_check_expression(expr, ctx);
}

Type *
type_get_constructed_arg(const Type *type, size_t index)
{
    if (type == NULL || type->kind != TYPE_KIND_CONSTRUCTED)
        return TYPE_UNKNOWN;
    if (index >= type->data.constructed.arg_count)
        return TYPE_UNKNOWN;
    return type->data.constructed.args[index];
}

#include "type_checker_expr.inc"
#include "type_checker_ownership_boundaries.inc"
#include "type_checker_ownership_destructure.inc"
#include "type_checker_ownership_destructure_stmt.inc"
#include "type_checker_ownership_param_summary.inc"


bool
type_check_parallel_block(ASTNode *node, SemanticContext *ctx)
{
    bool prev_parallel = ctx->in_parallel;
    ctx->in_parallel   = true;

    for (size_t i = 0; i < node->data.parallel.task_count; i++) {
        scope_enter(&ctx->scope, SCOPE_BLOCK);
        type_check_statement(node->data.parallel.tasks[i], ctx);
        scope_exit(&ctx->scope);
    }

    ctx->in_parallel = prev_parallel;
    return !ctx->has_error;
}

static bool
semantic_is_known_stdlib_use_module(const char *module_name)
{
    static const char *const modules[] = {
        "datetime",
        "device_adapter",
        "http",
        "ledger",
        "money",
        "obligation",
        "page",
        "spray",
        "storage",
        "timer",
        "versioning"
    };

    if (module_name == NULL)
        return false;

    for (size_t i = 0; i < sizeof(modules) / sizeof(modules[0]); i++) {
        if (strcmp(module_name, modules[i]) == 0)
            return true;
    }

    return false;
}

static bool
callable_contract_is_externally_visible(ASTNode *node, SemanticContext *ctx)
{
    ASTNode *host = current_host_decl(ctx);

    if (node == NULL || ctx == NULL || node->type != AST_FUNC_DECL)
        return false;
    if (node->is_exported)
        return true;
    if (host == NULL || !host->is_exported)
        return false;
    if (!node->data.func_decl.has_explicit_access)
        return true;
    return node->data.func_decl.access == ACCESS_PUBLIC
        || node->data.func_decl.access == ACCESS_PROTECTED;
}

bool
type_check_ability_decl(ASTNode *node, SemanticContext *ctx)
{
    const char *name = node->data.ability_decl.name;
    bool has_generics = (node->data.ability_decl.generic_params != NULL
                         && node->data.ability_decl.generic_params->count > 0);

    /* Register ability as a symbol so roles can reference it */
    Symbol *sym = calloc(1, sizeof(Symbol));
    sym->name = pergyra_strdup(name);
    sym->kind = SYMBOL_ABILITY;
    sym->type = TYPE_VOID; /* Abilities don't have a concrete type */
    sym->decl_line = node->line;
    sym->decl_col = node->column;

    Symbol *existing = scope_lookup_current(ctx->scope, name);
    if (existing != NULL) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_REDECLARATION,
            PGY_CAUSE_ABILITY_DUPLICATE_NAME,
            PGY_FIX_RENAME_OR_REMOVE_DUPLICATE,
            node, "Redeclaration of ability '%s'", name);
        symbol_destroy(sym);
        return false;
    }
    scope_declare(ctx->scope, sym);

    if (has_generics) {
        validate_generic_param_defaults(node->data.ability_decl.generic_params,
            ctx, node, "ability");
        scope_enter(&ctx->scope, SCOPE_BLOCK);
        GenericParams *gp = node->data.ability_decl.generic_params;
        for (size_t gi = 0; gi < gp->count; gi++) {
            if (gp->params[gi] == NULL || gp->params[gi]->name == NULL)
                continue;
            Type *tp = calloc(1, sizeof(Type));
            if (tp != NULL) {
                tp->kind = TYPE_KIND_CLASS;
                tp->name = pergyra_strdup(gp->params[gi]->name);
            }
            Symbol *s = symbol_create_variable(
                gp->params[gi]->name,
                tp != NULL ? tp : TYPE_UNKNOWN,
                node->line, node->column);
            s->kind = SYMBOL_CLASS;
            scope_declare(ctx->scope, s);
        }
    }

    validate_where_clause_bounds(node->data.ability_decl.where_clause, ctx, node);
    validate_generic_param_default_bounds(
        node->data.ability_decl.generic_params,
        node->data.ability_decl.where_clause,
        ctx,
        node,
        "ability",
        name);
    validate_ability_require_fields(node, ctx);

    /* Check method signatures */
    scope_enter(&ctx->scope, SCOPE_BLOCK);
    for (size_t i = 0; i < node->data.ability_decl.method_count; i++) {
        ASTNode *method = node->data.ability_decl.methods[i];
        /* Only type-check methods that have a body */
        if (method->data.func_decl.body != NULL) {
            type_check_func_decl(method, ctx);
        } else {
            /* Abstract method — just validate the signature types */
            if (method->data.func_decl.return_type != NULL)
                resolve_type_node(method->data.func_decl.return_type, ctx);
            for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
                if (method->data.func_decl.params[j]->type != NULL)
                    resolve_type_node(method->data.func_decl.params[j]->type, ctx);
            }
        }
    }
    scope_exit(&ctx->scope);
    if (has_generics)
        scope_exit(&ctx->scope);

    return !ctx->has_error;
}

#include "type_checker_decls.inc"

#include "type_checker_async_channel.inc"

bool
type_check_event_decl(ASTNode *node, SemanticContext *ctx)
{
    bool ok = true;

    if (node == NULL || ctx == NULL || node->type != AST_EVENT_DECL)
        return false;

    for (size_t i = 0; i < node->data.event_decl.param_count; i++) {
        ASTNode *param = node->data.event_decl.params[i];
        if (param == NULL)
            continue;

        if (param->type != AST_LET_DECL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_EVENT_CONTRACT_INVALID, PGY_CAUSE_EVENT_SIGNATURE, PGY_FIX_ALIGN_EVENT_SIGNATURE, param,
                "Event '%s' parameter %llu must be a typed binding",
                node->data.event_decl.name != NULL
                    ? node->data.event_decl.name : "<event>",
                (unsigned long long) (i + 1));
            ok = false;
            continue;
        }

        if (param->data.let_decl.type == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_EVENT_CONTRACT_INVALID, PGY_CAUSE_EVENT_SIGNATURE, PGY_FIX_ALIGN_EVENT_SIGNATURE, param,
                "Event '%s' parameter '%s' requires an explicit type",
                node->data.event_decl.name != NULL
                    ? node->data.event_decl.name : "<event>",
                param->data.let_decl.name != NULL
                    ? param->data.let_decl.name : "<param>");
            ok = false;
            continue;
        }

        if (resolve_type_node(param->data.let_decl.type, ctx) == NULL)
            ok = false;
    }

    if (node->data.event_decl.return_type != NULL) {
        Type *return_type = resolve_type_node(node->data.event_decl.return_type, ctx);
        if (return_type != NULL && !type_equals(return_type, TYPE_VOID)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_EVENT_CONTRACT_INVALID, PGY_CAUSE_EVENT_SIGNATURE, PGY_FIX_ALIGN_EVENT_SIGNATURE, node->data.event_decl.return_type,
                "Event '%s' must return Void, got '%s'",
                node->data.event_decl.name != NULL
                    ? node->data.event_decl.name : "<event>",
                return_type->name != NULL ? return_type->name : "<type>");
            ok = false;
        }
    }

    return ok && !ctx->has_error;
}

static const char *
semantic_event_expr_name(ASTNode *expr)
{
    if (expr == NULL)
        return "<event>";
    if (expr->type == AST_IDENTIFIER && expr->data.identifier.name != NULL)
        return expr->data.identifier.name;
    if (expr->type == AST_MEMBER_ACCESS && expr->data.member.name != NULL)
        return expr->data.member.name;
    return "<event>";
}

char *
semantic_assignment_target_path(ASTNode *expr)
{
    char *base = NULL;
    char index_buf[32];

    if (expr == NULL)
        return pergyra_strdup("<target>");

    switch (expr->type) {
    case AST_IDENTIFIER:
        return expr->data.identifier.name != NULL
            ? pergyra_strdup(expr->data.identifier.name)
            : pergyra_strdup("<target>");
    case AST_MEMBER_ACCESS:
        if (expr->data.member.name == NULL)
            return pergyra_strdup("<target>");
        base = semantic_assignment_target_path(expr->data.member.object);
        if (base == NULL)
            return tc_strdup_fmt("<target>.%s", expr->data.member.name);
        {
            char *result = tc_strdup_fmt("%s.%s", base, expr->data.member.name);
            free(base);
            return result != NULL ? result : pergyra_strdup("<target>");
        }
    case AST_ARRAY_ACCESS:
        base = semantic_assignment_target_path(expr->data.array_access.array);
        if (expr->data.array_access.index != NULL
            && expr->data.array_access.index->type == AST_NUMBER) {
            snprintf(index_buf, sizeof(index_buf), "%g",
                expr->data.array_access.index->data.number.value);
        } else if (expr->data.array_access.index != NULL
                   && expr->data.array_access.index->type == AST_IDENTIFIER
                   && expr->data.array_access.index->data.identifier.name != NULL) {
            snprintf(index_buf, sizeof(index_buf), "%s",
                expr->data.array_access.index->data.identifier.name);
        } else {
            snprintf(index_buf, sizeof(index_buf), "?");
        }
        if (base == NULL)
            return tc_strdup_fmt("<target>[%s]", index_buf);
        {
            char *result = tc_strdup_fmt("%s[%s]", base, index_buf);
            free(base);
            return result != NULL ? result : pergyra_strdup("<target>");
        }
    default:
        return pergyra_strdup("<target>");
    }
}

const char *
semantic_borrowed_boundary_root_name(ASTNode *expr, SemanticContext *ctx)
{
    if (expr == NULL || ctx == NULL)
        return NULL;

    switch (expr->type) {
    case AST_IDENTIFIER:
        return identifier_is_borrowed_boundary_param(expr, ctx)
            ? expr->data.identifier.name
            : NULL;
    case AST_MEMBER_ACCESS:
        return semantic_borrowed_boundary_root_name(
            expr->data.member.object, ctx);
    case AST_ARRAY_ACCESS:
        return semantic_borrowed_boundary_root_name(
            expr->data.array_access.array, ctx);
    default:
        return NULL;
    }
}

static Type *
semantic_event_handler_signature(ASTNode *handler, SemanticContext *ctx)
{
    if (handler == NULL || ctx == NULL)
        return NULL;

    if (handler->type == AST_LAMBDA_EXPR) {
        size_t param_count = handler->data.lambda_expr.param_count;
        Type **param_types = calloc(param_count > 0 ? param_count : 1, sizeof(Type *));
        Type *return_type = TYPE_VOID;
        Type *lambda_type;

        if (param_types == NULL)
            return TYPE_UNKNOWN;

        for (size_t i = 0; i < param_count; i++) {
            ASTNode *param = handler->data.lambda_expr.params[i];
            if (param != NULL
                && param->type == AST_LET_DECL
                && param->data.let_decl.type != NULL) {
                param_types[i] = resolve_type_node(param->data.let_decl.type, ctx);
            } else {
                param_types[i] = TYPE_UNKNOWN;
            }
        }

        if (handler->data.lambda_expr.return_type != NULL) {
            Type *resolved = resolve_type_node(handler->data.lambda_expr.return_type, ctx);
            if (resolved != NULL)
                return_type = resolved;
        }

        lambda_type = type_create_function(param_types, param_count, return_type);
        free(param_types);
        return lambda_type != NULL ? lambda_type : TYPE_UNKNOWN;
    }

    return type_check_expression(handler, ctx);
}

static bool
type_check_event_subscription(ASTNode *node, SemanticContext *ctx,
                              const char *op_name)
{
    Type *event_type;
    Type *handler_type;
    const char *event_name;
    bool ok = true;

    if (node == NULL || ctx == NULL)
        return false;

    event_type = type_check_expression(node->data.event_op.event, ctx);
    handler_type = semantic_event_handler_signature(node->data.event_op.handler, ctx);
    event_name = semantic_event_expr_name(node->data.event_op.event);

    if (event_type == NULL || event_type->kind != TYPE_KIND_FUNCTION) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_EVENT_CONTRACT_INVALID,
            PGY_CAUSE_EVENT_SIGNATURE, PGY_FIX_ALIGN_EVENT_SIGNATURE,
            node->data.event_op.event,
            "Event %s target '%s' must be an event-compatible callable",
            op_name != NULL ? op_name : "operation",
            event_name);
        return false;
    }

    if (event_type->data.function.return_type != NULL
        && !type_equals(event_type->data.function.return_type, TYPE_VOID)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_EVENT_CONTRACT_INVALID, PGY_CAUSE_EVENT_SIGNATURE, PGY_FIX_ALIGN_EVENT_SIGNATURE, node->data.event_op.event,
            "Event '%s' must return Void to support %s",
            event_name, op_name != NULL ? op_name : "subscription");
        ok = false;
    }

    if (handler_type == NULL || handler_type->kind != TYPE_KIND_FUNCTION) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_EVENT_CONTRACT_INVALID,
            PGY_CAUSE_EVENT_SIGNATURE, PGY_FIX_ALIGN_EVENT_SIGNATURE,
            node->data.event_op.handler,
            "Event %s handler for '%s' must be a function or typed lambda",
            op_name != NULL ? op_name : "operation",
            event_name);
        return false;
    }

    if (handler_type->data.function.return_type != NULL
        && !type_equals(handler_type->data.function.return_type, TYPE_VOID)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_EVENT_CONTRACT_INVALID,
            PGY_CAUSE_EVENT_SIGNATURE, PGY_FIX_ALIGN_EVENT_SIGNATURE,
            node->data.event_op.handler,
            "Event %s handler for '%s' must return Void, got '%s'",
            op_name != NULL ? op_name : "operation",
            event_name,
            handler_type->data.function.return_type->name != NULL
                ? handler_type->data.function.return_type->name : "<type>");
        ok = false;
    }

    if (event_type->data.function.param_count != handler_type->data.function.param_count) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_EVENT_CONTRACT_INVALID,
            PGY_CAUSE_EVENT_SIGNATURE, PGY_FIX_ALIGN_EVENT_SIGNATURE,
            node->data.event_op.handler,
            "Event %s handler for '%s' has parameter count mismatch: expected %llu, got %llu",
            op_name != NULL ? op_name : "operation",
            event_name,
            (unsigned long long) event_type->data.function.param_count,
            (unsigned long long) handler_type->data.function.param_count);
        return false;
    }

    for (size_t i = 0; i < event_type->data.function.param_count; i++) {
        Type *expected = event_type->data.function.param_types[i];
        Type *actual = handler_type->data.function.param_types[i];

        if (expected == NULL || actual == NULL
            || expected == TYPE_UNKNOWN || actual == TYPE_UNKNOWN) {
            continue;
        }

        if (!type_equals(expected, actual)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_EVENT_CONTRACT_INVALID,
                PGY_CAUSE_EVENT_SIGNATURE, PGY_FIX_ALIGN_EVENT_SIGNATURE,
                node->data.event_op.handler,
                "Event %s handler for '%s' parameter %llu mismatch: expected '%s', got '%s'",
                op_name != NULL ? op_name : "operation",
                event_name,
                (unsigned long long) (i + 1),
                expected->name != NULL ? expected->name : "<type>",
                actual->name != NULL ? actual->name : "<type>");
            ok = false;
        }
    }

    return ok && !ctx->has_error;
}

static bool
type_check_event_invoke_stmt(ASTNode *node, SemanticContext *ctx)
{
    Type *event_type;
    const char *event_name;
    bool ok = true;

    if (node == NULL || ctx == NULL || node->type != AST_EVENT_INVOKE)
        return false;

    event_type = type_check_expression(node->data.event_invoke.event, ctx);
    event_name = semantic_event_expr_name(node->data.event_invoke.event);

    if (event_type == NULL || event_type->kind != TYPE_KIND_FUNCTION) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_EVENT_CONTRACT_INVALID,
            PGY_CAUSE_EVENT_SIGNATURE, PGY_FIX_ALIGN_EVENT_SIGNATURE,
            node->data.event_invoke.event,
            "Event invoke target '%s' must be an event-compatible callable",
            event_name);
        return false;
    }

    if (event_type->data.function.param_count != node->data.event_invoke.arg_count) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_EVENT_CONTRACT_INVALID, PGY_CAUSE_EVENT_SIGNATURE, PGY_FIX_ALIGN_EVENT_SIGNATURE, node,
            "Event '%s' invoke argument count mismatch: expected %llu, got %llu",
            event_name,
            (unsigned long long) event_type->data.function.param_count,
            (unsigned long long) node->data.event_invoke.arg_count);
        return false;
    }

    for (size_t i = 0; i < node->data.event_invoke.arg_count; i++) {
        Type *expected = event_type->data.function.param_types[i];
        Type *actual = type_check_expression(node->data.event_invoke.arguments[i], ctx);

        if (expected == NULL || actual == NULL
            || expected == TYPE_UNKNOWN || actual == TYPE_UNKNOWN) {
            continue;
        }

        if (!type_is_assignable(actual, expected)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_EVENT_CONTRACT_INVALID, PGY_CAUSE_EVENT_SIGNATURE, PGY_FIX_ALIGN_EVENT_SIGNATURE, node->data.event_invoke.arguments[i],
                "Event '%s' invoke argument %llu mismatch: expected '%s', got '%s'",
                event_name,
                (unsigned long long) (i + 1),
                expected->name != NULL ? expected->name : "<type>",
                actual->name != NULL ? actual->name : "<type>");
            ok = false;
        }
    }

    return ok && !ctx->has_error;
}

bool
type_check_statement(ASTNode *node, SemanticContext *ctx)
{
    if (node == NULL)
        return true;

    switch (node->type) {
    case AST_LET_DECL:
        return type_check_let_decl(node, ctx);
    case AST_LET_DESTRUCTURE:
        return type_check_let_destructure_stmt(node, ctx);
    case AST_FUNC_DECL:
        return type_check_func_decl(node, ctx);
    case AST_EVENT_DECL:
        return type_check_event_decl(node, ctx);
    case AST_TYPE_ALIAS:
        if (node->data.type_alias.target_type != NULL)
            (void)resolve_type_node(node->data.type_alias.target_type, ctx);
        return !ctx->has_error;
    case AST_CLASS_DECL:
        return type_check_class_decl(node, ctx);
    case AST_EXTERN_BLOCK:
        return type_check_extern_block(node, ctx);
    case AST_IF_STMT:
        return type_check_if_stmt(node, ctx);
    case AST_FOR_LOOP:
        return type_check_for_loop(node, ctx);
    case AST_WHILE_LOOP:
        return type_check_while_loop(node, ctx);
    case AST_MATCH_STMT:
        return type_check_match_stmt(node, ctx);
    case AST_RETURN:
        return type_check_return_stmt(node, ctx);
    case AST_BREAK:
        if (ctx->loop_depth <= 0) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_LOOP_CONTROL_INVALID, PGY_CAUSE_LOOP_CONTROL, PGY_FIX_MOVE_INTO_LOOP_OR_FIX_LABEL, node, "'break' used outside of loop");
            return false;
        }
        if (node->data.break_stmt.label != NULL
            && semantic_find_labeled_loop_depth(ctx,
                node->data.break_stmt.label) < 0) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_LOOP_CONTROL_INVALID, PGY_CAUSE_LOOP_CONTROL, PGY_FIX_MOVE_INTO_LOOP_OR_FIX_LABEL, node,
                "Unknown loop label '%s' in break",
                node->data.break_stmt.label);
            return false;
        }
        return true;
    case AST_CONTINUE:
        if (ctx->loop_depth <= 0) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_LOOP_CONTROL_INVALID, PGY_CAUSE_LOOP_CONTROL, PGY_FIX_MOVE_INTO_LOOP_OR_FIX_LABEL, node, "'continue' used outside of loop");
            return false;
        }
        if (node->data.continue_stmt.label != NULL
            && semantic_find_labeled_loop_depth(ctx,
                node->data.continue_stmt.label) < 0) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_LOOP_CONTROL_INVALID, PGY_CAUSE_LOOP_CONTROL, PGY_FIX_MOVE_INTO_LOOP_OR_FIX_LABEL, node,
                "Unknown loop label '%s' in continue",
                node->data.continue_stmt.label);
            return false;
        }
        return true;
    case AST_ENUM_DECL:
        {
            const char *name = node->data.enum_decl.name;
            ASTNode *saved_nominal = ctx->current_nominal_decl;

            scope_enter(&ctx->scope, SCOPE_CLASS);
            ctx->current_nominal_decl = node;

            for (size_t i = 0; i < node->data.enum_decl.method_count; i++)
                type_check_func_decl(node->data.enum_decl.methods[i], ctx);

            for (size_t i = 0; i < node->data.enum_decl.method_count; i++) {
                ASTNode *method = node->data.enum_decl.methods[i];
                if (method == NULL || method->type != AST_FUNC_DECL
                    || method->data.func_decl.name == NULL || name == NULL)
                    continue;
                Symbol *msym = scope_lookup_current(ctx->scope, method->data.func_decl.name);
                if (msym == NULL || msym->kind != SYMBOL_FUNCTION)
                    continue;
                size_t len = strlen(name) + 1 + strlen(method->data.func_decl.name) + 1;
                char *mangled = malloc(len);
                if (mangled == NULL)
                    continue;
                snprintf(mangled, len, "%s_%s", name, method->data.func_decl.name);
                Symbol *mangled_sym = symbol_create_function(
                    mangled, msym->type, method->line, method->column);
                Scope *enum_scope = ctx->scope;
                ctx->scope = enum_scope->parent;
                if (!scope_declare(ctx->scope, mangled_sym))
                    symbol_destroy(mangled_sym);
                ctx->scope = enum_scope;
                free(mangled);
            }

            scope_exit(&ctx->scope);
            ctx->current_nominal_decl = saved_nominal;
            return !ctx->has_error;
        }
    case AST_WITH_STMT:
        return type_check_with_stmt(node, ctx);
    case AST_PARALLEL_BLOCK:
        return type_check_parallel_block(node, ctx);
    case AST_ABILITY_DECL:
        return type_check_ability_decl(node, ctx);
    case AST_ROLE_DECL:
        return type_check_role_decl(node, ctx);
    case AST_PARTY_DECL:
        return type_check_party_decl(node, ctx);
    case AST_ROSTER_DECL:
        return type_check_roster_decl(node, ctx);
    case AST_WORLD_DECL:
        return type_check_world_decl(node, ctx);
    case AST_INTENT_DECL:
        return type_check_intent_decl(node, ctx);
    case AST_RELATION_DECL:
        return type_check_relation_decl(node, ctx);
    case AST_EFFECT_DECL:
        return type_check_effect_decl(node, ctx);
    case AST_ZONE_DECL:
        return type_check_zone_decl(node, ctx);
    case AST_ASYNC_BLOCK:
        return type_check_async_block(node, ctx);
    case AST_SELECT_STMT:
        return type_check_select_stmt(node, ctx);
    case AST_EVENT_SUBSCRIBE:
        return type_check_event_subscription(node, ctx, "subscription");
    case AST_EVENT_UNSUBSCRIBE:
        return type_check_event_subscription(node, ctx, "unsubscription");
    case AST_EVENT_INVOKE:
        return type_check_event_invoke_stmt(node, ctx);
    case AST_BLOCK:
        return type_check_block(node, ctx);
    case AST_IMPORT_DECL:
        /* Already resolved by driver — skip */
        return true;
    case AST_USE_DECL:
        validate_stdlib_use_decl(node, ctx);
        return !ctx->has_error;
    case AST_UNSAFE_BLOCK:
        /* Type-check body normally; safety constraints relaxed at codegen */
        if (node->data.unsafe_block.body != NULL)
            type_check_block(node->data.unsafe_block.body, ctx);
        return !ctx->has_error;
    case AST_DEFER_STMT:
        /* Type-check deferred body — actual slot state save/restore
         * is handled in type_check_statement_flow (type_checker_flow.c). */
        if (node->data.defer_stmt.body != NULL)
            type_check_block(node->data.defer_stmt.body, ctx);
        return !ctx->has_error;
    case AST_BIND_STMT:
        /* bind party.slot = Role; — validated at codegen level */
        return true;
    default:
        /* Expression statement */
        type_check_expression(node, ctx);
        return !ctx->has_error;
    }
}

#include "type_checker_program.inc"
