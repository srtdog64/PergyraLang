/*
 * Copyright (c) 2026 Pergyra Language Project
 * Join-form parallel admission (docs/181 SS1, rung 0):
 * `parallel (x in xs) [join with all] { body }`.
 *
 * The body is REPLICATED — one runtime task per element — so the
 * admission rules differ from the arms form in one load-bearing way:
 * single-writer evidence can never hold (there are N concurrent
 * instances of the "single" writer), so every write to an outer binding
 * fails closed. Element ownership is the N-way disjointness evidence:
 * task i owns element i by construction, value-independently.
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

/* Validates the rung-0 join form; on success stores the element type for
 * the body-scope binding. Any failure records a semantic error. */
bool
type_check_parallel_join_admit(ASTNode *node, SemanticContext *ctx,
                               Type **elem_type_out)
{
    ASTNode *coll;
    ASTNode *body;
    const char *coll_name;
    const char *elem_name;
    Symbol *coll_sym;
    Type *elem_type;

    if (node == NULL || ctx == NULL || elem_type_out == NULL)
        return false;
    *elem_type_out = NULL;

    coll = ast_parallel_join_collection(node);
    elem_name = ast_parallel_join_element(node);
    body = ast_parallel_task(node, 0);

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
     * body never touches the collection binding itself. */
    if (ast_contains_free_identifier_ref(body, coll_name)) {
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
            "parallel join element binding is read-only in rung 0 (docs/181 SS1.2); in-place element writes are a later rung");
        return false;
    }

    /* Replicated-arm rule: N instances of the body run concurrently, so a
     * write to ANY outer binding is a write-write race by construction. */
    for (Scope *scope = ctx->scope; scope != NULL; scope = scope->parent) {
        for (size_t i = 0; i < scope->symbol_count; i++) {
            Symbol *sym = scope->symbols[i];

            if (sym == NULL || sym->name == NULL)
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
                "- send results through a channel and reduce after the join",
                sym->name);
            return false;
        }
    }

    (void)type_check_expression(coll, ctx);
    *elem_type_out = elem_type;
    return !ctx->has_error;
}
