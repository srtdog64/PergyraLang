/*
 * Copyright (c) 2026 Pergyra Language Project
 * LLVM domain role lookup helpers.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"
#include "llvm_domain_role_helpers.h"
#include "llvm_inventory_decl_lookup.h"
#include "llvm_inventory_host_methods.h"

bool
llvm_role_method_symbol_name(char *out,
                             size_t out_size,
                             const char *role_name,
                             const char *method_name)
{
    int written;

    if (out == NULL || out_size == 0 || role_name == NULL
        || method_name == NULL) {
        return false;
    }

    written = snprintf(out, out_size, "%s_%s", role_name, method_name);
    return written >= 0 && (size_t)written < out_size;
}

bool
llvm_role_operator_symbol_name(char *out,
                               size_t out_size,
                               const char *suffix,
                               const char *for_type_name)
{
    int written;

    if (out == NULL || out_size == 0 || suffix == NULL
        || for_type_name == NULL) {
        return false;
    }

    written = snprintf(out, out_size, "operator_%s_%s", suffix, for_type_name);
    return written >= 0 && (size_t)written < out_size;
}

bool
llvm_role_vtable_type_name(char *out,
                           size_t out_size,
                           const char *ability_name)
{
    int written;

    if (out == NULL || out_size == 0 || ability_name == NULL)
        return false;

    written = snprintf(out, out_size, "%s_vtable", ability_name);
    return written >= 0 && (size_t)written < out_size;
}

bool
llvm_role_vtable_global_name(char *out,
                             size_t out_size,
                             const char *role_name,
                             const char *ability_name)
{
    int written;

    if (out == NULL || out_size == 0 || role_name == NULL
        || ability_name == NULL) {
        return false;
    }

    written = snprintf(out, out_size, "%s_%s_vtable_instance",
        role_name, ability_name);
    return written >= 0 && (size_t)written < out_size;
}

ASTNode *
llvm_find_role_decl(LLVMGenCtx *ctx, const char *role_name)
{
    if (ctx == NULL || role_name == NULL)
        return NULL;
    return llvm_find_decl_in_active_inventory(ctx, AST_ROLE_DECL, role_name);
}

ASTNode *
llvm_role_for_type_node(ASTNode *role)
{
    return ast_role_for_type(role);
}

const char *
llvm_role_for_type_name(ASTNode *role)
{
    ASTNode *for_type = llvm_role_for_type_node(role);

    return ast_type_name(for_type);
}

ASTNode *
llvm_find_role_operator_method(LLVMGenCtx *ctx, ASTNode *role,
                               PgyTokenType op, int depth)
{
    if (ctx == NULL || role == NULL || role->type != AST_ROLE_DECL || depth > 16)
        return NULL;

    for (size_t i = 0; i < ast_role_impl_count(role); i++) {
        ASTNode *impl = ast_role_impl(role, i);
        if (impl == NULL || impl->type != AST_IMPL_ABILITY)
            continue;
        for (size_t j = 0; j < ast_impl_ability_method_count(impl); j++) {
            ASTNode *method = ast_impl_ability_method(impl, j);
            const char *method_name = ast_declaration_name(method);
            if (method != NULL && method->type == AST_FUNC_DECL
                && method_name != NULL
                && llvm_operator_method_name_matches(op, method_name)) {
                return method;
            }
        }
    }

    for (size_t i = 0; i < ast_role_include_count(role); i++) {
        ASTNode *inc = ast_role_include(role, i);
        const char *role_name = ast_include_role_name(inc);
        ASTNode *included;
        ASTNode *method;

        if (role_name == NULL)
            continue;
        included = llvm_find_role_decl(ctx, role_name);
        method = llvm_find_role_operator_method(ctx, included, op, depth + 1);
        if (method != NULL)
            return method;
    }

    return NULL;
}

const MIRDeclMethod *
llvm_find_role_operator_method_metadata(LLVMGenCtx *ctx,
                                        ASTNode *role,
                                        PgyTokenType op,
                                        int depth)
{
    const char *role_name;
    LLVMHostedMethodView view;

    if (ctx == NULL || role == NULL || role->type != AST_ROLE_DECL
        || depth > 16) {
        return NULL;
    }

    role_name = llvm_decl_node_name(role);
    view = llvm_hosted_method_view_from_decl(ctx, role_name, role);
    for (size_t i = 0; i < view.count; i++) {
        const MIRDeclMethod *method =
            llvm_hosted_method_view_metadata(&view, i);
        const char *method_name = llvm_mir_decl_method_name(method);

        if (method_name != NULL
            && llvm_operator_method_name_matches(op, method_name)) {
            return method;
        }
    }

    for (size_t i = 0; i < ast_role_include_count(role); i++) {
        ASTNode *inc = ast_role_include(role, i);
        const char *included_name = ast_include_role_name(inc);
        ASTNode *included;
        const MIRDeclMethod *method;

        if (included_name == NULL)
            continue;
        included = llvm_find_role_decl(ctx, included_name);
        method = llvm_find_role_operator_method_metadata(
            ctx, included, op, depth + 1);
        if (method != NULL)
            return method;
    }

    return NULL;
}

const char *
llvm_party_slot_first_ability_name(LLVMGenCtx *ctx,
                                   const char *party_type_name,
                                   const char *slot_name)
{
    ASTNode *party_decl;
    LLVMHostedRoleSlotView role_view;

    if (ctx == NULL || party_type_name == NULL || slot_name == NULL)
        return NULL;

    party_decl = llvm_find_named_domain_decl(ctx, AST_PARTY_DECL,
        party_type_name);
    if (party_decl == NULL || party_decl->type != AST_PARTY_DECL)
        return NULL;

    role_view = llvm_hosted_role_slot_view_from_decl(
        ctx, party_type_name, party_decl);
    if (llvm_hosted_role_slot_view_missing_mir_metadata(&role_view))
        return NULL;

    for (size_t i = 0; i < role_view.count; i++) {
        const char *role_slot_name =
            llvm_hosted_role_slot_view_name(&role_view, i);
        ASTNode *ability_ref;
        const char *ability_name;

        if (role_slot_name == NULL
            || strcmp(role_slot_name, slot_name) != 0) {
            continue;
        }

        ability_ref =
            llvm_hosted_role_slot_view_required_ability(&role_view, i, 0);
        ability_name = ast_type_name(ability_ref);
        return ability_name;
    }

    return NULL;
}

LLVMValueRef
llvm_lookup_role_vtable_global(LLVMGenCtx *ctx,
                               const char *role_name,
                               const char *ability_name)
{
    char global_name[256];

    if (ctx == NULL || role_name == NULL || ability_name == NULL)
        return NULL;
    if (!llvm_role_vtable_global_name(global_name, sizeof(global_name),
            role_name, ability_name)) {
        return NULL;
    }

    return LLVMGetNamedGlobal(ctx->module, global_name);
}

#endif /* PGY_LLVM_ENABLED */
