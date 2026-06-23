/*
 * Copyright (c) 2026 Pergyra Language Project
 * LLVM domain lookup helpers.
 */

#ifdef PGY_LLVM_ENABLED

#include "../compiler/mir_decl_headers.h"
#include "host_decl_compat.h"
#include "llvm_backend_generic.h"
#include "llvm_domain_lookup.h"
#include "llvm_internal.h"
#include "llvm_inventory_decl_lookup.h"
#include "llvm_inventory_internal.h"
#include "parser/ast_api.h"

static ASTNode *
llvm_find_extern_function_decl(LLVMGenCtx *ctx, const char *function_name)
{
    ASTNode **externs = NULL;
    size_t extern_count = 0;

    if (ctx == NULL || function_name == NULL)
        return NULL;

    llvm_active_externs(ctx, &externs, &extern_count);
    for (size_t i = 0; externs != NULL && i < extern_count; i++) {
        ASTNode *block = externs[i];
        size_t block_decl_count = 0;
        if (block == NULL || block->type != AST_EXTERN_BLOCK)
            continue;
        (void)ast_extern_block_declarations(block, &block_decl_count);
        for (size_t j = 0; j < block_decl_count; j++) {
            ASTNode *decl = ast_extern_block_declaration(block, j);
            const char *decl_name = ast_declaration_name(decl);
            if (decl != NULL && decl->type == AST_FUNC_DECL
                && decl_name != NULL
                && strcmp(decl_name, function_name) == 0) {
                return decl;
            }
        }
    }
    return NULL;
}

ASTNode *
llvm_find_named_domain_decl(LLVMGenCtx *ctx, ASTNodeType decl_type,
                            const char *name)
{
    if (ctx == NULL || name == NULL)
        return NULL;
    return llvm_find_decl_in_active_inventory(ctx, decl_type, name);
}

bool
llvm_named_domain_decl_exists(LLVMGenCtx *ctx, ASTNodeType decl_type,
                              const char *name)
{
    if (ctx == NULL || name == NULL)
        return false;
    return llvm_decl_exists_in_context(ctx, decl_type, name);
}

ASTNode *
llvm_find_domain_constructor_decl(LLVMGenCtx *ctx, const char *name)
{
    const ASTNodeType *constructor_types = NULL;
    size_t constructor_type_count = 0;

    if (ctx == NULL || name == NULL)
        return NULL;

    constructor_types =
        pgy_host_decl_compat_constructor_domain_types(
            &constructor_type_count);
    for (size_t i = 0; constructor_types != NULL
         && i < constructor_type_count; i++) {
        ASTNode *decl = llvm_find_named_domain_decl(
            ctx, constructor_types[i], name);
        if (decl != NULL)
            return decl;
    }
    return NULL;
}

bool
llvm_domain_constructor_decl_exists(LLVMGenCtx *ctx, const char *name)
{
    const ASTNodeType *constructor_types = NULL;
    size_t constructor_type_count = 0;

    if (ctx == NULL || name == NULL)
        return false;

    constructor_types =
        pgy_host_decl_compat_constructor_domain_types(
            &constructor_type_count);
    for (size_t i = 0; constructor_types != NULL
         && i < constructor_type_count; i++) {
        if (llvm_named_domain_decl_exists(
                ctx, constructor_types[i], name)) {
            return true;
        }
    }
    return false;
}

ASTNode *
llvm_find_function_decl(LLVMGenCtx *ctx, const char *name)
{
    ASTNode *decl;

    if (ctx == NULL || name == NULL)
        return NULL;
    decl = llvm_find_decl_in_active_inventory(ctx, AST_FUNC_DECL, name);
    if (decl != NULL)
        return decl;
    decl = llvm_find_extern_function_decl(ctx, name);
    if (decl != NULL)
        return decl;
    return llvm_lookup_generic_template(ctx, name);
}

bool
llvm_function_decl_exists(LLVMGenCtx *ctx, const char *name)
{
    if (ctx == NULL || name == NULL)
        return false;
    if (llvm_decl_exists_in_context(ctx, AST_FUNC_DECL, name))
        return true;
    if (llvm_find_extern_function_decl(ctx, name) != NULL)
        return true;
    return llvm_lookup_generic_template(ctx, name) != NULL;
}

