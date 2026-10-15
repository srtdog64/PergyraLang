#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "type_checker_internal.h"
#include "string_window_extent.h"
#include "type_checker_visibility.h"
#include "indexed_string_borrow_owner.h"
#include "diag_codes.h"
#include "../common/match_variant_policy.h"
#include "function_param_publication_owner.h"

/* PGY_DEBUG_SEMANTIC_TIMING visit census: total expression visits vs
 * distinct nodes separates "each node costs too much" from "nodes are
 * re-walked" (docs/183 methodology). Per-kind time is INCLUSIVE (a parent
 * accumulates its children), so leaf kinds (identifier/number/string) read
 * as true self-time. clock() is 1ms-grained on MinGW -- too coarse for
 * ~us visits -- hence QPC on Windows. */
#ifdef _WIN32
#include <windows.h>
#endif

typedef struct
{
    size_t visits_total;
    size_t visits_by_kind[512];
    double seconds_by_kind[512];
} ExprVisitCensus;

static ExprVisitCensus *
expr_visit_census(void)
{
    static _Thread_local ExprVisitCensus census;
    return &census;
}

static double
expr_census_now(void)
{
#ifdef _WIN32
    LARGE_INTEGER freq;
    LARGE_INTEGER counter;

    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)freq.QuadPart;
#else
    return (double)clock() / (double)CLOCKS_PER_SEC;
#endif
}

static bool
expr_visit_census_enabled(void)
{
    static _Thread_local int enabled = -1;

    if (enabled < 0)
        enabled = getenv("PGY_DEBUG_SEMANTIC_TIMING") != NULL ? 1 : 0;
    return enabled == 1;
}

