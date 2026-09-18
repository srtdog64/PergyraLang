/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Let-bound Slice disjointness evidence.
 */

#include <string.h>

#include "type_checker_internal.h"
#include "type_checker_ownership_let_internal.h"

static Symbol *
ownership_slice_place_root_symbol(ASTNode *place, SemanticContext *ctx)
{
    ASTNode *cursor = place;

    while (cursor != NULL && cursor->type == AST_MEMBER_ACCESS)
        cursor = ast_member_object(cursor);
    if (cursor == NULL || cursor->type != AST_IDENTIFIER || ctx == NULL)
        return NULL;
    return scope_lookup(ctx->scope, ast_identifier_name(cursor));
}

static bool
ownership_slice_place_shape_equal(const ASTNode *left, const ASTNode *right)
{
    const char *left_member;
    const char *right_member;

    if (left == NULL || right == NULL || left->type != right->type)
        return false;
    if (left->type == AST_IDENTIFIER)
        return true;
    if (left->type != AST_MEMBER_ACCESS)
        return false;
    left_member = ast_member_name(left);
    right_member = ast_member_name(right);
    return left_member != NULL && right_member != NULL
        && strcmp(left_member, right_member) == 0
        && ownership_slice_place_shape_equal(
            ast_member_object(left), ast_member_object(right));
}

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
    Type *base_type;
    Symbol *boundary_sym = NULL;
    long long boundary_lit = 0;
    bool is_upper;

    if (sym == NULL || ctx == NULL || init == NULL
        || !type_is_constructed_named(decl_type, "Slice")
        || init->type != AST_CALL || ast_call_arg_count(init) != 2)
        return;
    callee = ast_call_callee(init);
    if (callee == NULL || callee->type != AST_MEMBER_ACCESS
        || ast_member_name(callee) == NULL
        || strcmp(ast_member_name(callee), "Slice") != 0)
        return;
    object = ast_member_object(callee);
    if (object == NULL || (object->type != AST_IDENTIFIER
            && object->type != AST_MEMBER_ACCESS))
        return;
    base_sym = ownership_slice_place_root_symbol(object, ctx);
    if (base_sym == NULL)
        return;
    base_type = object->type == AST_IDENTIFIER
        ? base_sym->type
        : type_check_expression(object, ctx);
    if (!type_is_constructed_named(base_type, "Array"))
        return;

    sym->slice_borrow_base_sym = base_sym;
    sym->slice_borrow_base_place = object;
    /* Split/disjointness evidence remains direct-Array-only. Member-place
     * lifetime identity is a separate fact and does not imply disjointness. */
    if (object->type != AST_IDENTIFIER)
        return;
    if (ast_let_is_mutable(node))
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

Symbol *
semantic_find_active_slice_borrow_for_array(Scope *scope,
                                            const Symbol *array_sym)
{
    for (Scope *cur = scope; cur != NULL; cur = cur->parent) {
        for (size_t i = 0; i < cur->symbol_count; i++) {
            Symbol *candidate = cur->symbols[i];
            if (candidate != NULL
                && candidate->slice_borrow_base_sym == array_sym
                && candidate->slice_borrow_base_place != NULL
                && candidate->slice_borrow_base_place->type == AST_IDENTIFIER)
                return candidate;
        }
    }
    return NULL;
}

Symbol *
semantic_find_active_slice_borrow_for_array_place(Scope *scope,
                                                  ASTNode *array_place,
                                                  SemanticContext *ctx)
{
    Symbol *root_sym = ownership_slice_place_root_symbol(array_place, ctx);

    if (root_sym == NULL)
        return NULL;
    for (Scope *cur = scope; cur != NULL; cur = cur->parent) {
        for (size_t i = 0; i < cur->symbol_count; i++) {
            Symbol *candidate = cur->symbols[i];
            if (candidate != NULL
                && candidate->slice_borrow_base_sym == root_sym
                && ownership_slice_place_shape_equal(
                    candidate->slice_borrow_base_place, array_place))
                return candidate;
        }
    }
    return NULL;
}
