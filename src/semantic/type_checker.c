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
#include "type_checker_ability_ref_internal.h"
#include "type_checker_ownership_internal.h"
#include "type_checker_ownership_diag_internal.h"
#include "type_checker_ownership_consumers_internal.h"
#include "type_checker_channel_transport_internal.h"
#include "type_checker_ownership_support_internal.h"
#include "type_checker_stdlib_use_internal.h"
#include "type_checker_module_contract_internal.h"
#include "type_checker_module_contract_diag_internal.h"
#include "type_checker_ability_fields_internal.h"
#include "type_checker_ability_match_internal.h"
#include "type_checker_ability_where_internal.h"
#include "slot_analyzer.h"

#define INITIAL_DIAG_CAPACITY 16

static bool
callable_contract_is_externally_visible(ASTNode *node, SemanticContext *ctx);
char *
format_generic_subject_signature(const char *name, GenericParams *params);
static const char *
format_generic_subject_signature_scratch(SemanticContext *ctx,
                                         const char *name,
                                         GenericParams *params);
static char *
format_effective_generic_type_list(const char *name, Type **types, size_t count);
/* format_effective_generic_type_list_scratch is declared in type_checker_internal.h
 * (promoted to external linkage for the helpers_late.c TU). */
static char *
semantic_assignment_target_path_impl(ASTNode *expr,
                                     SemanticContext *ctx,
                                     bool scratch);
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
bool
type_resolution_validate_graph(SemanticContext *ctx);
bool
type_resolution_build_topo_order(TypeResolutionGraph *graph,
                                 size_t **out_order,
                                 size_t *out_count);
ASTNode **
collect_effective_generic_arg_nodes(GenericParams *decl_params,
                                    GenericParams *provided_args,
                                    const ASTNode *site,
                                    SemanticContext *ctx,
                                    const char *owner_kind,
                                    const char *owner_name,
                                    size_t *out_count);
char *
semantic_assignment_target_path(ASTNode *expr);
int
find_generic_param_index(GenericParams *gp, const char *param_name);
bool
concrete_type_satisfies_bound(Type *concrete_type, ASTNode *bound_node,
                              SemanticContext *ctx);

/* Helper .inc fragments (tc_strdup_fmt, ownership/qubit helpers, etc).
 * Formerly chained through helpers.inc + helpers_core.inc — wrappers
 * deleted once the helpers_late.c TU went out. */
#include "type_checker_helpers_context.inc"
#include "type_checker_helpers_resolution.inc"
#include "type_checker_helpers_effects.inc"
#include "type_checker_helpers_host.inc"
/* type_checker_visibility.inc was promoted to type_checker_visibility.{h,c}
 * (P1 axis 1).  See docs/92_inc_split_roadmap.md. */

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

#include "type_checker_resolution_graph_core.inc"
#include "type_checker_resolution_graph_inventory.inc"

void
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

#include "type_checker_generic_support.inc"

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

/* type_check_ability_decl body moved to type_checker_ability_decl.c — see docs/101_semantic_split_template.md */

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

static char *
semantic_assignment_target_path_impl(ASTNode *expr,
                                     SemanticContext *ctx,
                                     bool scratch)
{
    char *base = NULL;
    char index_buf[32];

#define PGY_SEM_PATH_DUP(s) \
    (scratch \
        ? pgy_arena_strdup(&ctx->scratch_arena, (s)) \
        : pergyra_strdup((s)))
#define PGY_SEM_PATH_FMT(...) \
    (scratch \
        ? pgy_arena_fmt(&ctx->scratch_arena, __VA_ARGS__) \
        : tc_strdup_fmt(__VA_ARGS__))

    if (expr == NULL)
        return PGY_SEM_PATH_DUP("<target>");

    switch (expr->type) {
    case AST_IDENTIFIER:
        return expr->data.identifier.name != NULL
            ? PGY_SEM_PATH_DUP(expr->data.identifier.name)
            : PGY_SEM_PATH_DUP("<target>");
    case AST_MEMBER_ACCESS:
        if (expr->data.member.name == NULL)
            return PGY_SEM_PATH_DUP("<target>");
        base = semantic_assignment_target_path_impl(expr->data.member.object, ctx, scratch);
        if (base == NULL)
            return PGY_SEM_PATH_FMT("<target>.%s", expr->data.member.name);
        {
            char *result = PGY_SEM_PATH_FMT("%s.%s", base, expr->data.member.name);
            if (!scratch)
                free(base);
            return result != NULL ? result : PGY_SEM_PATH_DUP("<target>");
        }
    case AST_ARRAY_ACCESS:
        base = semantic_assignment_target_path_impl(expr->data.array_access.array, ctx, scratch);
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
            return PGY_SEM_PATH_FMT("<target>[%s]", index_buf);
        {
            char *result = PGY_SEM_PATH_FMT("%s[%s]", base, index_buf);
            if (!scratch)
                free(base);
            return result != NULL ? result : PGY_SEM_PATH_DUP("<target>");
        }
    default:
        return PGY_SEM_PATH_DUP("<target>");
    }

#undef PGY_SEM_PATH_FMT
#undef PGY_SEM_PATH_DUP
}

char *
semantic_assignment_target_path(ASTNode *expr)
{
    return semantic_assignment_target_path_impl(expr, NULL, false);
}

const char *
semantic_assignment_target_path_scratch(ASTNode *expr, SemanticContext *ctx)
{
    if (ctx == NULL)
        return semantic_assignment_target_path(expr);
    return semantic_assignment_target_path_impl(expr, ctx, true);
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
                /* Mangled name is a scratch string: symbol_create_function
                 * duplicates it into the symbol, so the arena allocation
                 * never escapes beyond this block. */
                char *mangled = pgy_arena_fmt(&ctx->scratch_arena,
                    "%s_%s", name, method->data.func_decl.name);
                if (mangled == NULL)
                    continue;
                Symbol *mangled_sym = symbol_create_function(
                    mangled, msym->type, method->line, method->column);
                Scope *enum_scope = ctx->scope;
                ctx->scope = enum_scope->parent;
                if (!scope_declare(ctx->scope, mangled_sym))
                    symbol_destroy(mangled_sym);
                ctx->scope = enum_scope;
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
