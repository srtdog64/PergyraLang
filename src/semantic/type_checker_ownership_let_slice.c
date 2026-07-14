/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Let-bound Slice disjointness evidence.
 */

#include <string.h>

#include "type_checker_internal.h"
#include "type_checker_ownership_let_internal.h"

/* A split boundary is stable when both slicing sites use the same immutable
 * Int local or the same non-negative Int literal. */
static bool
ownership_let_slice_split_boundary(ASTNode *boundary_node,
                                   SemanticContext *ctx,
                                   Symbol **boundary_sym_out,
                                   long long *boundary_lit_out)
{
    if (boundary_node == NULL)
        return false;
    if (boundary_node->type == AST_NUMBER) {
        double value = ast_number_value(boundary_node);
        if (value < 0 || value != (double)(long long)value)
            return false;
        *boundary_sym_out = NULL;
        *boundary_lit_out = (long long)value;
        return true;
    }
    if (boundary_node->type == AST_IDENTIFIER) {
        Symbol *boundary = scope_lookup(
            ctx->scope, ast_identifier_name(boundary_node));
        if (boundary == NULL || boundary->kind != SYMBOL_VARIABLE
            || boundary->is_parameter || boundary->is_mut_binding
            || !type_equals(boundary->type, TYPE_INT))
            return false;
        *boundary_sym_out = boundary;
        *boundary_lit_out = 0;
        return true;
    }
    return false;
}

/* docs/178 WO-DOP-1 rung 0. Immutable Slice bindings carry the
 * construction-guaranteed split fact consumed by the parallel boundary. */
void
ownership_let_record_slice_split_fact(ASTNode *node, SemanticContext *ctx,
                                      Symbol *sym, Type *decl_type)
{
    ASTNode *init = ast_let_initializer(node);
    ASTNode *callee;
    ASTNode *object;
    ASTNode *arg0;
    ASTNode *boundary_node;
    Symbol *base_sym;
    Symbol *boundary_sym = NULL;
    long long boundary_lit = 0;
    bool is_upper;

    if (sym == NULL || ctx == NULL || init == NULL
        || ast_let_is_mutable(node)
        || !type_is_constructed_named(decl_type, "Slice")
        || init->type != AST_CALL || ast_call_arg_count(init) != 2)
        return;
    callee = ast_call_callee(init);
    if (callee == NULL || callee->type != AST_MEMBER_ACCESS
        || ast_member_name(callee) == NULL
        || strcmp(ast_member_name(callee), "Slice") != 0)
        return;
    object = ast_member_object(callee);
    if (object == NULL || object->type != AST_IDENTIFIER)
        return;
    base_sym = scope_lookup(ctx->scope, ast_identifier_name(object));
    if (base_sym == NULL
        || !type_is_constructed_named(base_sym->type, "Array"))
        return;

    arg0 = ast_call_argument(init, 0);
    if (arg0 != NULL && arg0->type == AST_NUMBER
        && ast_number_value(arg0) == 0) {
        is_upper = false;
        boundary_node = ast_call_argument(init, 1);
    } else {
        is_upper = true;
        boundary_node = arg0;
    }
    if (!ownership_let_slice_split_boundary(boundary_node, ctx,
                                            &boundary_sym, &boundary_lit))
        return;

    sym->slice_split_info.has_fact = true;
    sym->slice_split_info.is_upper = is_upper;
    sym->slice_split_info.base_sym = base_sym;
    sym->slice_split_info.boundary_sym = boundary_sym;
    sym->slice_split_info.boundary_lit = boundary_lit;
}
