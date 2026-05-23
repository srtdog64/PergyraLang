/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Let-binding ownership helper routines.
 */

#include <string.h>

#include "type_checker_ownership_let_internal.h"

Type *
ownership_let_resolve_type_ref(ASTNode *type_ref, SemanticContext *ctx)
{
    Type *resolved = semantic_type_resolution_lookup_metadata_type_ref(
        ctx, type_ref);
    if (resolved != NULL)
        return resolved;
    if (semantic_type_resolution_reject_invalid_stable_shell_arity(ctx,
                                                                   type_ref))
        return TYPE_UNKNOWN;
    if (semantic_type_resolution_reject_invalid_stable_constructed_type(
            ctx, type_ref))
        return TYPE_UNKNOWN;
    if (semantic_type_resolution_reject_unknown_bare_named_type(ctx, type_ref))
        return TYPE_UNKNOWN;
    return TYPE_UNKNOWN;
}

Type *
ownership_let_resolve_first_call_type_arg(ASTNode *call, SemanticContext *ctx)
{
    GenericParam *param;
    ASTNode *inner_node;
    const char *inner_name;

    if (call == NULL || call->type != AST_CALL
        || ast_call_generic_arg_count(call) < 1
        || ast_call_generic_arg(call, 0) == NULL) {
        return NULL;
    }

    param = ast_call_generic_arg(call, 0);
    inner_node = ast_generic_param_constraint(param);
    inner_name = ast_generic_param_name(param);
    if (inner_node != NULL)
        return ownership_let_resolve_type_ref(inner_node, ctx);
    if (inner_name != NULL)
        return semantic_type_resolution_lookup_metadata_name_or_alias_or_unknown(
            ctx, inner_name, call);
    return NULL;
}

bool
ownership_let_view_init_info(ASTNode *init,
                             const char **source_slot,
                             bool *is_write_view)
{
    ASTNode *callee;
    ASTNode *source_arg;
    const char *callee_name;

    if (source_slot != NULL)
        *source_slot = NULL;
    if (is_write_view != NULL)
        *is_write_view = false;
    if (init == NULL || init->type != AST_CALL)
        return false;

    callee = ast_call_callee(init);
    source_arg = ast_call_argument(init, 0);
    if (callee == NULL || callee->type != AST_IDENTIFIER
        || ast_call_arg_count(init) < 1
        || source_arg == NULL || source_arg->type != AST_IDENTIFIER) {
        return false;
    }

    callee_name = ast_identifier_name(callee);
    if (callee_name == NULL)
        return false;
    if (strcmp(callee_name, "ViewRead") != 0
        && strcmp(callee_name, "ViewWrite") != 0) {
        return false;
    }

    if (source_slot != NULL)
        *source_slot = ast_identifier_name(source_arg);
    if (is_write_view != NULL)
        *is_write_view = strcmp(callee_name, "ViewWrite") == 0;
    return true;
}

bool
ownership_let_find_conflicting_view(Scope *scope,
                                    const char *source_slot,
                                    bool new_write_view,
                                    const char **existing_name,
                                    const char **existing_kind)
{
    for (Scope *cur = scope; cur != NULL; cur = cur->parent) {
        for (size_t i = 0; i < cur->symbol_count; i++) {
            Symbol *sym = cur->symbols[i];
            bool existing_read;
            bool existing_write;

            if (sym == NULL || sym->slot_info.paired_slot_name == NULL
                || source_slot == NULL
                || strcmp(sym->slot_info.paired_slot_name, source_slot) != 0) {
                continue;
            }
            existing_read = type_is_read_view(sym->type);
            existing_write = type_is_write_view(sym->type);
            if (!existing_read && !existing_write)
                continue;
            if (!new_write_view && !existing_write)
                continue;

            if (existing_name != NULL)
                *existing_name = sym->name;
            if (existing_kind != NULL)
                *existing_kind = existing_write ? "WriteView" : "ReadView";
            return true;
        }
    }
    return false;
}

bool
ownership_let_is_unresolved_none_option(const Type *type)
{
    return type != NULL
        && type_constructed_constructor(type) == TYPE_OPTION
        && type_constructed_arg_count(type) == 1
        && type_constructed_arg(type, 0) == TYPE_UNKNOWN;
}

bool
ownership_let_is_unresolved_empty_array(const Type *type)
{
    return type_is_constructed_named(type, "Array")
        && type_constructed_arg_count(type) == 1
        && type_constructed_arg(type, 0) == TYPE_UNKNOWN;
}

bool
ownership_let_is_unresolved_device_slot(const Type *type)
{
    return type_is_constructed_named(type, "DeviceSlot")
        && type_constructed_arg_count(type) == 1
        && type_constructed_arg(type, 0) == TYPE_UNKNOWN;
}
