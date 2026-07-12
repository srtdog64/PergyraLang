/*
 * Copyright (c) 2026 Pergyra Language Project
 * Join-form parallel admission (docs/181 SS1, rungs 0+1):
 *   parallel (x in xs)     [join with all] { body }   element mode (R0)
 *   parallel (i in lo..hi) [join with all] { body }   index mode (R1)
 *
 * The body is REPLICATED -- one runtime task per element/index -- so the
 * admission rules differ from the arms form in one load-bearing way:
 * single-writer evidence can never hold (there are N concurrent
 * instances of the "single" writer), so every write to an outer binding
 * fails closed. The one admitted write shape is the R1 index discipline:
 * in index mode, an outer array whose EVERY body access is exactly
 * `name[binding]` touches only the task-owned element (index-disjointness
 * evidence, value-independent and alias-robust). This checker is the
 * single producer of those fact rows; both backend emitters consume them
 * and admit array captures from the sealed list only.
 */

#include <string.h>
#include "type_checker_internal.h"
#include "type_checker_flow_internal.h"
#include "diag_codes.h"
#include "../parser/ast_analysis.h"

static bool
parallel_join_element_type_supported(const Type *elem)
{
    const char *tn = elem != NULL ? elem->name : NULL;

    if (tn == NULL)
        return false;
    return strcmp(tn, "Int") == 0
        || strcmp(tn, "Long") == 0
        || strcmp(tn, "Float") == 0
        || strcmp(tn, "Double") == 0
        || strcmp(tn, "Bool") == 0;
}

/* Element mode (rung 0): the fan-out source must be a named Array<T> of a
 * primitive element type. Stores the element type for the body binding. */
static bool
parallel_join_admit_collection(ASTNode *node, ASTNode *coll,
                               SemanticContext *ctx, Type **elem_type_out,
                               const char **coll_name_out)
{
    const char *coll_name;
    Symbol *coll_sym;
    Type *elem_type;

    if (coll == NULL || coll->type != AST_IDENTIFIER) {
        semantic_error(ctx, node,
            "parallel join rung 0 requires a named Array binding as its collection (docs/181 SS1)");
        return false;
    }
    coll_name = ast_identifier_name(coll);
    coll_sym = coll_name != NULL ? scope_lookup(ctx->scope, coll_name) : NULL;
    if (coll_sym == NULL || coll_sym->type == NULL) {
        semantic_error(ctx, node,
            "parallel join collection binding is not declared in this scope");
        return false;
    }
    if (!type_is_constructed_named(coll_sym->type, "Array")
        || type_constructed_arg_count(coll_sym->type) != 1) {
        semantic_error(ctx, node,
            "parallel join rung 0 requires an Array<T> collection (docs/181 SS1)");
        return false;
    }
    elem_type = type_constructed_arg(coll_sym->type, 0);
    if (!parallel_join_element_type_supported(elem_type)) {
        semantic_error(ctx, node,
            "parallel join rung 0 supports primitive element types only (Int/Long/Float/Double/Bool); wider element kinds are a later rung (docs/181 SS1.4)");
        return false;
    }
    *elem_type_out = elem_type;
    *coll_name_out = coll_name;
    return true;
}

/* Index mode (R1): both range endpoints are Int expressions evaluated
 * once at the call site; the binding is the Int index. */
static bool
parallel_join_admit_range(ASTNode *lo, ASTNode *hi, SemanticContext *ctx)
{
    Type *lo_t = flow_normalize_type(type_check_expression(lo, ctx));
    Type *hi_t = flow_normalize_type(type_check_expression(hi, ctx));

    require_assignable(lo_t, TYPE_INT, lo, ctx);
    require_assignable(hi_t, TYPE_INT, hi, ctx);
    return !ctx->has_error;
}

/* Index mode (R1): every outer array the body touches must satisfy the
 * [binding]-only discipline; each admitted name becomes a fact row. Reads
 * at any other index are rejected too -- two captured names may alias the
 * same backing storage, so only the everything-lands-on-element-i shape
 * is sound without alias evidence. */
