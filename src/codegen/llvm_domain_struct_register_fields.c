/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM domain struct field inventory registration.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_domain_struct_register_fields.h"

#include <stdio.h>

#include "llvm_domain_struct_fields.h"

static bool
llvm_domain_struct_register_field_name(char *out,
                                       size_t out_size,
                                       const char *kind,
                                       const char *name)
{
    int written;

    if (out == NULL || out_size == 0 || kind == NULL || name == NULL)
        return false;
    written = snprintf(out, out_size, "__%s_%s", kind, name);
    return written >= 0 && (size_t)written < out_size;
}

static bool
llvm_domain_struct_register_suffix_name(char *out,
                                        size_t out_size,
                                        const char *name,
                                        const char *suffix)
{
    int written;

    if (out == NULL || out_size == 0 || name == NULL || suffix == NULL)
        return false;
    written = snprintf(out, out_size, "%s_%s", name, suffix);
    return written >= 0 && (size_t)written < out_size;
}

static bool
llvm_domain_struct_add_generated_field(LLVMGenCtx *ctx,
                                       LLVMClassTypeEntry *entry,
                                       LLVMTypeRef field_type,
                                       int field_index,
                                       const char *kind,
                                       const char *name)
{
    char field_name[256];

    if (!llvm_domain_struct_register_field_name(field_name,
            sizeof(field_name), kind, name)) {
        llvm_set_error(ctx,
            "LLVM domain generated field name is too long for '%s'", name);
        return false;
    }
    llvm_class_add_field(entry, pergyra_strdup(field_name), field_type,
        field_index);
    return true;
}

static bool
llvm_domain_struct_add_suffixed_field(LLVMGenCtx *ctx,
                                      LLVMClassTypeEntry *entry,
                                      LLVMTypeRef field_type,
                                      int field_index,
                                      const char *name,
                                      const char *suffix)
{
    char field_name[256];

    if (!llvm_domain_struct_register_suffix_name(field_name,
            sizeof(field_name), name, suffix)) {
        llvm_set_error(ctx,
            "LLVM domain generated field name is too long for '%s'", name);
        return false;
    }
    llvm_class_add_field(entry, pergyra_strdup(field_name), field_type,
        field_index);
    return true;
}

static bool
llvm_domain_struct_register_zone_fields(LLVMGenCtx *ctx,
                                        ASTNode *stmt,
                                        LLVMClassTypeEntry *entry,
                                        LLVMTypeRef *ftypes)
{
    int field_index = 0;

    entry->domain_kind = LLVM_DOMAIN_ZONE;
    for (size_t j = 0; j < stmt->data.zone_decl.slot_count; j++, field_index++) {
        ASTNode *slot = stmt->data.zone_decl.slots[j];
        llvm_class_add_field_ex(entry, slot->data.domain_slot.slot_name,
            ftypes[field_index], field_index, slot->data.domain_slot.is_subject);
    }
    for (size_t j = 0; j < stmt->data.zone_decl.shared_count; j++, field_index++) {
        ASTNode *sf = stmt->data.zone_decl.shared_fields[j];
        llvm_class_add_field(entry, sf->data.party_shared.name,
            ftypes[field_index], field_index);
    }
    for (size_t j = 0; j < stmt->data.zone_decl.layer_slot_count; j++, field_index++) {
        ASTNode *slot = stmt->data.zone_decl.layer_slots[j];
        llvm_class_add_field(entry, slot->data.zone_layer_slot.slot_name,
            ftypes[field_index], field_index);
    }
    for (size_t j = 0; j < stmt->data.zone_decl.layer_slot_count; j++, field_index++) {
        ASTNode *slot = stmt->data.zone_decl.layer_slots[j];
        if (!llvm_domain_struct_add_generated_field(ctx, entry,
                ftypes[field_index], field_index, "layer_active",
                slot->data.zone_layer_slot.slot_name))
            return false;
    }
    for (size_t j = 0; j < stmt->data.zone_decl.layer_slot_count; j++, field_index++) {
        ASTNode *slot = stmt->data.zone_decl.layer_slots[j];
        if (!llvm_domain_struct_add_generated_field(ctx, entry,
                ftypes[field_index], field_index, "layer_epoch",
                slot->data.zone_layer_slot.slot_name))
            return false;
    }
    for (size_t j = 0; j < stmt->data.zone_decl.layer_slot_count; j++, field_index++) {
        ASTNode *slot = stmt->data.zone_decl.layer_slots[j];
        if (!llvm_domain_struct_add_generated_field(ctx, entry,
                ftypes[field_index], field_index, "layer_cause",
                slot->data.zone_layer_slot.slot_name))
            return false;
    }
    for (size_t j = 0; j < stmt->data.zone_decl.state_count; j++, field_index++) {
        ASTNode *state = stmt->data.zone_decl.states[j];
        if (!llvm_domain_struct_add_generated_field(ctx, entry,
                ftypes[field_index], field_index, "state",
                state->data.zone_state.state_name))
            return false;
    }
    for (size_t j = 0; j < stmt->data.zone_decl.state_count; j++, field_index++) {
        ASTNode *state = stmt->data.zone_decl.states[j];
        if (!llvm_domain_struct_add_generated_field(ctx, entry,
                ftypes[field_index], field_index, "state_epoch",
                state->data.zone_state.state_name))
            return false;
    }
    for (size_t j = 0; j < stmt->data.zone_decl.state_count; j++, field_index++) {
        ASTNode *state = stmt->data.zone_decl.states[j];
        if (!llvm_domain_struct_add_generated_field(ctx, entry,
                ftypes[field_index], field_index, "state_cause",
                state->data.zone_state.state_name))
            return false;
    }
    llvm_domain_add_projection_state_fields(ctx, entry, ftypes, &field_index,
        stmt->data.zone_decl.slots, stmt->data.zone_decl.slot_count,
        stmt->data.zone_decl.refreshes, stmt->data.zone_decl.refresh_count);
    llvm_class_add_field(entry, pergyra_strdup("__sync_generation"),
        ftypes[field_index], field_index);
    return true;
}

