/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM domain struct field inventory registration.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_domain_struct_register_fields.h"

#include <stdio.h>

#include "../parser/ast_api.h"
#include "llvm_domain_struct_fields.h"
#include "llvm_inventory_decl_lookup.h"

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
llvm_domain_struct_register_shared_field_names(
    LLVMGenCtx *ctx,
    LLVMClassTypeEntry *entry,
    LLVMTypeRef *ftypes,
    int *field_index,
    const LLVMHostedSharedFieldView *shared_view,
    const char *context)
{
    if (ctx == NULL || entry == NULL || ftypes == NULL || field_index == NULL
        || shared_view == NULL) {
        return false;
    }

    for (size_t j = 0; j < shared_view->count; j++) {
        const char *field_name =
            llvm_hosted_shared_field_view_name(shared_view, j);
        if (field_name == NULL) {
            llvm_set_error(ctx,
                "LLVM domain %s shared field missing MIR field name",
                context != NULL ? context : "declaration");
            return false;
        }
        llvm_class_add_field(entry, field_name, ftypes[*field_index],
            *field_index);
        (*field_index)++;
    }
    return true;
}

static bool
llvm_domain_struct_register_zone_fields(LLVMGenCtx *ctx,
                                        ASTNode *stmt,
                                        LLVMClassTypeEntry *entry,
                                        LLVMTypeRef *ftypes)
{
    const char *decl_name = llvm_decl_node_name(stmt);
    int field_index = 0;
    size_t slot_count = 0;
    ASTNode **slots = ast_zone_slots(stmt, &slot_count);
    LLVMHostedSharedFieldView shared_view =
        llvm_hosted_shared_field_view_from_decl(ctx, decl_name, stmt);
    size_t layer_slot_count = 0;
    ASTNode **layer_slots = ast_zone_layer_slots(stmt, &layer_slot_count);
    size_t state_count = 0;
    ASTNode **states = ast_zone_states(stmt, &state_count);
    size_t refresh_count = 0;
    ASTNode **refreshes = ast_zone_refreshes(stmt, &refresh_count);

    entry->domain_kind = LLVM_DOMAIN_ZONE;
    if (llvm_hosted_shared_field_view_missing_mir_metadata(&shared_view)) {
        llvm_set_error(ctx,
            "LLVM zone '%s' shared fields missing MIR declaration metadata",
            decl_name != NULL ? decl_name : "<anonymous>");
        return false;
    }
    for (size_t j = 0; j < slot_count; j++, field_index++) {
        ASTNode *slot = slots[j];
        llvm_class_add_field_ex(entry, ast_domain_slot_name(slot),
            ftypes[field_index], field_index,
            ast_domain_slot_is_subject(slot));
    }
    if (!llvm_domain_struct_register_shared_field_names(ctx, entry, ftypes,
            &field_index, &shared_view, "zone")) {
        return false;
    }
    for (size_t j = 0; j < layer_slot_count; j++, field_index++) {
        ASTNode *slot = layer_slots[j];
        llvm_class_add_field(entry, ast_zone_layer_slot_name(slot),
            ftypes[field_index], field_index);
    }
    for (size_t j = 0; j < layer_slot_count; j++, field_index++) {
        ASTNode *slot = layer_slots[j];
        if (!llvm_domain_struct_add_generated_field(ctx, entry,
                ftypes[field_index], field_index, "layer_active",
                ast_zone_layer_slot_name(slot)))
            return false;
    }
    for (size_t j = 0; j < layer_slot_count; j++, field_index++) {
        ASTNode *slot = layer_slots[j];
        if (!llvm_domain_struct_add_generated_field(ctx, entry,
                ftypes[field_index], field_index, "layer_epoch",
                ast_zone_layer_slot_name(slot)))
            return false;
    }
    for (size_t j = 0; j < layer_slot_count; j++, field_index++) {
        ASTNode *slot = layer_slots[j];
        if (!llvm_domain_struct_add_generated_field(ctx, entry,
                ftypes[field_index], field_index, "layer_cause",
                ast_zone_layer_slot_name(slot)))
            return false;
    }
    for (size_t j = 0; j < state_count; j++, field_index++) {
        ASTNode *state = states[j];
        if (!llvm_domain_struct_add_generated_field(ctx, entry,
                ftypes[field_index], field_index, "state",
                ast_zone_state_name(state)))
            return false;
    }
    for (size_t j = 0; j < state_count; j++, field_index++) {
        ASTNode *state = states[j];
        if (!llvm_domain_struct_add_generated_field(ctx, entry,
                ftypes[field_index], field_index, "state_epoch",
                ast_zone_state_name(state)))
            return false;
    }
    for (size_t j = 0; j < state_count; j++, field_index++) {
        ASTNode *state = states[j];
        if (!llvm_domain_struct_add_generated_field(ctx, entry,
                ftypes[field_index], field_index, "state_cause",
                ast_zone_state_name(state)))
            return false;
    }
    llvm_domain_add_projection_state_fields(ctx, entry, ftypes, &field_index,
        slots, slot_count, refreshes, refresh_count);
    llvm_class_add_field(entry, pergyra_strdup("__sync_generation"),
        ftypes[field_index], field_index);
    return true;
}

