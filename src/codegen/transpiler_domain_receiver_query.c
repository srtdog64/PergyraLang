/*
 * Copyright (c) 2026 Pergyra Language Project
 * Domain receiver queries shared by C backend projection/world lowering.
 */

#include "transpiler_domain_receiver_query.h"

#include <string.h>

#include "../parser/ast_api.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_projection.h"
#include "transpiler_symbols.h"

static const char *
transpiler_zone_subject_slot_type_name(ASTNode *zone_decl,
                                       const char *slot_name)
{
    ASTNode *slot = transpiler_find_zone_domain_slot(zone_decl, slot_name);
    ASTNode *slot_type;

    if (slot == NULL || slot->type != AST_DOMAIN_SLOT
        || !ast_domain_slot_is_subject(slot)) {
        return NULL;
    }

    slot_type = ast_domain_slot_type(slot);
    if (slot_type == NULL || slot_type->type != AST_TYPE)
        return NULL;
    return ast_type_name(slot_type);
}

ASTNode *
transpiler_find_subject_host_method_decl(TranspilerCtx *ctx,
                                         const char *type_name,
                                         const char *method_name)
{
    ASTNode *decl;
    ASTNode *method;

    if (ctx == NULL || type_name == NULL || method_name == NULL)
        return NULL;

    decl = find_subject_host_decl(ctx, type_name);
    if (decl == NULL || decl->type != AST_CLASS_DECL)
        return NULL;

    method = find_nominal_host_method_decl(ctx, type_name, method_name);
    if (method == NULL || method->type != AST_FUNC_DECL)
        return NULL;
    return method;
}

bool
transpiler_resolve_zone_subject_receiver(TranspilerCtx *ctx,
                                         ASTNode *receiver,
                                         const char **slot_name_out,
                                         const char **type_name_out)
{
    ASTNode *zone_decl;
    const char *slot_name = NULL;
    const char *type_name = NULL;

    if (slot_name_out != NULL)
        *slot_name_out = NULL;
    if (type_name_out != NULL)
        *type_name_out = NULL;

    if (ctx == NULL || receiver == NULL)
        return false;

    zone_decl = transpiler_current_host_decl_local(ctx);
    if (zone_decl != NULL && zone_decl->type != AST_ZONE_DECL)
        zone_decl = NULL;
    if (zone_decl == NULL)
        return false;

    if (receiver->type == AST_IDENTIFIER
        && ast_identifier_name(receiver) != NULL) {
        slot_name = ast_identifier_name(receiver);
        type_name = transpiler_zone_subject_slot_type_name(zone_decl,
            slot_name);
        if (type_name == NULL) {
            const char *var_type = lookup_typed_var(ctx, slot_name);
            if (var_type != NULL && is_subject_type_name(ctx, var_type))
                type_name = var_type;
        }
    } else if (receiver->type == AST_MEMBER_ACCESS
               && ast_member_object(receiver) != NULL
               && ast_member_object(receiver)->type == AST_IDENTIFIER
               && ast_identifier_name(ast_member_object(receiver)) != NULL
               && strcmp(ast_identifier_name(ast_member_object(receiver)),
                   "self") == 0
               && ast_member_name(receiver) != NULL) {
        slot_name = ast_member_name(receiver);
        type_name = transpiler_zone_subject_slot_type_name(zone_decl,
            slot_name);
    }

    if (slot_name == NULL || type_name == NULL)
        return false;

    if (slot_name_out != NULL)
        *slot_name_out = slot_name;
    if (type_name_out != NULL)
        *type_name_out = type_name;
    return true;
}

bool
transpiler_resolve_world_zone_subject_receiver(
    TranspilerCtx *ctx,
    ASTNode *receiver,
    const char **zone_slot_name_out,
    const char **zone_type_name_out,
    const char **slot_name_out,
    const char **type_name_out)
{
    ASTNode *world_decl;
    ASTNode *zone_decl;
    ASTNode *zone_expr;
    const char *zone_slot_name = NULL;
    const char *zone_type_name = NULL;
    const char *slot_name = NULL;
    const char *type_name = NULL;

    if (zone_slot_name_out != NULL)
        *zone_slot_name_out = NULL;
    if (zone_type_name_out != NULL)
        *zone_type_name_out = NULL;
    if (slot_name_out != NULL)
        *slot_name_out = NULL;
    if (type_name_out != NULL)
        *type_name_out = NULL;

    if (ctx == NULL
        || receiver == NULL || receiver->type != AST_MEMBER_ACCESS) {
        return false;
    }

    zone_expr = ast_member_object(receiver);
    slot_name = ast_member_name(receiver);
    if (zone_expr == NULL || slot_name == NULL)
        return false;

    if (zone_expr->type == AST_IDENTIFIER
        && ast_identifier_name(zone_expr) != NULL) {
        zone_slot_name = ast_identifier_name(zone_expr);
    } else if (zone_expr->type == AST_MEMBER_ACCESS
               && ast_member_object(zone_expr) != NULL
               && ast_member_object(zone_expr)->type == AST_IDENTIFIER
               && ast_identifier_name(ast_member_object(zone_expr)) != NULL
               && strcmp(ast_identifier_name(ast_member_object(zone_expr)),
                   "self") == 0
               && ast_member_name(zone_expr) != NULL) {
        zone_slot_name = ast_member_name(zone_expr);
    } else {
        return false;
    }

    world_decl = transpiler_current_host_decl_local(ctx);
    if (world_decl != NULL && world_decl->type != AST_WORLD_DECL)
        world_decl = NULL;
    if (world_decl == NULL)
        return false;

    zone_decl = transpiler_resolve_world_zone_decl(ctx, world_decl,
        zone_slot_name);
    if (zone_decl == NULL)
        return false;

    zone_type_name = transpiler_decl_name_local(zone_decl);
    type_name = transpiler_zone_subject_slot_type_name(zone_decl,
        slot_name);
    if (zone_type_name == NULL || type_name == NULL)
        return false;

    if (zone_slot_name_out != NULL)
        *zone_slot_name_out = zone_slot_name;
    if (zone_type_name_out != NULL)
        *zone_type_name_out = zone_type_name;
    if (slot_name_out != NULL)
        *slot_name_out = slot_name;
    if (type_name_out != NULL)
        *type_name_out = type_name;
    return true;
}