bool
llvm_decl_is_extern_function(LLVMGenCtx *ctx, const ASTNode *decl)
{
    ASTNode **externs = NULL;
    size_t extern_count = 0;

    if (ctx == NULL || decl == NULL || decl->type != AST_FUNC_DECL)
        return false;

    llvm_active_externs(ctx, &externs, &extern_count);
    for (size_t i = 0; externs != NULL && i < extern_count; i++) {
        ASTNode *block = externs[i];
        size_t block_decl_count = 0;
        if (block == NULL || block->type != AST_EXTERN_BLOCK)
            continue;
        (void)ast_extern_block_declarations(block, &block_decl_count);
        for (size_t j = 0; j < block_decl_count; j++) {
            if (ast_extern_block_declaration(block, j) == decl)
                return true;
        }
    }
    return false;
}

ASTNode *
llvm_find_intent_decl(LLVMGenCtx *ctx, const char *name)
{
    if (ctx == NULL || name == NULL)
        return NULL;
    return llvm_find_decl_in_active_inventory(ctx, AST_INTENT_DECL, name);
}

bool
llvm_intent_decl_exists(LLVMGenCtx *ctx, const char *name)
{
    if (ctx == NULL || name == NULL)
        return false;
    return llvm_decl_exists_in_context(ctx, AST_INTENT_DECL, name);
}

ASTNode *
llvm_find_callable_decl(LLVMGenCtx *ctx, const char *name)
{
    ASTNode *decl = llvm_find_function_decl(ctx, name);
    if (decl != NULL)
        return decl;
    return llvm_find_intent_decl(ctx, name);
}

bool
llvm_callable_decl_exists(LLVMGenCtx *ctx, const char *name)
{
    if (ctx == NULL || name == NULL)
        return false;
    return llvm_function_decl_exists(ctx, name)
        || llvm_intent_decl_exists(ctx, name);
}


bool
llvm_zone_has_state(LLVMGenCtx *ctx, ASTNode *zone_decl,
                    const char *state_name)
{
    const char *zone_name;
    LLVMHostedZoneStateView state_view;

    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL
        || state_name == NULL)
        return false;

    zone_name = llvm_decl_node_name(zone_decl);
    state_view =
        llvm_hosted_zone_state_view_from_decl(ctx, zone_name, zone_decl);
    if (llvm_hosted_zone_state_view_missing_mir_metadata(&state_view)) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing zone state lookup metadata for '%s'",
            zone_name != NULL ? zone_name : "<anonymous>");
        return false;
    }

    for (size_t i = 0; i < state_view.count; i++) {
        const char *candidate =
            llvm_hosted_zone_state_view_name(&state_view, i);
        if (candidate != NULL && strcmp(candidate, state_name) == 0)
            return true;
    }
    return false;
}

ASTNode *
llvm_find_world_state_decl(LLVMGenCtx *ctx, ASTNode *world_decl,
                           const char *state_name)
{
    size_t state_count = 0;
    ASTNode **states;

    (void)ctx;
    if (world_decl == NULL || world_decl->type != AST_WORLD_DECL
        || state_name == NULL)
        return NULL;
    states = ast_world_states(world_decl, &state_count);
    for (size_t i = 0; i < state_count; i++) {
        ASTNode *state = states[i];
        if (state != NULL && state->type == AST_WORLD_STATE
            && ast_world_state_name(state) != NULL
            && strcmp(ast_world_state_name(state), state_name) == 0)
            return state;
    }
    return NULL;
}

static const char *
llvm_world_zone_slot_type_name(LLVMGenCtx *ctx, ASTNode *world_decl,
                               const char *slot_name)
{
    const char *world_name;
    LLVMHostedWorldZoneSlotView zone_view;

    if (world_decl == NULL || world_decl->type != AST_WORLD_DECL
        || slot_name == NULL)
        return NULL;
    world_name = llvm_decl_node_name(world_decl);
    zone_view = llvm_hosted_world_zone_slot_view_from_decl(ctx, world_name,
        world_decl);
    if (llvm_hosted_world_zone_slot_view_missing_mir_metadata(&zone_view)) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing world zone-slot type metadata for '%s'",
            world_name != NULL ? world_name : "<anonymous>");
        return NULL;
    }
    for (size_t i = 0; i < zone_view.count; i++) {
        const char *zone_slot_name =
            llvm_hosted_world_zone_slot_view_name(&zone_view, i);
        if (zone_slot_name != NULL && strcmp(zone_slot_name, slot_name) == 0) {
            return llvm_hosted_world_zone_slot_view_type_name(&zone_view, i);
        }
    }
    return NULL;
}