static void
llvm_domain_struct_register_roster_fields(LLVMGenCtx *ctx,
                                          ASTNode *stmt,
                                          LLVMClassTypeEntry *entry,
                                          LLVMTypeRef *ftypes)
{
    const char *decl_name = llvm_decl_node_name(stmt);
    LLVMHostedSharedFieldView shared_view =
        llvm_hosted_shared_field_view_from_decl(ctx, decl_name, stmt);
    int field_index = 0;

    entry->domain_kind = LLVM_DOMAIN_SYSTEMIC;
    if (llvm_hosted_shared_field_view_missing_mir_metadata(&shared_view)) {
        llvm_set_error(ctx,
            "LLVM roster '%s' shared fields missing MIR declaration metadata",
            decl_name != NULL ? decl_name : "<anonymous>");
        return;
    }
    for (size_t j = 0; j < ast_roster_party_count(stmt); j++, field_index++) {
        ASTNode *slot = ast_roster_party(stmt, j);
        llvm_class_add_field(entry, ast_roster_slot_name(slot),
            ftypes[field_index], field_index);
    }
    (void) llvm_domain_struct_register_shared_field_names(ctx, entry, ftypes,
        &field_index, &shared_view, "roster");
}

static bool
llvm_domain_struct_register_world_fields(LLVMGenCtx *ctx,
                                         ASTNode *stmt,
                                         LLVMClassTypeEntry *entry,
                                         LLVMTypeRef *ftypes)
{
    const char *decl_name = llvm_decl_node_name(stmt);
    int field_index = 0;
    size_t roster_count = 0;
    ASTNode **rosters = ast_world_rosters(stmt, &roster_count);
    size_t zone_count = 0;
    ASTNode **zones = ast_world_zones(stmt, &zone_count);
    LLVMHostedSharedFieldView shared_view =
        llvm_hosted_shared_field_view_from_decl(ctx, decl_name, stmt);
    size_t state_count = 0;
    ASTNode **states = ast_world_states(stmt, &state_count);

    entry->domain_kind = LLVM_DOMAIN_WORLD;
    if (llvm_hosted_shared_field_view_missing_mir_metadata(&shared_view)) {
        llvm_set_error(ctx,
            "LLVM world '%s' shared fields missing MIR declaration metadata",
            decl_name != NULL ? decl_name : "<anonymous>");
        return false;
    }
    for (size_t j = 0; j < roster_count; j++, field_index++) {
        ASTNode *ws = rosters[j];
        llvm_class_add_field(entry, ast_world_roster_slot_name(ws),
            ftypes[field_index], field_index);
    }
    for (size_t j = 0; j < zone_count; j++, field_index++) {
        ASTNode *wz = zones[j];
        llvm_class_add_field(entry, ast_world_zone_slot_name(wz),
            ftypes[field_index], field_index);
    }
    if (!llvm_domain_struct_register_shared_field_names(ctx, entry, ftypes,
            &field_index, &shared_view, "world")) {
        return false;
    }
    for (size_t j = 0; j < zone_count; j++, field_index++) {
        ASTNode *wz = zones[j];
        if (!llvm_domain_struct_add_generated_field(ctx, entry,
                ftypes[field_index], field_index, "zone_active",
                ast_world_zone_slot_name(wz)))
            return false;
    }
    for (size_t j = 0; j < zone_count; j++, field_index++) {
        ASTNode *wz = zones[j];
        if (!llvm_domain_struct_add_generated_field(ctx, entry,
                ftypes[field_index], field_index, "zone_dirty",
                ast_world_zone_slot_name(wz)))
            return false;
    }
    for (size_t j = 0; j < zone_count; j++, field_index++) {
        ASTNode *wz = zones[j];
        if (!llvm_domain_struct_add_generated_field(ctx, entry,
                ftypes[field_index], field_index, "zone_seen_generation",
                ast_world_zone_slot_name(wz)))
            return false;
    }
    for (size_t j = 0; j < state_count; j++, field_index++) {
        ASTNode *state = states[j];
        if (!llvm_domain_struct_add_generated_field(ctx, entry,
                ftypes[field_index], field_index, "zone_state",
                ast_world_state_name(state)))
            return false;
    }
    for (size_t j = 0; j < zone_count; j++, field_index++) {
        ASTNode *wz = zones[j];
        if (!llvm_domain_struct_add_generated_field(ctx, entry,
                ftypes[field_index], field_index, "zone_epoch",
                ast_world_zone_slot_name(wz)))
            return false;
    }
    for (size_t j = 0; j < zone_count; j++, field_index++) {
        ASTNode *wz = zones[j];
        if (!llvm_domain_struct_add_generated_field(ctx, entry,
                ftypes[field_index], field_index, "zone_cause",
                ast_world_zone_slot_name(wz)))
            return false;
    }
    for (size_t j = 0; j < state_count; j++, field_index++) {
        ASTNode *state = states[j];
        if (!llvm_domain_struct_add_generated_field(ctx, entry,
                ftypes[field_index], field_index, "zone_state_epoch",
                ast_world_state_name(state)))
            return false;
    }
    for (size_t j = 0; j < state_count; j++, field_index++) {
        ASTNode *state = states[j];
        if (!llvm_domain_struct_add_generated_field(ctx, entry,
                ftypes[field_index], field_index, "zone_state_cause",
                ast_world_state_name(state)))
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
                                           ASTNode **refreshes,
                                           size_t refresh_count)
{
    const char *decl_name = llvm_decl_node_name(stmt);
    LLVMHostedSharedFieldView shared_view =
        llvm_hosted_shared_field_view_from_decl(ctx, decl_name, stmt);
    int field_index = 0;

    if (stmt->type == AST_RELATION_DECL || stmt->type == AST_EFFECT_DECL)
        entry->domain_kind = LLVM_DOMAIN_PROJECTION;
    if (llvm_hosted_shared_field_view_missing_mir_metadata(&shared_view)) {
        llvm_set_error(ctx,
            "LLVM domain '%s' shared fields missing MIR declaration metadata",
            decl_name != NULL ? decl_name : "<anonymous>");
        return false;
    }
    for (size_t j = 0; j < slot_count; j++, field_index++) {
        ASTNode *slot = slots[j];
        llvm_class_add_field(entry, ast_domain_slot_name(slot),
            ftypes[field_index], field_index);
    }
    if (!llvm_domain_struct_register_shared_field_names(ctx, entry, ftypes,
            &field_index, &shared_view, "domain")) {
        return false;
    }
    for (size_t j = 0; j < ast_party_role_count(stmt); j++) {
        ASTNode *rs = ast_party_role(stmt, j);
        if (rs == NULL || rs->type != AST_ROLE_SLOT
            || !ast_role_slot_is_dynamic(rs))
            continue;
        if (!llvm_domain_struct_add_suffixed_field(ctx, entry, ctx->type_i8ptr,
                field_index, ast_role_slot_name(rs), "vtable"))
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
                                   ASTNode **refreshes,
                                   size_t refresh_count)
{
    if (ctx == NULL || stmt == NULL || entry == NULL || ftypes == NULL)
        return false;
    if (stmt->type == AST_ZONE_DECL)
        return llvm_domain_struct_register_zone_fields(ctx, stmt, entry, ftypes);
    if (stmt->type == AST_ROSTER_DECL) {
        llvm_domain_struct_register_roster_fields(ctx, stmt, entry, ftypes);
        return !ctx->has_error;
    }
    if (stmt->type == AST_WORLD_DECL)
        return llvm_domain_struct_register_world_fields(ctx, stmt, entry, ftypes);
    return llvm_domain_struct_register_default_fields(ctx, stmt, entry, ftypes,
        slots, slot_count, refreshes, refresh_count);
}

#endif /* PGY_LLVM_ENABLED */
