// Typed member resolution and publication; expression dispatch owns entry.
#include <stdlib.h>
#include <string.h>
#include "type_checker_internal.h"
#include "type_checker_visibility.h"
#include "function_param_publication_owner.h"
#include "diag_codes.h"

static Type *
expr_resolve_type_ref(ASTNode *type_ref, SemanticContext *ctx)
{
    Type *resolved =
        semantic_type_resolution_lookup_metadata_type_ref(ctx, type_ref);
    return resolved != NULL ? resolved : TYPE_UNKNOWN;
}
static void
expr_report_unknown_member(SemanticContext *ctx, ASTNode *site,
                           const Type *object_type, const char *field_name)
{
    const char *type_name = type_name_or_unknown(object_type);
    const char *member_name = field_name != NULL ? field_name : "<member>";
    semantic_error_with_hints(ctx, PGY_CODE_SEM_UNDEFINED_SYMBOL,
        PGY_CAUSE_SYMBOL_UNDEFINED, PGY_FIX_IMPORT_OR_DECLARE_SYMBOL, site,
        "Unknown member '%s.%s'.\n"
        "Reason:\n"
        "- the receiver type is known, but no field or overlay member with this name is declared\n"
        "- method calls are resolved through the call path; value-position member access requires a field\n"
        "Fix:\n"
        "- declare field '%s' on '%s'\n"
        "- or call an existing method with parentheses if this was meant to be an action",
        type_name,
        member_name,
        member_name,
        type_name);
}
Type *
type_check_member_access(ASTNode *expr, SemanticContext *ctx)
{
    ASTNode *member_object = ast_member_object(expr);
    const char *member_name = ast_member_name(expr);

    if (expr_member_is_static_access(expr)) {
        char *flat_name = flatten_static_member_access(expr, '_');
        char *display_name = flatten_static_member_access(expr, '.');
        Symbol *sym = flat_name != NULL ? scope_lookup(ctx->scope, flat_name) : NULL;
        if (sym == NULL && member_name != NULL)
            sym = scope_lookup(ctx->scope, member_name);
        if (sym != NULL) {
            sym->is_used = true;
            free(flat_name);
            free(display_name);
            if (semantic_param_publication_pending(ctx) &&
                !semantic_param_publication_record_value(ctx, expr, sym->type, true))
                semantic_error(ctx, expr, "Checked static value publication failed");
            return sym->type;
        }
        semantic_error_with_hints(ctx, PGY_CODE_SEM_UNDEFINED_SYMBOL, PGY_CAUSE_SYMBOL_UNDEFINED, PGY_FIX_IMPORT_OR_DECLARE_SYMBOL, expr,
            "Undefined symbol '%s' (check namespace spelling or export visibility)",
            display_name != NULL ? display_name : "<member>");
        free(flat_name);
        free(display_name);
        return TYPE_UNKNOWN;
    }

    Type *object_type = type_check_expression(member_object, ctx);

    if (semantic_collection_reject_moved_member_use(expr, ctx))
        return TYPE_UNKNOWN;

    Type *projection_field = expr_ops_projection_member(expr, member_object,
        member_name, object_type, ctx);
    if (projection_field != NULL)
        return projection_field;

    if ((type_is_constructed_named(object_type, "Array")
         || type_is_constructed_named(object_type, "Slice"))
        && strcmp(member_name, "Length") == 0) {
        return TYPE_INT;
    }

    if (object_type != NULL && object_type->kind == TYPE_KIND_ENUM) {
        Type *variant_payload = expr_type_for_enum_variant_projection(ctx,
            expr, object_type, member_name);
        if (variant_payload != NULL)
            return variant_payload;
    }

    if (object_type != NULL && object_type->kind == TYPE_KIND_CLASS
        && object_type->name != NULL && strchr(object_type->name, '$') != NULL) {
        Type *payload_field = expr_type_for_enum_payload_field(ctx, expr,
            object_type, member_name);
        if (payload_field != NULL)
            return payload_field;
    }

    /* Resolve nominal/domain field types through the shared host-decl seam. */
    if (object_type != NULL
        && (object_type->kind == TYPE_KIND_CLASS
            || object_type->kind == TYPE_KIND_ENUM
            || object_type->kind == TYPE_KIND_CONSTRUCTED)
        && object_type->name != NULL) {
        const char *field_name = member_name;
        ASTNode *decl = semantic_host_decl_for_type(ctx, object_type);

        if (decl != NULL && decl->type == AST_CLASS_DECL) {
            size_t field_count = projection_source_field_count(decl);
            for (size_t fi = 0; fi < field_count; fi++) {
                PgyDeclField cf = projection_source_field_at(decl, fi);
                if (cf.name == NULL)
                    continue;
                if (strcmp(cf.name, field_name) == 0) {
                    if (!explicit_member_access_allowed(decl,
                            object_type,
                            cf.access,
                            cf.has_explicit_access,
                            ctx)) {
                        semantic_error_with_hints(ctx, PGY_CODE_SEM_VISIBILITY_BOUNDARY, PGY_CAUSE_VISIBILITY_BOUNDARY_CROSS, PGY_FIX_WIDEN_VISIBILITY_OR_MOVE_CALLER, expr,
                            "Member '%s.%s' is not accessible across the current visibility boundary",
                            object_type->name,
                            field_name);
                        return TYPE_UNKNOWN;
                    }
                    return expr_host_method_generic_substitute(
                        expr_resolve_type_ref(cf.type_ast, ctx), decl,
                        object_type, NULL);
                }
            }
        } else if (decl != NULL) {
            if (decl->type == AST_WORLD_DECL) {
                size_t roster_count = 0;
                ASTNode **rosters = ast_world_rosters(decl, &roster_count);
                for (size_t i = 0; i < roster_count; i++) {
                    ASTNode *slot = rosters[i];
                    const char *slot_name = ast_world_roster_slot_name(slot);
                    if (slot != NULL && slot_name != NULL
                        && strcmp(slot_name, field_name) == 0) {
                        return semantic_type_resolution_lookup_metadata_name_or_alias_or_unknown(
                            ctx, ast_world_roster_type_name(slot), slot);
                    }
                }
                size_t zone_count = 0;
                ASTNode **zones = ast_world_zones(decl, &zone_count);
                for (size_t i = 0; i < zone_count; i++) {
                    ASTNode *slot = zones[i];
                    const char *slot_name = ast_world_zone_slot_name(slot);
                    if (slot != NULL && slot_name != NULL
                        && strcmp(slot_name, field_name) == 0) {
                        return semantic_type_resolution_lookup_metadata_name_or_alias_or_unknown(
                            ctx, ast_world_zone_type_name(slot), slot);
                    }
                }
            }
            if (decl->type == AST_ZONE_DECL) {
                size_t layer_slot_count = 0;
                ASTNode **layer_slots = ast_zone_layer_slots(
                    decl, &layer_slot_count);
                for (size_t i = 0; i < layer_slot_count; i++) {
                    ASTNode *slot = layer_slots[i];
                    if (slot != NULL
                        && ast_zone_layer_slot_name(slot) != NULL
                        && strcmp(ast_zone_layer_slot_name(slot),
                                  field_name) == 0) {
                        return semantic_type_resolution_lookup_metadata_name_or_alias_or_unknown(
                            ctx, ast_zone_layer_slot_layer_type(slot), slot);
                    }
                }
            }

            if (decl->type == AST_PARTY_DECL) {
                size_t party_role_count = ast_party_role_count(decl);
                for (size_t i = 0; i < party_role_count; i++) {
                    ASTNode *slot = ast_party_role(decl, i);
                    const char *party_slot_name = ast_role_slot_name(slot);
                    if (party_slot_name == NULL
                        || strcmp(party_slot_name, field_name) != 0)
                        continue;
                    if (ast_role_slot_required_ability_count(slot) > 0) {
                        ASTNode *ability_ref =
                            ast_role_slot_required_ability(slot, 0);
                        const char *ability_name = ability_ref != NULL
                            ? ast_type_name(ability_ref)
                            : NULL;
                        if (ability_name != NULL)
                            return create_overlay_nominal_type(ability_name);
                    }
                    return TYPE_UNKNOWN;
                }
            }

            for (size_t fi = 0; fi < overlay_field_count(decl); fi++) {
                const char *overlay_field_name = NULL;
                ASTNode *field_type_node = overlay_field_decl_at(decl, fi,
                    &overlay_field_name);
                if (overlay_field_name != NULL
                    && strcmp(overlay_field_name, field_name) == 0) {
                    return expr_resolve_type_ref(field_type_node, ctx);
                }
            }
        }

        expr_report_unknown_member(ctx, expr, object_type, field_name);
        return TYPE_UNKNOWN;
    }

    if (object_type != NULL
        && (object_type->kind == TYPE_KIND_CLASS
            || object_type->kind == TYPE_KIND_ENUM)) {
        expr_report_unknown_member(ctx, expr, object_type,
            member_name);
        return TYPE_UNKNOWN;
    }

    return TYPE_UNKNOWN;
}