static void
llvm_domain_struct_register_roster_fields(ASTNode *stmt,
                                          LLVMClassTypeEntry *entry,
                                          LLVMTypeRef *ftypes)
{
    int field_index = 0;

    entry->domain_kind = LLVM_DOMAIN_SYSTEMIC;
    for (size_t j = 0; j < ast_roster_party_count(stmt); j++, field_index++) {
        ASTNode *slot = ast_roster_party(stmt, j);
        llvm_class_add_field(entry, slot->data.roster_slot.slot_name,
            ftypes[field_index], field_index);
    }
    for (size_t j = 0; j < ast_roster_shared_count(stmt); j++, field_index++) {
        ASTNode *sf = ast_roster_shared(stmt, j);
        llvm_class_add_field(entry, sf->data.party_shared.name,
            ftypes[field_index], field_index);
    }
}

static bool
llvm_domain_struct_register_world_fields(LLVMGenCtx *ctx,
                                         ASTNode *stmt,
                                         LLVMClassTypeEntry *entry,
                                         LLVMTypeRef *ftypes)
{
    int field_index = 0;

    entry->domain_kind = LLVM_DOMAIN_WORLD;
    for (size_t j = 0; j < stmt->data.world_decl.roster_count; j++, field_index++) {
        ASTNode *ws = stmt->data.world_decl.rosters[j];
        llvm_class_add_field(entry, ws->data.world_roster.slot_name,
            ftypes[field_index], field_index);
    }
    for (size_t j = 0; j < stmt->data.world_decl.zone_count; j++, field_index++) {
        ASTNode *wz = stmt->data.world_decl.zones[j];
        llvm_class_add_field(entry, wz->data.world_zone.slot_name,
            ftypes[field_index], field_index);
    }
    for (size_t j = 0; j < stmt->data.world_decl.shared_count; j++, field_index++) {
        ASTNode *sf = stmt->data.world_decl.shared_fields[j];
        llvm_class_add_field(entry, sf->data.party_shared.name,
            ftypes[field_index], field_index);
    }
    for (size_t j = 0; j < stmt->data.world_decl.zone_count; j++, field_index++) {
        ASTNode *wz = stmt->data.world_decl.zones[j];
        if (!llvm_domain_struct_add_generated_field(ctx, entry,
                ftypes[field_index], field_index, "zone_active",
                wz->data.world_zone.slot_name))
            return false;
    }
    for (size_t j = 0; j < stmt->data.world_decl.zone_count; j++, field_index++) {
        ASTNode *wz = stmt->data.world_decl.zones[j];
        if (!llvm_domain_struct_add_generated_field(ctx, entry,
                ftypes[field_index], field_index, "zone_dirty",
                wz->data.world_zone.slot_name))
            return false;
    }
    for (size_t j = 0; j < stmt->data.world_decl.zone_count; j++, field_index++) {
        ASTNode *wz = stmt->data.world_decl.zones[j];
        if (!llvm_domain_struct_add_generated_field(ctx, entry,
                ftypes[field_index], field_index, "zone_seen_generation",
                wz->data.world_zone.slot_name))
            return false;
    }
    for (size_t j = 0; j < stmt->data.world_decl.state_count; j++, field_index++) {
        ASTNode *state = stmt->data.world_decl.states[j];
        if (!llvm_domain_struct_add_generated_field(ctx, entry,
                ftypes[field_index], field_index, "zone_state",
                state->data.world_state.state_name))
            return false;
    }
    for (size_t j = 0; j < stmt->data.world_decl.zone_count; j++, field_index++) {
        ASTNode *wz = stmt->data.world_decl.zones[j];
        if (!llvm_domain_struct_add_generated_field(ctx, entry,
                ftypes[field_index], field_index, "zone_epoch",
                wz->data.world_zone.slot_name))
            return false;
    }
    for (size_t j = 0; j < stmt->data.world_decl.zone_count; j++, field_index++) {
        ASTNode *wz = stmt->data.world_decl.zones[j];
        if (!llvm_domain_struct_add_generated_field(ctx, entry,
                ftypes[field_index], field_index, "zone_cause",
                wz->data.world_zone.slot_name))
            return false;
    }
    for (size_t j = 0; j < stmt->data.world_decl.state_count; j++, field_index++) {
        ASTNode *state = stmt->data.world_decl.states[j];
        if (!llvm_domain_struct_add_generated_field(ctx, entry,
                ftypes[field_index], field_index, "zone_state_epoch",
                state->data.world_state.state_name))
            return false;
    }
    for (size_t j = 0; j < stmt->data.world_decl.state_count; j++, field_index++) {
        ASTNode *state = stmt->data.world_decl.states[j];
        if (!llvm_domain_struct_add_generated_field(ctx, entry,
                ftypes[field_index], field_index, "zone_state_cause",
                state->data.world_state.state_name))
            return false;
    }
    llvm_class_add_field(entry, pergyra_strdup("__world_derived_dirty"),
        ftypes[field_index], field_index);
    return true;
}