static bool
parallel_join_admit_index_arrays(ASTNode *node, ASTNode *body,
                                 const char *elem_name,
                                 SemanticContext *ctx)
{
    for (Scope *scope = ctx->scope; scope != NULL; scope = scope->parent) {
        for (size_t i = 0; i < scope->symbol_count; i++) {
            Symbol *sym = scope->symbols[i];

            if (sym == NULL || sym->name == NULL || sym->type == NULL)
                continue;
            if (!type_is_constructed_named(sym->type, "Array"))
                continue;
            if (!ast_contains_free_identifier_ref(body, sym->name))
                continue;
            /* A shadowed outer array of the same name resolves to the
             * same admission verdict; the first (innermost) row wins. */
            if (ast_parallel_join_index_array_admitted(node, sym->name))
                continue;
            if (type_constructed_arg_count(sym->type) != 1
                || !parallel_join_element_type_supported(
                       type_constructed_arg(sym->type, 0))) {
                semantic_error(ctx, body,
                    "parallel join index form carries Array<Int/Long/Float/Double/Bool> captures only; wider element kinds are a later rung (docs/181 SS1.4)");
                return false;
            }
            /* R5 snapshot-read relaxation (docs/181): an array the body
             * never writes and only ever uses as `name[<expr>]` may be
             * read at ANY index -- there is no write set of its own to
             * collide with. The one residual hazard is its backing
             * aliasing an index-WRITTEN array (`let b = a;` is a handle
             * copy); the emitters close that with a fail-closed alias
             * check at fan-out entry, so admission here plus that check
             * is sound. Jacobi-style double buffering is the intended
             * shape; in-place neighbor access stays rejected below. */
            if (!ast_statement_assigns_identifier(body, sym->name)
                && ast_identifier_only_indexed_reads(body, sym->name)) {
                if (!ast_parallel_add_join_readonly_array(node,
                                                          sym->name)) {
                    semantic_error(ctx, node,
                        "Snapshot-read fact allocation failed while admitting parallel join array");
                    return false;
                }
                continue;
            }
            if (!ast_identifier_only_indexed_by(body, sym->name,
                                                elem_name)) {
                semantic_error_with_hints(ctx,
                    PGY_CODE_SEM_PARALLEL_SLOT_RACE_RISK,
                    PGY_CAUSE_PARALLEL_RESOURCE_CONFLICT,
                    PGY_FIX_SERIALIZE_OUTSIDE_PARALLEL,
                    body,
                    "parallel join body accesses array '%s' outside the index-disjoint form '%s[%s]' (docs/181 R1).\n"
                    "Reason:\n"
                    "- task i owns index i; only '%s[%s]' stays on the owned element\n"
                    "- a fixed or computed index, or a whole-array use, can touch another task's element\n"
                    "Fix:\n"
                    "- access the array as '%s[%s]' only\n"
                    "- or send values through a channel and reduce after the join",
                    sym->name, sym->name, elem_name,
                    sym->name, elem_name,
                    sym->name, elem_name);
                return false;
            }
            if (!ast_parallel_add_join_index_array(node, sym->name)) {
                semantic_error(ctx, node,
                    "Index-disjointness fact allocation failed while admitting parallel join array");
                return false;
            }
        }
    }
    return true;
}

/* Validates the join form (both modes); on success stores the element
 * type for the body-scope binding. Any failure records a semantic error. */