ASTNode *
llvm_resolve_world_zone_decl(LLVMGenCtx *ctx, ASTNode *world_decl,
                             const char *slot_name)
{
    const char *zone_type_name = llvm_world_zone_slot_type_name(ctx, world_decl,
        slot_name);
    if (ctx == NULL || zone_type_name == NULL)
        return NULL;
    return llvm_find_named_domain_decl(ctx, AST_ZONE_DECL,
        zone_type_name);
}

bool
llvm_zone_has_domain_slot(LLVMGenCtx *ctx,
                          ASTNode *zone_decl,
                          const char *slot_name)
{
    const char *zone_name;
    LLVMHostedDomainSlotView slot_view;

    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL
        || slot_name == NULL)
        return false;
    zone_name = llvm_decl_node_name(zone_decl);
    slot_view = llvm_hosted_domain_slot_view_from_decl(ctx, zone_name,
        zone_decl);
    if (llvm_hosted_domain_slot_view_missing_mir_metadata(&slot_view)) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing zone domain-slot lookup metadata for '%s'",
            zone_name != NULL ? zone_name : "<anonymous>");
        return false;
    }
    for (size_t i = 0; i < slot_view.count; i++) {
        const char *candidate_name =
            llvm_hosted_domain_slot_view_name(&slot_view, i);
        if (candidate_name != NULL && strcmp(candidate_name, slot_name) == 0)
            return true;
    }
    return false;
}

bool
llvm_zone_has_layer_slot(LLVMGenCtx *ctx, ASTNode *zone_decl,
                         const char *slot_name)
{
    const char *zone_name;
    LLVMHostedZoneLayerSlotView layer_view;

    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL
        || slot_name == NULL)
        return false;

    zone_name = llvm_decl_node_name(zone_decl);
    layer_view =
        llvm_hosted_zone_layer_slot_view_from_decl(ctx, zone_name, zone_decl);
    if (llvm_hosted_zone_layer_slot_view_missing_mir_metadata(&layer_view)) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing zone layer-slot metadata for '%s'",
            zone_name != NULL ? zone_name : "<anonymous>");
        return false;
    }

    for (size_t i = 0; i < layer_view.count; i++) {
        const char *candidate_name =
            llvm_hosted_zone_layer_slot_view_name(&layer_view, i);
        if (candidate_name != NULL && strcmp(candidate_name, slot_name) == 0) {
            return true;
        }
    }
    return false;
}

bool
llvm_world_has_zone_slot(LLVMGenCtx *ctx, ASTNode *world_decl,
                         const char *slot_name)
{
    return llvm_world_zone_slot_type_name(ctx, world_decl, slot_name) != NULL;
}

const char *
llvm_current_zone_slot_type_name(LLVMGenCtx *ctx, const char *slot_name)
{
    ASTNode *zone_decl;
    const char *zone_name;
    LLVMHostedDomainSlotView slot_view;

    if (ctx == NULL || slot_name == NULL)
        return NULL;
    zone_decl = llvm_current_host_decl(ctx);
    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL)
        return NULL;

    zone_name = llvm_decl_node_name(zone_decl);
    slot_view = llvm_hosted_domain_slot_view_from_decl(ctx, zone_name,
        zone_decl);
    if (llvm_hosted_domain_slot_view_missing_mir_metadata(&slot_view))
        return NULL;

    for (size_t i = 0; i < slot_view.count; i++) {
        const char *candidate_name =
            llvm_hosted_domain_slot_view_name(&slot_view, i);
        if (candidate_name != NULL
            && strcmp(candidate_name, slot_name) == 0) {
            return llvm_hosted_domain_slot_view_type_name(&slot_view, i);
        }
    }
    return NULL;
}

const char *
llvm_current_host_class_name(LLVMGenCtx *ctx)
{
    ASTNode *decl = NULL;

    if (ctx == NULL)
        return NULL;

    decl = llvm_current_host_decl(ctx);
    if (decl == NULL)
        return NULL;

    return llvm_is_host_decl_type(decl->type)
        ? llvm_decl_node_name(decl)
        : NULL;
}

