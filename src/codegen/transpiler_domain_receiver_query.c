/*
 * Copyright (c) 2026 Pergyra Language Project
 * Domain receiver queries shared by C backend projection/world lowering.
 */

#include "transpiler_domain_receiver_query.h"

#include <string.h>

#include "../parser/ast_api.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_nominal.h"
#include "transpiler_projection.h"
#include "transpiler_symbols.h"

static const char *
transpiler_zone_subject_slot_type_name(TranspilerCtx *ctx,
                                       ASTNode *zone_decl,
                                       const char *slot_name)
{
    const char *zone_name;
    TranspilerHostedDomainSlotView slot_view;

    if (ctx == NULL || zone_decl == NULL || zone_decl->type != AST_ZONE_DECL
        || slot_name == NULL) {
        return NULL;
    }

    zone_name = transpiler_decl_name_local(zone_decl);
    slot_view = transpiler_hosted_domain_slot_view_from_decl(ctx, zone_name,
        zone_decl);
    if (transpiler_hosted_domain_slot_view_missing_mir_metadata(&slot_view)) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing zone subject-slot receiver metadata for '%s'",
            zone_name != NULL ? zone_name : "(anonymous-zone)");
        return NULL;
    }

    for (size_t i = 0; i < slot_view.count; i++) {
        const char *candidate_name =
            transpiler_hosted_domain_slot_view_name(&slot_view, i);
        if (candidate_name == NULL || strcmp(candidate_name, slot_name) != 0)
            continue;
        if (!transpiler_hosted_domain_slot_view_is_subject_like(
                &slot_view, i)) {
            return NULL;
        }
        return transpiler_hosted_domain_slot_view_type_name(&slot_view, i);
    }
    return NULL;
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
        type_name = transpiler_zone_subject_slot_type_name(ctx, zone_decl,
            slot_name);
        if (type_name == NULL) {
            const char *var_type =
                transpiler_resolve_nominal_host_expr_type_name(ctx, receiver);
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
        type_name = transpiler_zone_subject_slot_type_name(ctx, zone_decl,
            slot_name);
        if (type_name == NULL) {
            const char *member_type =
                transpiler_resolve_nominal_host_expr_type_name(ctx, receiver);
            if (member_type != NULL && is_subject_type_name(ctx, member_type))
                type_name = member_type;
        }
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
    type_name = transpiler_zone_subject_slot_type_name(ctx, zone_decl,
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