bool
type_check_parallel_join_admit(ASTNode *node, SemanticContext *ctx,
                               Type **elem_type_out)
{
    ASTNode *coll;
    ASTNode *range_end;
    ASTNode *body;
    const char *coll_name = NULL;
    const char *elem_name;
    Type *elem_type = NULL;
    bool index_mode;

    if (node == NULL || ctx == NULL || elem_type_out == NULL)
        return false;
    *elem_type_out = NULL;

    coll = ast_parallel_join_collection(node);
    range_end = ast_parallel_join_range_end(node);
    index_mode = range_end != NULL;
    elem_name = ast_parallel_join_element(node);
    body = ast_parallel_task(node, 0);

    /* This checker is the single producer of the index-disjointness and
     * snapshot-read fact families (docs/180 SS6); the resets keep
     * re-checks idempotent. The scalar-race checker owns (and resets)
     * the snapshot-row family. */
    ast_parallel_reset_join_index_arrays(node);
    ast_parallel_reset_join_readonly_arrays(node);

    if (index_mode) {
        if (coll == NULL || !parallel_join_admit_range(coll, range_end, ctx))
            return false;
        elem_type = TYPE_INT;
    } else {
        if (!parallel_join_admit_collection(node, coll, ctx, &elem_type,
                                            &coll_name))
            return false;
    }
    if (body == NULL || ast_parallel_task_count(node) != 1) {
        semantic_error(ctx, node,
            "parallel join form requires exactly one body block");
        return false;
    }
    if (elem_name == NULL) {
        semantic_error(ctx, node,
            "parallel join form is missing its element binding name");
        return false;
    }
    if (scope_lookup(ctx->scope, elem_name) != NULL) {
        semantic_error(ctx, node,
            "parallel join element binding must not shadow an outer binding");
        return false;
    }
    /* The fan-out length and storage must stay construction-stable: the
     * body never touches the collection binding itself. (Index mode has
     * no collection; the endpoints are call-site reads.) */
    if (!index_mode
        && ast_contains_free_identifier_ref(body, coll_name)) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_BORROW_ESCAPE,
            PGY_CAUSE_BORROW_ESCAPE,
            PGY_FIX_SERIALIZE_OUTSIDE_PARALLEL,
            body,
            "parallel join body cannot reference the collection '%s' it fans out over.\n"
            "Reason:\n"
            "- each task owns exactly one element (N-way disjointness evidence)\n"
            "- touching the whole collection would alias every other task's element\n"
            "Fix:\n"
            "- use the element binding, or send data through a channel",
            coll_name);
        return false;
    }
    if (ast_statement_assigns_identifier(body, elem_name)) {
        semantic_error(ctx, body,
            "parallel join element binding is read-only (docs/181 SS1.2); write elements through the index form 'arr[i] = v'");
        return false;
    }

    if (index_mode
        && !parallel_join_admit_index_arrays(node, body, elem_name, ctx))
        return false;

    /* Replicated-arm rule: N instances of the body run concurrently, so a
     * write to ANY outer binding is a write-write race by construction.
     * The one exception is the R1 fact list above -- writes that land on
     * the task-owned element only. */
    for (Scope *scope = ctx->scope; scope != NULL; scope = scope->parent) {
        for (size_t i = 0; i < scope->symbol_count; i++) {
            Symbol *sym = scope->symbols[i];

            if (sym == NULL || sym->name == NULL)
                continue;
            if (ast_parallel_join_index_array_admitted(node, sym->name))
                continue;
            if (!ast_statement_assigns_identifier(body, sym->name))
                continue;
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_PARALLEL_SLOT_RACE_RISK,
                PGY_CAUSE_PARALLEL_RESOURCE_CONFLICT,
                PGY_FIX_SERIALIZE_OUTSIDE_PARALLEL,
                body,
                "parallel join body writes outer binding '%s': join arms are replicated (one task per element), so no single-writer evidence can hold.\n"
                "Reason:\n"
                "- every element task would execute this write concurrently\n"
                "Fix:\n"
                "- send results through a channel and reduce after the join\n"
                "- or use the index form 'parallel (i in lo..hi)' with '%s[i]' writes (docs/181 R1)",
                sym->name, sym->name);
            return false;
        }
    }

    if (!index_mode)
        (void)type_check_expression(coll, ctx);
    *elem_type_out = elem_type;
    return !ctx->has_error;
}

/* --- R2: expression form (`let rs = parallel ... { give <expr>; };`) --- */

/* Counts give statements in the body without descending into a nested
 * parallel block (a nested join owns its own gives). */
static size_t
parallel_join_count_gives(const ASTNode *node)
{
    if (node == NULL)
        return 0;
    switch (node->type) {
    case AST_GIVE_STMT:
        return 1;
    case AST_BLOCK: {
        size_t n = 0;
        for (size_t i = 0; i < node->data.block.count; i++)
            n += parallel_join_count_gives(node->data.block.statements[i]);
        return n;
    }
    case AST_IF_STMT:
        return parallel_join_count_gives(node->data.if_stmt.then_branch)
             + parallel_join_count_gives(node->data.if_stmt.else_branch);
    case AST_WHILE_LOOP:
        return parallel_join_count_gives(node->data.while_loop.body);
    case AST_FOR_LOOP:
        return parallel_join_count_gives(node->data.for_loop.body);
    case AST_PARALLEL_BLOCK:
        return 0;
    default:
        return 0;
    }
}

/* Statement flow for `give <expr>;` -- legal only inside an
 * expression-form parallel join body. */
FlowFlags
type_check_give_stmt_flow(ASTNode *node, SemanticContext *ctx)
{
    if (node == NULL || ctx == NULL)
        return FLOW_FALLTHROUGH;
    if (!ctx->in_parallel_join_expr) {
        semantic_error(ctx, node,
            "give names a per-task result; it is only legal as the final statement of an expression-form parallel join 'let rs = parallel ...' (docs/181 R2)");
        return FLOW_FALLTHROUGH;
    }
    (void)type_check_expression(ast_give_value(node), ctx);
    return FLOW_FALLTHROUGH;
}

/* Type of an expression-position parallel join: Array<R> where R is the
 * give value's (primitive) type. Runs the full statement admission --
 * the expression form is the statement form plus a result. */
