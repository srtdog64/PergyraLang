/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Let-bound Slice disjointness evidence.
 */

#include <string.h>
#include <stdlib.h>

#include "type_checker_internal.h"
#include "type_checker_ownership_let_internal.h"
#include "../diag_codes.h"

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

static void
slice_borrow_source_append(Symbol *sym, Symbol *root, ASTNode *place, bool readonly,
                          SemanticContext *ctx)
{
    if (root != NULL) {
        /* Initializers are checked before their binding is declared. Rebinds
         * retain the scope of the already declared Symbol. */
        Scope *binding_scope = sym->birth_serial == 0 ? ctx->scope : NULL;
        Scope *source_scope = NULL;
        for (Scope *scope = ctx->scope; scope != NULL; scope = scope->parent)
            for (size_t i = 0; i < scope->symbol_count; i++) {
                if (scope->symbols[i] == sym) binding_scope = scope;
                if (scope->symbols[i] == root) source_scope = scope;
            }
        bool outlives = false;
        for (Scope *scope = binding_scope; scope != NULL; scope = scope->parent)
            if (scope == source_scope) outlives = true;
        if (!outlives) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_REDUCE_SCOPE_OR_RETRY, place,
                "Slice backing scope must outlive the view binding; keep the view inside its owner's scope or use SliceCopy");
            return;
        }
    }
    for (SliceBorrowSource *row = sym->slice_borrow_sources; row; row = row->next)
        if (row->root == root && row->readonly == readonly && ((row->place == NULL && place == NULL)
                || ownership_slice_place_shape_equal(row->place, place)))
            return;
    SliceBorrowSource *row = malloc(sizeof(*row));
    if (row == NULL) {
        semantic_error(ctx, place, "Unable to retain Slice backing permission");
        return;
    }
    *row = (SliceBorrowSource){root, place, readonly, sym->slice_borrow_sources};
    sym->slice_borrow_sources = row;
}

void
ownership_slice_record_source(Symbol *sym, ASTNode *value, SemanticContext *ctx)
{
    if (sym == NULL || !type_is_constructed_named(sym->type, "Slice")) return;
    ASTNode *source = value;
    if (source != NULL && source->type == AST_CALL) {
        ASTNode *callee = ast_call_callee(source);
        if (callee != NULL && callee->type == AST_MEMBER_ACCESS
            && strcmp(ast_member_name(callee), "Slice") == 0)
            source = ast_member_object(callee);
        else source = NULL;
    }
    Symbol *root = ownership_slice_place_root_symbol(source, ctx);
    if (root != NULL && source->type == AST_IDENTIFIER
            && type_is_constructed_named(root->type, "Slice")) {
        if (root == sym) return;
        if (root->is_parameter) {
            slice_borrow_source_append(sym, root, source, root->param_mode == PARAM_MODE_REF, ctx);
            return;
        }
        if (root->slice_borrow_sources != NULL) {
            for (SliceBorrowSource *row = root->slice_borrow_sources; row; row = row->next)
                slice_borrow_source_append(sym, row->root, row->place, row->readonly || !root->is_mut_binding, ctx);
            return;
        }
    }
    Type *type = root != NULL && source->type == AST_IDENTIFIER ? root->type
        : source != NULL && source->type == AST_MEMBER_ACCESS
            ? type_check_expression(source, ctx) : NULL;
    if (type_is_constructed_named(type, "Array"))
        slice_borrow_source_append(sym, root, source, false, ctx);
    else
        slice_borrow_source_append(sym, NULL, NULL, false, ctx);
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

/* docs/178 WO-DOP-1 rung 0. The initializer issues a split fact; a later
 * descriptor replacement retires it before another parallel admission. */
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
        || !type_is_constructed_named(decl_type, "Slice"))
        return;
    ownership_slice_record_source(sym, init, ctx);
    if (init->type != AST_CALL || ast_call_arg_count(init) != 2)
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

    /* Split/disjointness evidence remains direct-Array-only. Member-place
     * lifetime identity is a separate fact and does not imply disjointness. */
    if (object->type != AST_IDENTIFIER)
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
            for (SliceBorrowSource *row = candidate != NULL ? candidate->slice_borrow_sources : NULL;
                    row; row = row->next)
                if (row->root == array_sym && row->place != NULL
                        && row->place->type == AST_IDENTIFIER)
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
            for (SliceBorrowSource *row = candidate != NULL ? candidate->slice_borrow_sources : NULL;
                    row; row = row->next)
                if (row->root == root_sym && ownership_slice_place_shape_equal(row->place, array_place))
                    return candidate;
        }
    }
    return NULL;
}