static bool
llvm_domain_struct_register_default_fields(LLVMGenCtx *ctx,
                                           ASTNode *stmt,
                                           LLVMClassTypeEntry *entry,
                                           LLVMTypeRef *ftypes,
                                           ASTNode **slots,
                                           size_t slot_count,
                                           ASTNode **shared_fields,
                                           size_t shared_count,
                                           ASTNode **refreshes,
                                           size_t refresh_count)
{
    int field_index = 0;

    if (stmt->type == AST_RELATION_DECL || stmt->type == AST_EFFECT_DECL)
        entry->domain_kind = LLVM_DOMAIN_PROJECTION;
    for (size_t j = 0; j < slot_count; j++, field_index++) {
        ASTNode *slot = slots[j];
        llvm_class_add_field(entry, slot->data.domain_slot.slot_name,
            ftypes[field_index], field_index);
    }
    for (size_t j = 0; j < shared_count; j++, field_index++) {
        ASTNode *sf = shared_fields[j];
        llvm_class_add_field(entry, sf->data.party_shared.name,
            ftypes[field_index], field_index);
    }
    for (size_t j = 0; j < ast_party_role_count(stmt); j++) {
        ASTNode *rs = ast_party_role(stmt, j);
        if (rs == NULL || rs->type != AST_ROLE_SLOT
            || !rs->data.role_slot.is_dynamic)
            continue;
        if (!llvm_domain_struct_add_suffixed_field(ctx, entry, ctx->type_i8ptr,
                field_index, rs->data.role_slot.slot_name, "vtable"))
            return false;
        field_index++;
    }
    if (stmt->type == AST_RELATION_DECL || stmt->type == AST_EFFECT_DECL) {
        llvm_domain_add_projection_state_fields(ctx, entry, ftypes,
            &field_index, slots, slot_count, refreshes, refresh_count);
    }
    return true;
}

bool
llvm_domain_struct_register_fields(LLVMGenCtx *ctx,
                                   ASTNode *stmt,
                                   LLVMClassTypeEntry *entry,
                                   LLVMTypeRef *ftypes,
                                   ASTNode **slots,
                                   size_t slot_count,
                                   ASTNode **shared_fields,
                                   size_t shared_count,
                                   ASTNode **refreshes,
                                   size_t refresh_count)
{
    if (ctx == NULL || stmt == NULL || entry == NULL || ftypes == NULL)
        return false;
    if (stmt->type == AST_ZONE_DECL)
        return llvm_domain_struct_register_zone_fields(ctx, stmt, entry, ftypes);
    if (stmt->type == AST_ROSTER_DECL) {
        llvm_domain_struct_register_roster_fields(stmt, entry, ftypes);
        return true;
    }
    if (stmt->type == AST_WORLD_DECL)
        return llvm_domain_struct_register_world_fields(ctx, stmt, entry, ftypes);
    return llvm_domain_struct_register_default_fields(ctx, stmt, entry, ftypes,
        slots, slot_count, shared_fields, shared_count, refreshes,
        refresh_count);
}

#endif /* PGY_LLVM_ENABLED */
