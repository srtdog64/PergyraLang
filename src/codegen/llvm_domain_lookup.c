/*
 * Copyright (c) 2026 Pergyra Language Project
 * LLVM domain lookup helpers.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

ASTNode *
llvm_find_named_domain_decl(LLVMGenCtx *ctx, ASTNodeType decl_type,
                            const char *name)
{
    if (ctx == NULL || name == NULL)
        return NULL;
    return llvm_find_decl_in_active_inventory(ctx, decl_type, name);
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
    (void)ctx;
    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL
        || state_name == NULL)
        return NULL;
    for (size_t i = 0; i < zone_decl->data.zone_decl.state_count; i++) {
        ASTNode *state = zone_decl->data.zone_decl.states[i];
        if (state != NULL && state->type == AST_ZONE_STATE
            && state->data.zone_state.state_name != NULL
            && strcmp(state->data.zone_state.state_name, state_name) == 0)
            return state;
    }
    return NULL;
}

ASTNode *
llvm_find_world_state_decl(LLVMGenCtx *ctx, ASTNode *world_decl,
                           const char *state_name)
{
    (void)ctx;
    if (world_decl == NULL || world_decl->type != AST_WORLD_DECL
        || state_name == NULL)
        return NULL;
    for (size_t i = 0; i < world_decl->data.world_decl.state_count; i++) {
        ASTNode *state = world_decl->data.world_decl.states[i];
        if (state != NULL && state->type == AST_WORLD_STATE
            && state->data.world_state.state_name != NULL
            && strcmp(state->data.world_state.state_name, state_name) == 0)
            return state;
    }
    return NULL;
}

static ASTNode *
llvm_find_world_zone_slot_decl(ASTNode *world_decl, const char *slot_name)
{
    if (world_decl == NULL || world_decl->type != AST_WORLD_DECL
        || slot_name == NULL)
        return NULL;
    for (size_t i = 0; i < world_decl->data.world_decl.zone_count; i++) {
        ASTNode *zone = world_decl->data.world_decl.zones[i];
        if (zone != NULL && zone->type == AST_WORLD_ZONE
            && zone->data.world_zone.slot_name != NULL
            && strcmp(zone->data.world_zone.slot_name, slot_name) == 0)
            return zone;
    }
    return NULL;
}

ASTNode *
llvm_resolve_world_zone_decl(LLVMGenCtx *ctx, ASTNode *world_decl,
                             const char *slot_name)
{
    ASTNode *zone_slot = llvm_find_world_zone_slot_decl(world_decl, slot_name);
    if (ctx == NULL || zone_slot == NULL
        || zone_slot->data.world_zone.zone_type == NULL)
        return NULL;
    return llvm_find_named_domain_decl(ctx, AST_ZONE_DECL,
        zone_slot->data.world_zone.zone_type);
}

ASTNode *
llvm_find_zone_domain_slot_decl(ASTNode *zone_decl, const char *slot_name)
{
    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL
        || slot_name == NULL)
        return NULL;
    for (size_t i = 0; i < zone_decl->data.zone_decl.slot_count; i++) {
        ASTNode *slot = zone_decl->data.zone_decl.slots[i];
        if (slot != NULL && slot->type == AST_DOMAIN_SLOT
            && slot->data.domain_slot.slot_name != NULL
            && strcmp(slot->data.domain_slot.slot_name, slot_name) == 0)
            return slot;
    }
    return NULL;
}

ASTNode *
llvm_find_zone_layer_slot_decl(ASTNode *zone_decl, const char *slot_name)
{
    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL
        || slot_name == NULL)
        return NULL;
    for (size_t i = 0; i < zone_decl->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = zone_decl->data.zone_decl.layer_slots[i];
        if (slot != NULL && slot->type == AST_ZONE_LAYER_SLOT
            && slot->data.zone_layer_slot.slot_name != NULL
            && strcmp(slot->data.zone_layer_slot.slot_name, slot_name) == 0)
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

    switch (decl->type) {
    case AST_CLASS_DECL:
        return decl->data.class_decl.name;
    case AST_ENUM_DECL:
        return decl->data.enum_decl.name;
    case AST_PARTY_DECL:
        return decl->data.party_decl.name;
    case AST_ROLE_DECL:
        return decl->data.role_decl.name;
    case AST_ROSTER_DECL:
        return decl->data.roster_decl.name;
    case AST_RELATION_DECL:
        return decl->data.relation_decl.name;
    case AST_EFFECT_DECL:
        return decl->data.effect_decl.name;
    case AST_ZONE_DECL:
        return decl->data.zone_decl.name;
    case AST_WORLD_DECL:
        return decl->data.world_decl.name;
    default:
        return NULL;
    }
}

static ASTNode *
llvm_find_projection_class_decl(LLVMGenCtx *ctx, const char *name)
{
    if (ctx == NULL || name == NULL)
        return NULL;
    return llvm_find_decl_in_active_inventory(ctx, AST_CLASS_DECL, name);
}

bool
llvm_type_name_uses_pointer_self(LLVMGenCtx *ctx, const char *type_name)
{
    const MIRDeclHeader *mir_decl;

    if (ctx == NULL || type_name == NULL)
        return false;

    {
        LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, type_name);
        if (cls != NULL && cls->is_pointer_self_host)
            return true;
    }

    mir_decl = ctx->mir != NULL ? mir_find_decl_header(ctx->mir, type_name) : NULL;
    if (mir_decl != NULL)
        return mir_decl->uses_pointer_self;

    if (llvm_find_named_domain_decl(ctx, AST_PARTY_DECL, type_name) != NULL
        || llvm_find_named_domain_decl(ctx, AST_ROSTER_DECL, type_name) != NULL
        || llvm_find_named_domain_decl(ctx, AST_ROLE_DECL, type_name) != NULL
        || llvm_find_named_domain_decl(ctx, AST_WORLD_DECL, type_name) != NULL
        || llvm_find_named_domain_decl(ctx, AST_RELATION_DECL, type_name) != NULL
        || llvm_find_named_domain_decl(ctx, AST_EFFECT_DECL, type_name) != NULL
        || llvm_find_named_domain_decl(ctx, AST_ZONE_DECL, type_name) != NULL)
        return true;

    {
        ASTNode *stmt = llvm_find_projection_class_decl(ctx, type_name);
        return stmt != NULL && stmt->type == AST_CLASS_DECL
            && stmt->data.class_decl.nominal_kind == NOMINAL_DECL_VESSEL;
    }
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

    for (size_t i = 0; i < host_decl->data.class_decl.field_count; i++) {
        ClassField *field = host_decl->data.class_decl.fields[i];
        if (field == NULL || field->name == NULL
            || strcmp(field->name, field_name) != 0
            || field->type == NULL || field->type->type != AST_TYPE
            || field->type->data.type.name == NULL)
            continue;
        if (llvm_lookup_class(ctx, field->type->data.type.name) != NULL)
            return field->type->data.type.name;
    }

    return NULL;
}

#endif /* PGY_LLVM_ENABLED */