void
type_check_expr_debug_visit_report(void)
{
    ExprVisitCensus *census = expr_visit_census();

    if (!expr_visit_census_enabled() || census->visits_total == 0)
        return;
    fprintf(stderr, "[semantic timing] expr visits total=%zu\n",
            census->visits_total);
    for (size_t i = 0; i < 512; i++) {
        if (census->visits_by_kind[i] > 5000
            || census->seconds_by_kind[i] > 0.2) {
            fprintf(stderr,
                    "[semantic timing]   kind=%zu visits=%zu inclusive=%.3fs\n",
                    i, census->visits_by_kind[i], census->seconds_by_kind[i]);
        }
    }
}
static Type *
expr_normalize_type(Type *type)
{
    return type != NULL ? type : TYPE_UNKNOWN;
}
static Type *
type_check_expression_dispatch(ASTNode *expr, SemanticContext *ctx)
{
    switch (expr->type) {
    case AST_NUMBER:
        /* Duration first: it rides the is_long lane for codegen, but
         * its TYPE stays distinct so bare numbers cannot pose as time
         * (docs/181 SS2.3). */
        if (ast_number_is_duration(expr))
            return TYPE_DURATION;
        if (ast_number_is_long(expr))
            return TYPE_LONG;
        if (ast_number_is_float(expr))
            return TYPE_FLOAT;
        return ast_number_value(expr) == (int64_t)ast_number_value(expr)
            ? TYPE_INT
            : TYPE_FLOAT;
    case AST_STRING:
        return TYPE_STRING;
    case AST_BOOLEAN:
        return TYPE_BOOL;
    case AST_LAMBDA_EXPR:
        return type_check_lambda_expression(expr, ctx);

    case AST_IDENTIFIER: {
        const char *expr_name = ast_identifier_name(expr);
        /* Special handling for Option value constructors used without parens */
        if (pgy_match_variant_lookup(expr_name)
                == PGY_MATCH_VARIANT_NONE_CTOR) {
            Type *none = wrap_constructed(TYPE_OPTION, TYPE_UNKNOWN);
            if (semantic_param_publication_pending(ctx) &&
                !semantic_param_publication_record_value(ctx, expr, none, true))
                semantic_error(ctx, expr, "Checked None value publication failed");
            return none;
        }
        Symbol *sym = lookup_identifier_symbol(expr, ctx);
        if (sym == NULL) {
            Type *field_type = expr_current_host_field_type(
                ctx, expr_name);
            if (field_type != NULL)
                return field_type;
            if (name_looks_qualified(expr_name)) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_UNDEFINED_SYMBOL, PGY_CAUSE_SYMBOL_UNDEFINED, PGY_FIX_IMPORT_OR_DECLARE_SYMBOL, expr,
                    "Undefined symbol '%s' (check namespace spelling or export visibility)",
                    expr_name);
            } else {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_UNDEFINED_SYMBOL, PGY_CAUSE_SYMBOL_UNDEFINED, PGY_FIX_IMPORT_OR_DECLARE_SYMBOL, expr,
                    "Undefined symbol '%s'",
                    expr_name);
            }
            return TYPE_UNKNOWN;
        }
        if (sym->kind == SYMBOL_FUNCTION)
            semantic_string_window_extent_record_function_value(
                ctx, expr, sym->decl_syntax_id);
        if (!semantic_future_validate_use(sym, expr, ctx))
            return TYPE_UNKNOWN;
        if (!semantic_indexed_string_borrow_validate_use(sym, expr, ctx))
            return TYPE_UNKNOWN;
        if ((type_is_general_boundary_type(sym->type, ctx)
             || type_is_constructed_named(sym->type, "Array")
             || type_is_move_token(sym->type))
            && sym->is_consumed) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_MOVE_FROM_RELEASED,
                PGY_CAUSE_MOVE_FROM_RELEASED, PGY_FIX_RECLAIM_OR_TRACE_EARLIER_MOVE,
                expr,
                "%s '%s' was moved or released and cannot be used again.\n"
                "Reason:\n"
                "- value '%s' was already consumed by an ownership transfer or release path\n"
                "- ownership-bearing boundary values cannot be reused after consumption\n"
                "Fix:\n"
                "- create/acquire a fresh %s value\n"
                "- or keep ownership in one binding and avoid the earlier move",
                type_is_subject_type(sym->type, ctx)
                    ? type_name_or_unknown(sym->type)
                    : resource_handle_display_name(sym->type),
                expr_name,
                expr_name != NULL ? expr_name : "<value>",
                type_is_subject_type(sym->type, ctx)
                    ? type_name_or_unknown(sym->type)
                    : resource_handle_display_name(sym->type));
            return TYPE_UNKNOWN;
        }
        if (sym->kind == SYMBOL_SLOT && sym->type != NULL
            && type_is_owned_slot_handle(sym->type)) {
            const char *active_view_name = NULL;
            bool active_is_write = false;
            if (semantic_find_active_slot_view_for_source(ctx->scope,
                    sym->name, &active_view_name, NULL, &active_is_write)
                && active_is_write) {
                semantic_error_with_hints(ctx,
                    PGY_CODE_SEM_PIN_PARALLEL_CONFLICT,
                    PGY_CAUSE_PIN_PARALLEL_CONFLICT,
                    PGY_FIX_SERIALIZE_PIN_ACCESS,
                    expr,
                    "Cannot read slot '%s' while WriteView '%s' is live.\n"
                    "Reason:\n"
                    "- slot identifier use in value position auto-reads the owner slot\n"
                    "- owner reads during a live write view would bypass the view's aliasing contract\n"
                    "Fix:\n"
                    "- end the write view scope before reading '%s'\n"
                    "- or split the operation into a read-only view followed by a write view",
                    sym->name,
                    active_view_name != NULL ? active_view_name : "<view>",
                    sym->name);
                return TYPE_UNKNOWN;
            }
        }
        sym->is_used = true;
        return sym->type;
    }

    case AST_BINARY:
        return type_check_binary(expr, ctx);
    case AST_UNARY:
        return type_check_unary(expr, ctx);

    case AST_CALL: {
        Type *call_type = type_check_call(expr, ctx);
        semantic_string_window_extent_record_call(ctx, expr);
        return call_type;
    }

    case AST_MEMBER_ACCESS:
        return type_check_member_access(expr, ctx);

    case AST_ARRAY_ACCESS:
        return type_check_array_access(expr, ctx);

    case AST_ARRAY_LITERAL:
        return type_check_array_literal(expr, ctx);

    case AST_PARALLEL_BLOCK:
        /* Expression-form parallel join (docs/181 R2):
         * `let rs = parallel ... join with all { give <expr>; };` */
        return type_check_parallel_join_expr_type(expr, ctx);

    case AST_MAP_LITERAL:
        return type_check_map_literal(expr, ctx);

    case AST_SET_LITERAL:
        return type_check_set_literal(expr, ctx);

    case AST_CAST: {
        const char *target = ast_cast_target_type(expr);
        Type *source = expr_normalize_type(
            type_check_expression(ast_cast_operand(expr), ctx));
        bool source_numeric = type_equals(source, TYPE_INT)
            || type_equals(source, TYPE_LONG)
            || type_equals(source, TYPE_FLOAT);
        Type *numeric_target = NULL;
        if (target != NULL && strcmp(target, "Int") == 0)
            numeric_target = TYPE_INT;
        else if (target != NULL && strcmp(target, "Long") == 0)
            numeric_target = TYPE_LONG;
        else if (target != NULL && strcmp(target, "Float") == 0)
            numeric_target = TYPE_FLOAT;
        if (numeric_target != NULL) {
            if (source_numeric && semantic_param_publication_pending(ctx) &&
                !semantic_param_publication_record_checked_numeric_cast(ctx, expr))
                semantic_error(ctx, expr, "Checked numeric cast publication failed");
            if (!source_numeric)
                semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
                    PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                    PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr,
                    "Cast to %s requires a numeric operand, got '%s'",
                    target, type_name_or_unknown(source));
            return numeric_target;
        }
        if (target != NULL && strcmp(target, "String") == 0)
            return TYPE_STRING;
        semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
            PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
            PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr,
            "Cast 'expr as %s' is supported only for Int, Long, Float, and String targets",
            target != NULL ? target : "<type>");
        return TYPE_UNKNOWN;
    }

    case AST_TYPE_TEST: {
        const char *target = ast_type_test_target_type(expr);
        /* Evaluate the operand so its own errors surface; the result of the
         * predicate is a scalar test folded by both backends. */
        (void)expr_normalize_type(
            type_check_expression(ast_type_test_operand(expr), ctx));
        if (ast_type_name_canonical_scalar(target) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
                PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr,
                "Type-test 'expr is %s' is supported only for Int, Long, Float, and Bool targets",
                target != NULL ? target : "<type>");
            return TYPE_UNKNOWN;
        }
        return TYPE_BOOL;
    }

    case AST_TUPLE_LITERAL: {
        size_t n = ast_tuple_literal_count(expr);
        if (n == 0)
            return TYPE_VOID;  /* unit value `()` */
        if (n < 2) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr,
                "Tuple literal requires at least 2 elements");
            return TYPE_UNKNOWN;
        }
        Type **elems = calloc(n, sizeof(Type *));
        if (elems == NULL)
            return TYPE_UNKNOWN;
        for (size_t i = 0; i < n; i++) {
            elems[i] = expr_normalize_type(type_check_expression(
                ast_tuple_literal_element(expr, i), ctx));
            if (type_equals(elems[i], TYPE_VOID)) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
                    PGY_CAUSE_ARRAY_LITERAL_ELEMENT_TYPE_MISMATCH,
                    PGY_FIX_ALIGN_ARRAY_ELEMENT_TYPES,
                    ast_tuple_literal_element(expr, i),
                    "Void expression cannot be stored as a tuple literal element; split the side effect before constructing the tuple");
                elems[i] = TYPE_UNKNOWN;
            }
        }
        Type *tup = type_create_tuple(elems, n);
        free(elems);
        return tup != NULL ? tup : TYPE_UNKNOWN;
    }

    case AST_ASSIGNMENT:
        return type_check_assignment(expr, ctx);

    case AST_AWAIT_EXPR:
        if (!ctx->in_async_func) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
                PGY_CAUSE_ASYNC_CONTEXT_REQUIRED, PGY_FIX_MOVE_INTO_ASYNC_FUNCTION,
                expr,
                "'await' used outside of async function");
        }
        (void)semantic_reject_active_slot_view_boundary(expr, ctx,
            "await suspension boundary",
            "await may suspend and resume after unrelated runtime work changes the slot frontier",
            "move await");
        semantic_record_effect(ctx, EFFECT_REMOTE);
        {
            ASTNode *awaited = ast_await_expression(expr);
            SemanticSpawnHandleUse saved_spawn_use = ctx->spawn_handle_use;
            size_t diagnostic_base = ctx->diagnostic_count;
            Type *future_type;
            if (awaited != NULL && awaited->type == AST_SPAWN_EXPR) {
                ctx->spawn_handle_use =
                    SEMANTIC_SPAWN_HANDLE_IMMEDIATE_AWAIT;
            }
            future_type = type_check_expression(awaited, ctx);
            ctx->spawn_handle_use = saved_spawn_use;
            bool is_local_future = future_type != NULL
                && type_equals(type_constructed_constructor(future_type),
                    TYPE_FUTURE);
            bool is_remote_future = future_type != NULL
                && type_equals(type_constructed_constructor(future_type),
                    TYPE_REMOTE_FUTURE);
            if (future_type != NULL
                && (is_local_future || is_remote_future)
                && type_constructed_arg_count(future_type) == 1) {
                Type *inner = type_constructed_arg(future_type, 0);
                /* RemoteFuture<T> -> Result<T>: remote operations can fail
                 * (network partition, timeout, etc.) so the result must be
                 * explicitly handled. Local Future<T> -> T as before. */
                if (is_remote_future) {
                    if (type_equals(inner, TYPE_VOID)) {
                        semantic_error_with_hints(ctx,
                            PGY_CODE_SEM_REMOTE_FUTURE_MISUSE,
                            PGY_CAUSE_REMOTE_FUTURE_DIRECT_ACCESS,
                            PGY_FIX_AWAIT_FUTURE_TYPE,
                            expr,
                            "RemoteFuture<Void> await is not in the stable "
                            "subset; return a value payload or model the "
                            "remote completion as Result<Int> until "
                            "Result<Void> ABI is frozen");
                        return TYPE_UNKNOWN;
                    }
                    Symbol *awaited_sym = lookup_identifier_symbol(awaited, ctx);
                    semantic_future_complete(awaited_sym);
                    Type *result_args[1] = { inner };
                    return type_create_constructed(TYPE_RESULT, result_args, 1);
                }
                Symbol *awaited_sym = lookup_identifier_symbol(awaited, ctx);
                semantic_future_complete(awaited_sym);
                return inner;
            }
            if (future_type == TYPE_UNKNOWN
                && semantic_future_use_failure_was_reported(awaited,
                    diagnostic_base, ctx)) {
                return TYPE_UNKNOWN;
            }
            semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
                PGY_CAUSE_AWAIT_NON_FUTURE, PGY_FIX_AWAIT_FUTURE_TYPE,
                awaited,
                "'await' requires Future<T> or RemoteFuture<T>");
            return TYPE_UNKNOWN;
        }

    case AST_SPAWN_EXPR:
        return type_check_spawn_expr(expr, ctx);

    case AST_CHANNEL_SEND:
        return type_check_channel_send(expr, ctx);

    case AST_CHANNEL_RECV:
        return type_check_channel_recv(expr, ctx);

    default:
        return TYPE_UNKNOWN;
    }
}

