/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend overlay host-field lookup helpers.
 */

#include "transpiler_overlay_host_fields.h"

#include <string.h>

#include "../parser/ast_api.h"
#include "host_decl_compat.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_host_self_policy.h"

static bool
domain_slot_list_has_field(ASTNode **slots, size_t slot_count,
                           const char *field_name)
{
    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        const char *slot_name = ast_domain_slot_name(slot);
        if (slot != NULL && slot_name != NULL
            && strcmp(slot_name, field_name) == 0) {
            return true;
        }
    }

    return false;
}

static bool
zone_layer_slot_list_has_field(ASTNode **slots, size_t slot_count,
                               const char *field_name)
{
    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        if (slot != NULL && ast_zone_layer_slot_name(slot) != NULL
            && strcmp(ast_zone_layer_slot_name(slot), field_name) == 0) {
            return true;
        }
    }

    return false;
}

static bool
roster_slot_list_has_field(ASTNode *roster, const char *field_name)
{
    size_t party_count;

    if (roster == NULL || field_name == NULL)
        return false;

    party_count = ast_roster_party_count(roster);
    for (size_t i = 0; i < party_count; i++) {
        ASTNode *slot = ast_roster_party(roster, i);
        const char *slot_name = ast_roster_slot_name(slot);
        if (slot_name != NULL && strcmp(slot_name, field_name) == 0)
            return true;
    }

    return false;
}

bool
current_class_uses_self_cell(TranspilerCtx *ctx)
{
    ASTNode *host_decl = NULL;
    const char *class_name = NULL;

    if (ctx == NULL)
        return false;
    host_decl = transpiler_current_host_decl_local(ctx);
    if (host_decl != NULL && host_decl->type == AST_CLASS_DECL)
        class_name = ast_class_name(host_decl);
    return class_name != NULL
        && is_pointer_self_host_type_name(ctx, class_name);
}

bool
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

    return pgy_host_class_field_compat_find(decl, field_name) != NULL;
}

bool
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

    size_t slot_count = 0;
    ASTNode **slots = ast_zone_slots(decl, &slot_count);
    if (domain_slot_list_has_field(slots, slot_count, field_name))
        return true;
    size_t layer_slot_count = 0;
    ASTNode **layer_slots = ast_zone_layer_slots(decl, &layer_slot_count);
    if (zone_layer_slot_list_has_field(
            layer_slots, layer_slot_count, field_name)) {
        return true;
    }
    if (pgy_host_shared_field_compat_find(decl, field_name) != NULL)
        return true;

    return false;
}

bool
current_party_has_field(TranspilerCtx *ctx, const char *field_name)
{
    ASTNode *decl;
    if (ctx == NULL || field_name == NULL)
        return false;

    decl = transpiler_current_host_decl_local(ctx);
    if (decl != NULL && decl->type != AST_PARTY_DECL)
        decl = NULL;
    if (decl == NULL)
        return false;

    if (pgy_host_shared_field_compat_find(decl, field_name) != NULL)
        return true;

    return false;
}

bool
current_roster_has_field(TranspilerCtx *ctx, const char *field_name)
{
    ASTNode *decl;
    if (ctx == NULL || field_name == NULL)
        return false;

    decl = transpiler_current_host_decl_local(ctx);
    if (decl != NULL && decl->type != AST_ROSTER_DECL)
        decl = NULL;
    if (decl == NULL)
        return false;

    if (roster_slot_list_has_field(decl, field_name))
        return true;

    if (pgy_host_shared_field_compat_find(decl, field_name) != NULL)
        return true;

    return false;
}

bool
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

    size_t slot_count = 0;
    ASTNode **slots = ast_relation_slots(decl, &slot_count);
    if (domain_slot_list_has_field(slots, slot_count, field_name))
        return true;
    if (pgy_host_shared_field_compat_find(decl, field_name) != NULL)
        return true;

    return false;
}

bool
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

    size_t slot_count = 0;
    ASTNode **slots = ast_effect_slots(decl, &slot_count);
    if (domain_slot_list_has_field(slots, slot_count, field_name))
        return true;
    if (pgy_host_shared_field_compat_find(decl, field_name) != NULL)
        return true;

    return false;
}
