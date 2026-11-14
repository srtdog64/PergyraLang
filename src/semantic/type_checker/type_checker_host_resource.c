#include <string.h>

#include "type_checker_internal.h"

static Type *
host_resource_resolve_domain_slot_type(ASTNode *slot, SemanticContext *ctx)
{
    if (slot == NULL || slot->type != AST_DOMAIN_SLOT)
        return TYPE_UNKNOWN;
    return semantic_host_resolve_type_ref(ast_domain_slot_type(slot), ctx);
}

bool
domain_has_subject_slot_type(ASTNode **slots, size_t slot_count,
                             SemanticContext *ctx, const char *type_name)
{
    if (slots == NULL || ctx == NULL || type_name == NULL)
        return false;

    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        Type *slot_type;
        if (slot == NULL || slot->type != AST_DOMAIN_SLOT
            || !ast_domain_slot_is_subject(slot)
            || ast_domain_slot_type(slot) == NULL) {
            continue;
        }
        slot_type = host_resource_resolve_domain_slot_type(slot, ctx);
        if (slot_type != NULL && slot_type->name != NULL
            && strcmp(slot_type->name, type_name) == 0) {
            return true;
        }
    }

    return false;
}

bool
zone_has_authority_for_subject_type(ASTNode *zone, SemanticContext *ctx,
                                    const char *type_name)
{
    ASTNode **authorities;
    size_t authority_count;

    if (zone == NULL || zone->type != AST_ZONE_DECL || ctx == NULL
        || type_name == NULL) {
        return false;
    }

    authorities = ast_zone_authorities(zone, &authority_count);
    for (size_t i = 0; i < authority_count; i++) {
        ASTNode *authority = authorities[i];
        const char *subject_slot = ast_zone_authority_subject_slot_name(authority);
        ASTNode *slot;
        Type *slot_type;
        if (authority == NULL || subject_slot == NULL) {
            continue;
        }
        slot = find_zone_domain_slot(zone, subject_slot);
        if (slot == NULL || ast_domain_slot_type(slot) == NULL)
            continue;
        slot_type = host_resource_resolve_domain_slot_type(slot, ctx);
        if (slot_type != NULL && slot_type->name != NULL
            && strcmp(slot_type->name, type_name) == 0) {
            return true;
        }
    }

    return false;
}

bool
zone_has_effect_layer_type(ASTNode *zone, const char *effect_name)
{
    ASTNode **layer_slots;
    size_t layer_slot_count;

    if (zone == NULL || zone->type != AST_ZONE_DECL || effect_name == NULL)
        return false;

    layer_slots = ast_zone_layer_slots(zone, &layer_slot_count);
    for (size_t i = 0; i < layer_slot_count; i++) {
        ASTNode *slot = layer_slots[i];
        if (slot == NULL || slot->type != AST_ZONE_LAYER_SLOT
            || ast_zone_layer_slot_layer_type(slot) == NULL
            || ast_zone_layer_slot_is_relation(slot)) {
            continue;
        }
        if (strcmp(ast_zone_layer_slot_layer_type(slot), effect_name) == 0)
            return true;
    }

    return false;
}

bool
expr_is_class_constructor_call(const ASTNode *expr, SemanticContext *ctx)
{
    ASTNode *callee;
    ASTNode *decl;

    if (expr == NULL || expr->type != AST_CALL || ctx == NULL)
        return false;

    callee = ast_call_callee(expr);
    if (callee == NULL || callee->type != AST_IDENTIFIER
        || ast_identifier_name(callee) == NULL) {
        return false;
    }

    decl = semantic_constructor_decl_for_symbol_kind(ctx, SYMBOL_CLASS,
                                                     ast_identifier_name(callee));
    if (decl != NULL)
        return !ast_class_is_struct(decl);
    return false;
}

bool
expr_is_qubit_claim(const ASTNode *expr)
{
    ASTNode *callee = (expr != NULL && expr->type == AST_CALL)
        ? ast_call_callee(expr)
        : NULL;

    return expr != NULL
        && expr->type == AST_CALL
        && callee != NULL
        && callee->type == AST_IDENTIFIER
        && ast_identifier_name(callee) != NULL
        && strcmp(ast_identifier_name(callee), "ClaimQubit") == 0;
}

bool
expr_is_device_slot_claim(const ASTNode *expr)
{
    ASTNode *callee = (expr != NULL && expr->type == AST_CALL)
        ? ast_call_callee(expr)
        : NULL;

    return expr != NULL
        && expr->type == AST_CALL
        && callee != NULL
        && callee->type == AST_IDENTIFIER
        && ast_identifier_name(callee) != NULL
        && strcmp(ast_identifier_name(callee), "ClaimDeviceSlot") == 0;
}

bool
expr_is_movable_resource_transfer_source(const ASTNode *expr)
{
    if (expr == NULL)
        return false;

    switch (expr->type) {
    case AST_IDENTIFIER:
    case AST_CALL:
    case AST_CHANNEL_RECV:
    case AST_AWAIT_EXPR:
        return true;
    default:
        return false;
    }
}

bool
slot_transfer_compatible(const Type *from, const Type *to)
{
    if (from == NULL || to == NULL)
        return false;
    if (from->kind != TYPE_KIND_SLOT || to->kind != TYPE_KIND_SLOT)
        return false;
    if (type_slot_access_mode(from) != SLOT_ACCESS_MOVE_TOKEN)
        return false;
    if (type_slot_access_mode(to) != SLOT_ACCESS_OWNED)
        return false;
    if (type_slot_is_secure(from) != type_slot_is_secure(to))
        return false;
    return type_is_assignable(type_slot_inner_type(from), type_slot_inner_type(to))
        && type_is_assignable(type_slot_inner_type(to), type_slot_inner_type(from));
}
