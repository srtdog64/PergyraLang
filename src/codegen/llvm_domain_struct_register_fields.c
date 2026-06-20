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
    LLVMHostedDomainSlotView slot_view =
        llvm_hosted_domain_slot_view_from_decl(ctx, decl_name, stmt);
    size_t domain_slot_count = slot_view.count;
    LLVMHostedSharedFieldView shared_view =
        llvm_hosted_shared_field_view_from_decl(ctx, decl_name, stmt);
    LLVMHostedZoneLayerSlotView layer_view =
        llvm_hosted_zone_layer_slot_view_from_decl(ctx, decl_name, stmt);
    LLVMHostedZoneStateView state_view =
        llvm_hosted_zone_state_view_from_decl(ctx, decl_name, stmt);
    LLVMHostedZoneRefreshView refresh_view =
        llvm_hosted_zone_refresh_view_from_decl(ctx, decl_name, stmt);

    entry->domain_kind = LLVM_DOMAIN_ZONE;
    if (llvm_hosted_domain_slot_view_missing_mir_metadata(&slot_view)) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing zone domain-slot metadata for '%s'",
            decl_name != NULL ? decl_name : "<anonymous>");
        return false;
    }
    if (llvm_hosted_shared_field_view_missing_mir_metadata(&shared_view)) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing zone shared-field metadata for '%s'",
            decl_name != NULL ? decl_name : "<anonymous>");
        return false;
    }
    if (llvm_hosted_zone_layer_slot_view_missing_mir_metadata(&layer_view)) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing zone layer-slot metadata for '%s'",
            decl_name != NULL ? decl_name : "<anonymous>");
        return false;
    }
    if (llvm_hosted_zone_state_view_missing_mir_metadata(&state_view)) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing zone state metadata for '%s'",
            decl_name != NULL ? decl_name : "<anonymous>");
        return false;
    }
    if (llvm_hosted_zone_refresh_view_missing_mir_metadata(&refresh_view)) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing zone refresh metadata for '%s'",
            decl_name != NULL ? decl_name : "<anonymous>");
        return false;
    }
    for (size_t j = 0; j < domain_slot_count; j++, field_index++) {
        const char *slot_name =
            llvm_hosted_domain_slot_view_name(&slot_view, j);
        llvm_class_add_field_ex(entry, slot_name,
            ftypes[field_index], field_index,
            llvm_hosted_domain_slot_view_is_subject_like(&slot_view, j));
    }
    if (!llvm_domain_struct_register_shared_field_names(ctx, entry, ftypes,
            &field_index, &shared_view, "zone")) {
        return false;
    }
    for (size_t j = 0; j < layer_view.count; j++, field_index++) {
        const char *slot_name =
            llvm_hosted_zone_layer_slot_view_name(&layer_view, j);
        if (slot_name == NULL) {
            llvm_set_error(ctx,
                "LLVM zone '%s' layer slot[%zu] missing MIR field name",
                decl_name != NULL ? decl_name : "<anonymous>",
                j);
            return false;
        }
        llvm_class_add_field(entry, slot_name, ftypes[field_index],
            field_index);
    }
    for (size_t j = 0; j < layer_view.count; j++, field_index++) {
        const char *slot_name =
            llvm_hosted_zone_layer_slot_view_name(&layer_view, j);
        if (slot_name == NULL) {
            llvm_set_error(ctx,
                "LLVM zone '%s' layer active field[%zu] missing MIR field name",
                decl_name != NULL ? decl_name : "<anonymous>",
                j);
            return false;
        }
        if (!llvm_domain_struct_add_generated_field(ctx, entry,
                ftypes[field_index], field_index, "layer_active",
                slot_name))
            return false;
    }
    for (size_t j = 0; j < layer_view.count; j++, field_index++) {
        const char *slot_name =
            llvm_hosted_zone_layer_slot_view_name(&layer_view, j);
        if (slot_name == NULL) {
            llvm_set_error(ctx,
                "LLVM zone '%s' layer epoch field[%zu] missing MIR field name",
                decl_name != NULL ? decl_name : "<anonymous>",
                j);
            return false;
        }
        if (!llvm_domain_struct_add_generated_field(ctx, entry,
                ftypes[field_index], field_index, "layer_epoch",
                slot_name))
            return false;
    }
    for (size_t j = 0; j < layer_view.count; j++, field_index++) {
        const char *slot_name =
            llvm_hosted_zone_layer_slot_view_name(&layer_view, j);
        if (slot_name == NULL) {
            llvm_set_error(ctx,
                "LLVM zone '%s' layer cause field[%zu] missing MIR field name",
                decl_name != NULL ? decl_name : "<anonymous>",
                j);
            return false;
        }
        if (!llvm_domain_struct_add_generated_field(ctx, entry,
                ftypes[field_index], field_index, "layer_cause",
                slot_name))
            return false;
    }
    for (size_t j = 0; j < state_view.count; j++, field_index++) {
        const char *state_name =
            llvm_hosted_zone_state_view_name(&state_view, j);
        if (state_name == NULL) {
            llvm_set_error(ctx,
                "LLVM zone '%s' state field[%zu] missing MIR state name",
                decl_name != NULL ? decl_name : "<anonymous>",
                j);
            return false;
        }
        if (!llvm_domain_struct_add_generated_field(ctx, entry,
                ftypes[field_index], field_index, "state",
                state_name))
            return false;
    }
    for (size_t j = 0; j < state_view.count; j++, field_index++) {
        const char *state_name =
            llvm_hosted_zone_state_view_name(&state_view, j);
        if (state_name == NULL) {
            llvm_set_error(ctx,
                "LLVM zone '%s' state epoch field[%zu] missing MIR state name",
                decl_name != NULL ? decl_name : "<anonymous>",
                j);
            return false;
        }
        if (!llvm_domain_struct_add_generated_field(ctx, entry,
                ftypes[field_index], field_index, "state_epoch",
                state_name))
            return false;
    }
    for (size_t j = 0; j < state_view.count; j++, field_index++) {
        const char *state_name =
            llvm_hosted_zone_state_view_name(&state_view, j);
        if (state_name == NULL) {
            llvm_set_error(ctx,
                "LLVM zone '%s' state cause field[%zu] missing MIR state name",
                decl_name != NULL ? decl_name : "<anonymous>",
                j);
            return false;
        }
        if (!llvm_domain_struct_add_generated_field(ctx, entry,
                ftypes[field_index], field_index, "state_cause",
                state_name))
            return false;
    }
    if (!llvm_domain_add_projection_state_fields_from_zone_refresh_view(ctx,
            entry, ftypes, &field_index, &slot_view, &refresh_view)) {
        return false;
    }
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
    LLVMHostedRosterSlotView roster_view =
        llvm_hosted_roster_slot_view_from_decl(ctx, decl_name, stmt);
    LLVMHostedSharedFieldView shared_view =
        llvm_hosted_shared_field_view_from_decl(ctx, decl_name, stmt);
    int field_index = 0;

    entry->domain_kind = LLVM_DOMAIN_SYSTEMIC;
    if (llvm_hosted_roster_slot_view_missing_mir_metadata(&roster_view)) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing roster-slot metadata for '%s'",
            decl_name != NULL ? decl_name : "<anonymous>");
        return;
    }
    if (llvm_hosted_shared_field_view_missing_mir_metadata(&shared_view)) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing roster shared-field metadata for '%s'",
            decl_name != NULL ? decl_name : "<anonymous>");
        return;
    }
    for (size_t j = 0; j < roster_view.count; j++, field_index++) {
        const char *slot_name =
            llvm_hosted_roster_slot_view_name(&roster_view, j);
        llvm_class_add_field(entry, slot_name,
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
    LLVMHostedWorldRosterSlotView roster_view =
        llvm_hosted_world_roster_slot_view_from_decl(ctx, decl_name, stmt);
    size_t roster_count = roster_view.count;
    LLVMHostedWorldZoneSlotView zone_view =
        llvm_hosted_world_zone_slot_view_from_decl(ctx, decl_name, stmt);
    size_t zone_count = zone_view.count;
    LLVMHostedSharedFieldView shared_view =
        llvm_hosted_shared_field_view_from_decl(ctx, decl_name, stmt);
    size_t state_count = 0;
    ASTNode **states = ast_world_states(stmt, &state_count);

    entry->domain_kind = LLVM_DOMAIN_WORLD;
    if (llvm_hosted_world_roster_slot_view_missing_mir_metadata(
            &roster_view)) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing world roster-slot metadata for '%s'",
            decl_name != NULL ? decl_name : "<anonymous>");
        return false;
    }
    if (llvm_hosted_shared_field_view_missing_mir_metadata(&shared_view)) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing world shared-field metadata for '%s'",
            decl_name != NULL ? decl_name : "<anonymous>");
        return false;
    }
    if (llvm_hosted_world_zone_slot_view_missing_mir_metadata(&zone_view)) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing world zone-slot metadata for '%s'",
            decl_name != NULL ? decl_name : "<anonymous>");
        return false;
    }
    for (size_t j = 0; j < roster_count; j++, field_index++) {
        const char *slot_name =
            llvm_hosted_world_roster_slot_view_name(&roster_view, j);
        llvm_class_add_field(entry, slot_name,
            ftypes[field_index], field_index);
    }
    for (size_t j = 0; j < zone_count; j++, field_index++) {
        const char *slot_name =
            llvm_hosted_world_zone_slot_view_name(&zone_view, j);
        llvm_class_add_field(entry, slot_name, ftypes[field_index],
            field_index);
    }
    if (!llvm_domain_struct_register_shared_field_names(ctx, entry, ftypes,
            &field_index, &shared_view, "world")) {
        return false;
    }
    for (size_t j = 0; j < zone_count; j++, field_index++) {
        const char *slot_name =
            llvm_hosted_world_zone_slot_view_name(&zone_view, j);
        if (!llvm_domain_struct_add_generated_field(ctx, entry,
                ftypes[field_index], field_index, "zone_active",
                slot_name))
            return false;
    }
    for (size_t j = 0; j < zone_count; j++, field_index++) {
        const char *slot_name =
            llvm_hosted_world_zone_slot_view_name(&zone_view, j);
        if (!llvm_domain_struct_add_generated_field(ctx, entry,
                ftypes[field_index], field_index, "zone_dirty",
                slot_name))
            return false;
    }
    for (size_t j = 0; j < zone_count; j++, field_index++) {
        const char *slot_name =
            llvm_hosted_world_zone_slot_view_name(&zone_view, j);
        if (!llvm_domain_struct_add_generated_field(ctx, entry,
                ftypes[field_index], field_index, "zone_seen_generation",
                slot_name))
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
        const char *slot_name =
            llvm_hosted_world_zone_slot_view_name(&zone_view, j);
        if (!llvm_domain_struct_add_generated_field(ctx, entry,
                ftypes[field_index], field_index, "zone_epoch",
                slot_name))
            return false;
    }
    for (size_t j = 0; j < zone_count; j++, field_index++) {
        const char *slot_name =
            llvm_hosted_world_zone_slot_view_name(&zone_view, j);
        if (!llvm_domain_struct_add_generated_field(ctx, entry,
                ftypes[field_index], field_index, "zone_cause",
                slot_name))
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
                                           const LLVMHostedZoneRefreshView *refresh_view)
{
    const char *decl_name = llvm_decl_node_name(stmt);
    LLVMHostedSharedFieldView shared_view =
        llvm_hosted_shared_field_view_from_decl(ctx, decl_name, stmt);
    LLVMHostedDomainSlotView slot_view =
        llvm_hosted_domain_slot_view_from_decl(ctx, decl_name, stmt);
    size_t domain_slot_count = slot_view.count;
    LLVMHostedRoleSlotView role_view =
        llvm_hosted_role_slot_view_from_decl(ctx, decl_name, stmt);
    int field_index = 0;

    if (stmt->type == AST_RELATION_DECL || stmt->type == AST_EFFECT_DECL)
        entry->domain_kind = LLVM_DOMAIN_PROJECTION;
    if (llvm_hosted_domain_slot_view_missing_mir_metadata(&slot_view)) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing domain-slot metadata for '%s'",
            decl_name != NULL ? decl_name : "<anonymous>");
        return false;
    }
    if (llvm_hosted_shared_field_view_missing_mir_metadata(&shared_view)) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing domain shared-field metadata for '%s'",
            decl_name != NULL ? decl_name : "<anonymous>");
        return false;
    }
    if (llvm_hosted_role_slot_view_missing_mir_metadata(&role_view)) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing party role-slot metadata for '%s'",
            decl_name != NULL ? decl_name : "<anonymous>");
        return false;
    }
    for (size_t j = 0; j < domain_slot_count; j++, field_index++) {
        const char *slot_name =
            llvm_hosted_domain_slot_view_name(&slot_view, j);
        llvm_class_add_field(entry, slot_name,
            ftypes[field_index], field_index);
    }
    if (!llvm_domain_struct_register_shared_field_names(ctx, entry, ftypes,
            &field_index, &shared_view, "domain")) {
        return false;
    }
    for (size_t j = 0; j < role_view.count; j++) {
        const char *slot_name;
        if (!llvm_hosted_role_slot_view_is_dynamic(&role_view, j))
            continue;
        slot_name = llvm_hosted_role_slot_view_name(&role_view, j);
        if (!llvm_domain_struct_add_suffixed_field(ctx, entry, ctx->type_i8ptr,
                field_index, slot_name, "vtable"))
            return false;
        field_index++;
    }
    if (stmt->type == AST_RELATION_DECL || stmt->type == AST_EFFECT_DECL) {
        if (!llvm_domain_add_projection_state_fields_from_zone_refresh_view(
                ctx, entry, ftypes, &field_index, &slot_view, refresh_view)) {
            return false;
        }
    }
    return true;
}

bool
llvm_domain_struct_register_fields(LLVMGenCtx *ctx,
                                   ASTNode *stmt,
                                   LLVMClassTypeEntry *entry,
                                   LLVMTypeRef *ftypes,
                                   const LLVMHostedZoneRefreshView *refresh_view)
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
        refresh_view);
}

#endif /* PGY_LLVM_ENABLED */
