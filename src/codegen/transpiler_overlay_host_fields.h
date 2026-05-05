#ifndef PGY_TRANSPILER_OVERLAY_HOST_FIELDS_H
#define PGY_TRANSPILER_OVERLAY_HOST_FIELDS_H

#include "transpiler_host_self_policy.h"

static bool
current_class_uses_self_cell(TranspilerCtx *ctx)
{
    ASTNode *host_decl = NULL;
    const char *class_name = NULL;

    if (ctx == NULL)
        return false;
    host_decl = transpiler_current_host_decl_local(ctx);
    if (host_decl != NULL && host_decl->type == AST_CLASS_DECL)
        class_name = host_decl->data.class_decl.name;
    return ctx != NULL
        && class_name != NULL
        && is_pointer_self_host_type_name(ctx, class_name);
}

static bool
current_class_has_field(TranspilerCtx *ctx, const char *field_name)
{
    ASTNode *decl;

    if (ctx == NULL || field_name == NULL)
        return false;

    decl = transpiler_current_host_decl_local(ctx);
    if (decl != NULL && decl->type != AST_CLASS_DECL)
        decl = NULL;
    if (decl == NULL)
        return false;

    for (size_t i = 0; i < decl->data.class_decl.field_count; i++) {
        ClassField *field = decl->data.class_decl.fields[i];
        if (field != NULL && field->name != NULL
            && strcmp(field->name, field_name) == 0) {
            return true;
        }
    }

    return false;
}

static bool
current_zone_has_field(TranspilerCtx *ctx, const char *field_name)
{
    ASTNode *decl;

    if (ctx == NULL || field_name == NULL)
        return false;

    decl = transpiler_current_host_decl_local(ctx);
    if (decl != NULL && decl->type != AST_ZONE_DECL)
        decl = NULL;
    if (decl == NULL)
        return false;

    for (size_t i = 0; i < decl->data.zone_decl.slot_count; i++) {
        ASTNode *slot = decl->data.zone_decl.slots[i];
        if (slot != NULL && slot->data.domain_slot.slot_name != NULL
            && strcmp(slot->data.domain_slot.slot_name, field_name) == 0) {
            return true;
        }
    }
    for (size_t i = 0; i < decl->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = decl->data.zone_decl.layer_slots[i];
        if (slot != NULL && slot->data.zone_layer_slot.slot_name != NULL
            && strcmp(slot->data.zone_layer_slot.slot_name, field_name) == 0) {
            return true;
        }
    }
    for (size_t i = 0; i < decl->data.zone_decl.shared_count; i++) {
        ASTNode *shared = decl->data.zone_decl.shared_fields[i];
        if (shared != NULL && shared->data.party_shared.name != NULL
            && strcmp(shared->data.party_shared.name, field_name) == 0) {
            return true;
        }
    }

    return false;
}

static bool
current_relation_has_field(TranspilerCtx *ctx, const char *field_name)
{
    ASTNode *decl;

    if (ctx == NULL || field_name == NULL)
        return false;

    decl = transpiler_current_host_decl_local(ctx);
    if (decl != NULL && decl->type != AST_RELATION_DECL)
        decl = NULL;
    if (decl == NULL)
        return false;

    for (size_t i = 0; i < decl->data.relation_decl.slot_count; i++) {
        ASTNode *slot = decl->data.relation_decl.slots[i];
        if (slot != NULL && slot->data.domain_slot.slot_name != NULL
            && strcmp(slot->data.domain_slot.slot_name, field_name) == 0) {
            return true;
        }
    }
    for (size_t i = 0; i < decl->data.relation_decl.shared_count; i++) {
        ASTNode *shared = decl->data.relation_decl.shared_fields[i];
        if (shared != NULL && shared->data.party_shared.name != NULL
            && strcmp(shared->data.party_shared.name, field_name) == 0) {
            return true;
        }
    }

    return false;
}

static bool
current_effect_has_field(TranspilerCtx *ctx, const char *field_name)
{
    ASTNode *decl;

    if (ctx == NULL || field_name == NULL)
        return false;

    decl = transpiler_current_host_decl_local(ctx);
    if (decl != NULL && decl->type != AST_EFFECT_DECL)
        decl = NULL;
    if (decl == NULL)
        return false;

    for (size_t i = 0; i < decl->data.effect_decl.slot_count; i++) {
        ASTNode *slot = decl->data.effect_decl.slots[i];
        if (slot != NULL && slot->data.domain_slot.slot_name != NULL
            && strcmp(slot->data.domain_slot.slot_name, field_name) == 0) {
            return true;
        }
    }
    for (size_t i = 0; i < decl->data.effect_decl.shared_count; i++) {
        ASTNode *shared = decl->data.effect_decl.shared_fields[i];
        if (shared != NULL && shared->data.party_shared.name != NULL
            && strcmp(shared->data.party_shared.name, field_name) == 0) {
            return true;
        }
    }

    return false;
}

#endif
