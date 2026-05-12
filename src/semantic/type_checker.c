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

/* Helper owner headers (tc_strdup_fmt, ownership/qubit helpers, etc).
 * Former wrapper include chains were deleted once the helpers_late.c TU went
 * out. */
#include "type_checker_context_helpers.h"
#include "type_checker_helpers_effects.h"
/* type_checker_visibility was promoted to type_checker_visibility.{h,c}
 * (P1 axis 1).  See docs/92_inc_split_roadmap.md. */

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

#include "type_checker_generic_support.h"

Type *
type_get_constructed_arg(const Type *type, size_t index)
{
    if (type == NULL || type->kind != TYPE_KIND_CONSTRUCTED)
        return TYPE_UNKNOWN;
    if (index >= type->data.constructed.arg_count)
        return TYPE_UNKNOWN;
    return type->data.constructed.args[index];
}

#include "type_checker_expr.h"
#include "type_checker_assignment.h"


bool
type_check_parallel_block(ASTNode *node, SemanticContext *ctx)
{
    return type_check_parallel_block_flow(node, ctx);
}

/* type_check_ability_decl body moved to type_checker_ability_decl.c — see docs/101_semantic_split_template.md */

#include "type_checker_async_channel.h"

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
            (void)domain_resolve_type_ref(
                node->data.type_alias.target_type, ctx);
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
        return type_check_defer_body_flow(node->data.defer_stmt.body, ctx);
    case AST_BIND_STMT:
        /* bind party.slot = Role; — validated at codegen level */
        return true;
    default:
        /* Expression statement */
        type_check_expression(node, ctx);
        return !ctx->has_error;
    }
}