ASTNode *
llvm_find_projection_nominal_decl(LLVMGenCtx *ctx, const char *name)
{
    if (ctx == NULL || name == NULL)
        return NULL;
    return llvm_find_decl_in_active_inventory(ctx, AST_CLASS_DECL, name);
}

bool
llvm_projection_nominal_decl_exists(LLVMGenCtx *ctx, const char *name)
{
    return llvm_decl_exists_in_context(ctx, AST_CLASS_DECL, name);
}

static bool
llvm_host_decl_uses_pointer_self(ASTNode *decl)
{
    return pgy_host_decl_compat_uses_pointer_self(decl);
}

bool
llvm_type_name_uses_pointer_self(LLVMGenCtx *ctx, const char *type_name)
{
    const MIRDeclHeader *mir_decl;
    ASTNode *host_decl;

    if (ctx == NULL || type_name == NULL)
        return false;

    {
        LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, type_name);
        if (cls != NULL && cls->is_pointer_self_host)
            return true;
    }

    mir_decl = llvm_find_host_decl_header_in_context(ctx, type_name);
    if (mir_decl != NULL)
        return mir_decl_header_uses_pointer_self(mir_decl);
    if (llvm_active_has_mir(ctx))
        return false;

    host_decl = llvm_find_host_decl_in_active_inventory(ctx, type_name);
    if (host_decl != NULL)
        return llvm_host_decl_uses_pointer_self(host_decl);

    {
        ASTNode *stmt = llvm_find_projection_nominal_decl(ctx, type_name);
        return stmt != NULL && stmt->type == AST_CLASS_DECL
            && ast_class_nominal_kind(stmt) == NOMINAL_DECL_VESSEL;
    }
}

bool
llvm_ast_type_uses_pointer_self(LLVMGenCtx *ctx, ASTNode *type_node)
{
    if (ctx == NULL || type_node == NULL
        || type_node->type != AST_TYPE
        || ast_type_name(type_node) == NULL) {
        return false;
    }
    return llvm_type_name_uses_pointer_self(ctx, ast_type_name(type_node));
}

const char *
llvm_current_field_class_name(LLVMGenCtx *ctx, const char *field_name)
{
    LLVMClassTypeEntry *parent_cls;
    LLVMClassTypeEntry *field_cls;
    ASTNode *host_decl;
    const MIRDeclField *mir_field;
    int field_idx;
    const char *host_name;

    host_name = llvm_current_host_class_name(ctx);
    if (ctx == NULL || host_name == NULL || field_name == NULL)
        return NULL;

    parent_cls = llvm_lookup_class(ctx, host_name);
    if (parent_cls == NULL)
        return NULL;

    field_idx = llvm_class_field_index(parent_cls, field_name);
    if (field_idx < 0)
        return NULL;

    field_cls = llvm_lookup_class_by_type(ctx,
        llvm_class_field_type_at_index(parent_cls, field_idx));
    if (field_cls != NULL)
        return field_cls->class_name;

    mir_field = llvm_find_decl_field_in_context(ctx, host_name, field_name);
    {
        const char *field_type_name = llvm_mir_decl_field_type_name(mir_field);
        ASTNode *field_type = llvm_mir_decl_field_type(mir_field);
        if (field_type_name == NULL && field_type != NULL)
            field_type_name = ast_type_name(field_type);
        if (field_type_name != NULL
            && llvm_lookup_class(ctx, field_type_name) != NULL)
            return field_type_name;
    }

    host_decl = llvm_find_projection_nominal_decl(ctx, host_name);
    if (host_decl == NULL || host_decl->type != AST_CLASS_DECL)
        return NULL;

    {
        LLVMHostedFieldView field_view =
            llvm_hosted_class_field_view_from_decl(ctx, host_name, host_decl);
        size_t field_index = 0;
        const char *field_type_name = NULL;
        if (llvm_hosted_field_view_missing_mir_metadata(&field_view)) {
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path missing current-field class metadata for '%s'",
                host_name);
            return NULL;
        }
        if (llvm_hosted_field_view_find_index(
                &field_view, field_name, &field_index)) {
            ASTNode *field_type =
                llvm_hosted_field_view_type(&field_view, field_index);
            field_type_name = field_type != NULL ? ast_type_name(field_type) : NULL;
        }
        if (field_type_name != NULL
            && llvm_lookup_class(ctx, field_type_name) != NULL)
            return field_type_name;
    }

    return NULL;
}

#endif /* PGY_LLVM_ENABLED */