Type *
type_check_parallel_join_expr_type(ASTNode *node, SemanticContext *ctx)
{
    ASTNode *body;
    ASTNode *last = NULL;
    size_t gives;
    Type *elem_type = NULL;
    Type *give_type;
    bool prev_expr_flag;

    if (node == NULL || ctx == NULL)
        return TYPE_UNKNOWN;
    if (!ast_parallel_is_join_form(node)) {
        semantic_error(ctx, node,
            "parallel/async block in expression position: only the join form 'parallel (x in xs) ... { give <expr>; }' produces a value (docs/181 R2)");
        return TYPE_UNKNOWN;
    }
    body = ast_parallel_task(node, 0);
    if (body != NULL && body->type == AST_BLOCK
        && body->data.block.count > 0)
        last = body->data.block.statements[body->data.block.count - 1];
    gives = parallel_join_count_gives(body);
    if (last == NULL || last->type != AST_GIVE_STMT) {
        if (gives == 0)
            semantic_error(ctx, node,
                "parallel join expression form requires a final 'give' statement (docs/181 R2)");
        else
            semantic_error(ctx, node,
                "'give' must be the final statement of the parallel join body (docs/181 R2)");
        return TYPE_UNKNOWN;
    }
    if (gives != 1) {
        semantic_error(ctx, node,
            "'give' must be the final statement of the parallel join body, exactly once (docs/181 R2)");
        return TYPE_UNKNOWN;
    }

    prev_expr_flag = ctx->in_parallel_join_expr;
    ctx->in_parallel_join_expr = true;
    (void)type_check_parallel_block_flow(node, ctx);
    ctx->in_parallel_join_expr = prev_expr_flag;
    if (ctx->has_error)
        return TYPE_UNKNOWN;

    /* Result element type: the give value checked in the body scope (the
     * element binding declared exactly like the task loop declares it).
     * The admission re-run is idempotent by design. */
    if (!type_check_parallel_join_admit(node, ctx, &elem_type))
        return TYPE_UNKNOWN;
    scope_enter(&ctx->scope, SCOPE_BLOCK);
    if (elem_type != NULL) {
        const char *elem_name = ast_parallel_join_element(node);
        Symbol *elem_sym = elem_name != NULL
            ? symbol_create_variable(elem_name, elem_type,
                                     node->line, node->column)
            : NULL;
        if (elem_sym != NULL)
            scope_declare(ctx->scope, elem_sym);
    }
    give_type = flow_normalize_type(
        type_check_expression(ast_give_value(last), ctx));
    scope_exit(&ctx->scope);
    if (!parallel_join_element_type_supported(give_type)) {
        semantic_error(ctx, last,
            "parallel join 'give' carries primitive results only (Int/Long/Float/Double/Bool); wider result kinds are a later rung (docs/181 SS1.4)");
        return TYPE_UNKNOWN;
    }
    /* Seal the result type as a node fact: both backend emitters derive
     * the result shape from it instead of re-inferring the body
     * (docs/180 SS6). */
    if (!ast_parallel_set_join_give_type(node, give_type->name)) {
        semantic_error(ctx, node,
            "Give-result fact allocation failed while admitting parallel join expression");
        return TYPE_UNKNOWN;
    }
    /* R4 reduce combinators (docs/181 R4): the fold is numeric-only
     * (Bool has no sum/product/min/max meaning) and yields the scalar
     * give type instead of Array<R>. */
    if (ast_parallel_join_reduce_op(node) != NULL) {
        if (give_type->name != NULL
            && strcmp(give_type->name, "Bool") == 0) {
            semantic_error(ctx, last,
                "parallel join reduce ('sum'/'product'/'min'/'max') folds numeric gives only (Int/Long/Float/Double) (docs/181 R4)");
            return TYPE_UNKNOWN;
        }
        return give_type;
    }
    /* R3 any-join (docs/181): the first give wins; the result is one
     * scalar. Element mode only in this slice -- losers' effects stay
     * discarded values by construction (no index-written arrays), which
     * is what keeps early decision sound. */
    if (ast_parallel_join_is_any(node)) {
        if (ast_parallel_join_range_end(node) != NULL) {
            semantic_error(ctx, node,
                "parallel join with any carries the element mode only in this slice; index-form in-place writes would leave losers' partial writes observable (docs/181 R3)");
            return TYPE_UNKNOWN;
        }
        return give_type;
    }
    return wrap_constructed(TYPE_ARRAY, give_type);
}
