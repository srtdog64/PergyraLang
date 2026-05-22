/*
 * Copyright (c) 2026 Pergyra Language Project
 * LLVM domain lookup helpers.
 */

#ifdef PGY_LLVM_ENABLED

#include "host_decl_compat.h"
#include "llvm_internal.h"
#include "parser/ast_api.h"

ASTNode *
llvm_find_named_domain_decl(LLVMGenCtx *ctx, ASTNodeType decl_type,
                            const char *name)
{
    if (ctx == NULL || name == NULL)
        return NULL;
    return llvm_find_decl_in_active_inventory(ctx, decl_type, name);
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

ASTNode *
llvm_find_nominal_host_method_decl(LLVMGenCtx *ctx, const char *host_type_name,
                                   const char *method_name)
{
    return llvm_find_host_method_decl_in_context(ctx, host_type_name,
                                                method_name);
}

ASTNode *
llvm_find_zone_state_decl(LLVMGenCtx *ctx, ASTNode *zone_decl,
                          const char *state_name)
{
    size_t state_count = 0;
    ASTNode **states;

    (void)ctx;
    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL
        || state_name == NULL)
        return NULL;
    states = ast_zone_states(zone_decl, &state_count);
    for (size_t i = 0; i < state_count; i++) {
        ASTNode *state = states[i];
        if (state != NULL && state->type == AST_ZONE_STATE
            && ast_zone_state_name(state) != NULL
            && strcmp(ast_zone_state_name(state), state_name) == 0)
            return state;
    }
    return NULL;
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

static ASTNode *
llvm_find_world_zone_slot_decl(ASTNode *world_decl, const char *slot_name)
{
    size_t zone_count = 0;
    ASTNode **zones;

    if (world_decl == NULL || world_decl->type != AST_WORLD_DECL
        || slot_name == NULL)
        return NULL;
    zones = ast_world_zones(world_decl, &zone_count);
    for (size_t i = 0; i < zone_count; i++) {
        ASTNode *zone = zones[i];
        const char *zone_slot_name = ast_world_zone_slot_name(zone);
        if (zone_slot_name != NULL && strcmp(zone_slot_name, slot_name) == 0)
            return zone;
    }
    return NULL;
}

ASTNode *
llvm_resolve_world_zone_decl(LLVMGenCtx *ctx, ASTNode *world_decl,
                             const char *slot_name)
{
    ASTNode *zone_slot = llvm_find_world_zone_slot_decl(world_decl, slot_name);
    const char *zone_type_name = ast_world_zone_type_name(zone_slot);
    if (ctx == NULL || zone_type_name == NULL)
        return NULL;
    return llvm_find_named_domain_decl(ctx, AST_ZONE_DECL,
        zone_type_name);
}

ASTNode *
llvm_find_zone_domain_slot_decl(ASTNode *zone_decl, const char *slot_name)
{
    size_t slot_count = 0;
    ASTNode **slots;

    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL
        || slot_name == NULL)
        return NULL;
    slots = ast_zone_slots(zone_decl, &slot_count);
    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        const char *candidate_name = ast_domain_slot_name(slot);
        if (slot != NULL && slot->type == AST_DOMAIN_SLOT
            && candidate_name != NULL
            && strcmp(candidate_name, slot_name) == 0)
            return slot;
    }
    return NULL;
}

ASTNode *
llvm_find_zone_layer_slot_decl(ASTNode *zone_decl, const char *slot_name)
{
    size_t layer_slot_count = 0;
    ASTNode **layer_slots;

    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL
        || slot_name == NULL)
        return NULL;
    layer_slots = ast_zone_layer_slots(zone_decl, &layer_slot_count);
    for (size_t i = 0; i < layer_slot_count; i++) {
        ASTNode *slot = layer_slots[i];
        if (slot != NULL && slot->type == AST_ZONE_LAYER_SLOT
            && ast_zone_layer_slot_name(slot) != NULL
            && strcmp(ast_zone_layer_slot_name(slot), slot_name) == 0)
            return slot;
    }
    return NULL;
}

bool
llvm_world_has_zone_slot(ASTNode *world_decl, const char *slot_name)
{
    return llvm_find_world_zone_slot_decl(world_decl, slot_name) != NULL;
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

static ASTNode *
llvm_find_projection_class_decl(LLVMGenCtx *ctx, const char *name)
{
    if (ctx == NULL || name == NULL)
        return NULL;
    return llvm_find_decl_in_active_inventory(ctx, AST_CLASS_DECL, name);
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
        return mir_decl->uses_pointer_self;

    host_decl = llvm_find_host_decl_in_active_inventory(ctx, type_name);
    if (host_decl != NULL)
        return llvm_host_decl_uses_pointer_self(host_decl);

    {
        ASTNode *stmt = llvm_find_projection_class_decl(ctx, type_name);
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
        parent_cls->fields[field_idx].field_type);
    if (field_cls != NULL)
        return field_cls->class_name;

    host_decl = llvm_find_projection_class_decl(ctx, host_name);
    if (host_decl == NULL || host_decl->type != AST_CLASS_DECL)
        return NULL;

    size_t field_count = 0;
    ClassField **fields = ast_class_fields(host_decl, &field_count);
    for (size_t i = 0; i < field_count; i++) {
        ClassField *field = fields != NULL ? fields[i] : NULL;
        const char *field_type_name = field != NULL ? ast_type_name(field->type) : NULL;
        if (field == NULL || field->name == NULL
            || strcmp(field->name, field_name) != 0
            || field_type_name == NULL)
            continue;
        if (llvm_lookup_class(ctx, field_type_name) != NULL)
            return field_type_name;
    }

    return NULL;
}

#endif /* PGY_LLVM_ENABLED */