Type *
type_check_expression(ASTNode *expr, SemanticContext *ctx)
{
    double t0;
    Type *result;

    if (expr == NULL)
        return TYPE_VOID;
    if (!semantic_termination_contract_tick(ctx, expr))
        return TYPE_UNKNOWN;
    if (semantic_param_publication_pending(ctx) &&
        !semantic_param_publication_begin_value(ctx, expr)) {
        semantic_error(ctx, expr, "Checked value entry publication failed");
        return TYPE_UNKNOWN;
    }
    if (!expr_visit_census_enabled()) {
        result = type_check_expression_dispatch(expr, ctx);
    } else {
        ExprVisitCensus *census = expr_visit_census();
        census->visits_total++;
        if ((size_t)expr->type < 512)
            census->visits_by_kind[(size_t)expr->type]++;
        t0 = expr_census_now();
        result = type_check_expression_dispatch(expr, ctx);
        if ((size_t)expr->type < 512)
            census->seconds_by_kind[(size_t)expr->type] += expr_census_now() - t0;
    }
    if (semantic_param_publication_pending(ctx) && !ctx->has_error) {
        if (result != NULL && result != TYPE_UNKNOWN &&
            !semantic_param_publication_record_value(ctx, expr, result, false))
            semantic_error(ctx, expr, "Checked value publication failed");
        if (expr->type == AST_CALL &&
            !semantic_param_publication_record_checked_call(ctx, expr))
            semantic_error(ctx, expr, "Checked call publication failed");
    }
    return result;
}
