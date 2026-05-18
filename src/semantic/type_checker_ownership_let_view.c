/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Let-binding ReadView/WriteView declaration checks.
 */

#include <string.h>

#include "diag_codes.h"
#include "type_checker_ownership_let_internal.h"

static const char *
ownership_let_view_call_callee_name(const ASTNode *node)
{
    ASTNode *callee = ast_call_callee(node);
    if (callee == NULL || callee->type != AST_IDENTIFIER)
        return NULL;
    return ast_identifier_name(callee);
}

bool
ownership_let_try_declare_view_binding(ASTNode *node,
                                       SemanticContext *ctx,
                                       const char *name,
                                       Type *decl_type,
                                       ASTNode *init,
                                       bool *handled)
{
    const char *source_slot = NULL;
    bool new_write_view = false;
    bool view_init = false;

    if (handled != NULL)
        *handled = false;
    if (decl_type == NULL || decl_type->kind != TYPE_KIND_SLOT
        || type_slot_access_mode(decl_type) == SLOT_ACCESS_OWNED) {
        return true;
    }
    if (handled != NULL)
        *handled = true;

    if (init != NULL && init->type == AST_CALL
        && ast_call_arg_count(init) >= 1
        && ast_call_argument(init, 0) != NULL
        && ast_call_argument(init, 0)->type == AST_IDENTIFIER) {
        const char *callee_name = ownership_let_view_call_callee_name(init);
        if (callee_name != NULL
            && (strcmp(callee_name, "ViewRead") == 0
                || strcmp(callee_name, "ViewWrite") == 0
                || strcmp(callee_name, "Move") == 0)) {
            source_slot = ast_identifier_name(ast_call_argument(init, 0));
        }
    }

    view_init = ownership_let_view_init_info(init, &source_slot,
                                             &new_write_view);
    if (view_init && source_slot != NULL) {
        const char *existing_name = NULL;
        const char *existing_kind = NULL;
        if (ctx->in_parallel) {
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_PIN_PARALLEL_CONFLICT,
                PGY_CAUSE_PIN_PARALLEL_CONFLICT,
                PGY_FIX_SERIALIZE_PIN_ACCESS,
                node,
                "%s for slot '%s' cannot be acquired inside a parallel task.\n"
                "Reason:\n"
                "- pinned views are scoped capability leases for the stable beta ownership subset\n"
                "- parallel tasks would make view cleanup and aliasing order depend on task scheduling\n"
                "Fix:\n"
                "- acquire the view outside parallel only for sequential work\n"
                "- or serialize the slot access before/after the parallel block",
                new_write_view ? "WriteView<T>" : "ReadView<T>",
                source_slot);
            return false;
        }
        if (ownership_let_find_conflicting_view(ctx->scope,
                                                source_slot,
                                                new_write_view,
                                                &existing_name,
                                                &existing_kind)) {
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_PIN_PARALLEL_CONFLICT,
                PGY_CAUSE_PIN_PARALLEL_CONFLICT,
                PGY_FIX_SERIALIZE_PIN_ACCESS,
                node,
                "%s for slot '%s' conflicts with existing %s '%s'.\n"
                "Reason:\n"
                "- WriteView<T> is exclusive for the stable beta ownership subset\n"
                "- overlapping read/write views would hide aliasing and cleanup order from CFG dataflow\n"
                "Fix:\n"
                "- keep the write view in a smaller scope\n"
                "- or finish/drop the existing view before acquiring a WriteView<T>",
                new_write_view ? "WriteView<T>" : "ReadView<T>",
                source_slot,
                existing_kind != NULL ? existing_kind : "view",
                existing_name != NULL ? existing_name : "<view>");
            return false;
        }
    }

    Symbol *sym = symbol_create_view(name, decl_type, source_slot,
        node->line, node->column);
    scope_declare(ctx->scope, sym);
    return !ctx->has_error;
}
