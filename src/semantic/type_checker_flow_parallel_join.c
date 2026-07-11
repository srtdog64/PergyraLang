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

    /* This checker is the single producer of the index-disjointness fact
     * family (docs/180 SS6); the reset keeps re-checks idempotent. The
     * scalar-race checker owns (and resets) the snapshot-row family. */
    ast_parallel_reset_join_index_arrays(node);

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
