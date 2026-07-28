#include "type_checker_internal.h"
#include "diag_codes.h"
#include "parser/ast_api.h"

#include <string.h>

static bool
projection_target_field_has_explicit_map(ASTNode *site,
                                         const char *target_field_name)
{
    if (site == NULL || site->type != AST_ZONE_REFRESH
        || target_field_name == NULL) {
        return false;
    }
    for (size_t i = 0; i < ast_zone_refresh_field_map_count(site); i++) {
        const char *mapped_target =
            ast_zone_refresh_mapped_target_field(site, i);
        if (mapped_target != NULL
            && strcmp(mapped_target, target_field_name) == 0) {
            return true;
        }
    }
    return false;
}

bool
type_check_projection_field_contracts(ASTNode *target_decl,
                                      ASTNode *source_decl,
                                      ASTNode *owner_decl,
                                      ASTNode *projection_slot,
                                      ASTNode *source_slot,
                                      Type *target_type,
                                      Type *source_type,
                                      const char *owner_label,
                                      const char *owner_name,
                                      ASTNode *site,
                                      const char *object_slot_name,
                                      const char *source_slot_name,
                                      SemanticContext *ctx,
                                      const char *action_name)
{
    size_t target_field_count;

    if (target_decl == NULL || source_decl == NULL || ctx == NULL)
        return false;

    if (site != NULL && site->type == AST_ZONE_REFRESH
        && ast_zone_refresh_field_map_count(site) > 0) {
        for (size_t i = 0; i < ast_zone_refresh_field_map_count(site); i++) {
            const char *mapped_target =
                ast_zone_refresh_mapped_target_field(site, i);

            if (!projection_target_decl_has_field(target_decl, mapped_target)) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, site,
                    "%s %s projection map refers to unknown target field '%s' in slot '%s'.\n"
                    "Contract source:\n"
                    "- target slot '%s' on %s '%s'\n"
                    "- source slot '%s' is driving this %s path\n"
                    "Reason:\n"
                    "- projection consumer path is target slot '%s' <- source slot '%s'\n"
                    "- explicit field map entry is '%s <- %s'\n"
                    "- target projection declaration '%s' does not declare field '%s'\n"
                    "- %s cannot write source slot '%s' into a field that does not exist\n"
                    "- target contract comes from projection type '%s'\n"
                    "Fix:\n"
                    "- add field '%s' to projection declaration '%s'\n"
                    "- or remove/update the field map entry for '%s'",
                    owner_label, action_name,
                    mapped_target != NULL ? mapped_target : "<field>",
                    object_slot_name != NULL ? object_slot_name : "<unknown>",
                    object_slot_name != NULL ? object_slot_name : "<unknown>",
                    owner_label != NULL ? owner_label : "<owner>",
                    owner_name != NULL ? owner_name : "<owner>",
                    source_slot_name != NULL ? source_slot_name : "<unknown>",
                    action_name,
                    object_slot_name != NULL ? object_slot_name : "<unknown>",
                    source_slot_name != NULL ? source_slot_name : "<unknown>",
                    mapped_target != NULL ? mapped_target : "<field>",
                    projection_refresh_source_field_name(site, mapped_target) != NULL
                        ? projection_refresh_source_field_name(site, mapped_target)
                        : "<source-field>",
                    target_type != NULL && target_type->name != NULL
                        ? target_type->name
                        : "<unknown>",
                    mapped_target != NULL ? mapped_target : "<field>",
                    action_name,
                    source_slot_name != NULL ? source_slot_name : "<unknown>",
                    target_type != NULL && target_type->name != NULL
                        ? target_type->name
                        : "<unknown>",
                    mapped_target != NULL ? mapped_target : "<field>",
                    target_type != NULL && target_type->name != NULL
                        ? target_type->name
                        : "<unknown>",
                    mapped_target != NULL ? mapped_target : "<field>");
            }

            for (size_t j = i + 1; j < ast_zone_refresh_field_map_count(site); j++) {
                const char *other_target =
                    ast_zone_refresh_mapped_target_field(site, j);
                if (mapped_target != NULL && other_target != NULL
                    && strcmp(mapped_target, other_target) == 0) {
                    semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, site,
                        "%s %s projection map duplicates target field '%s'.\n"
                        "Contract source:\n"
                        "- target slot '%s' on %s '%s'\n"
                        "- source slot '%s' is driving this %s path\n"
                        "Reason:\n"
                        "- projection consumer path is target slot '%s' <- source slot '%s'\n"
                        "- each projection target field may be filled from exactly one source field\n"
                        "- duplicate map entries make the projection source ambiguous\n"
                        "- target projection slot is '%s'\n"
                        "Fix:\n"
                        "- keep a single mapping for '%s'\n"
                        "- or split the target into distinct projection fields",
                        owner_label, action_name, mapped_target,
                        object_slot_name != NULL ? object_slot_name : "<unknown>",
                        owner_label != NULL ? owner_label : "<owner>",
                        owner_name != NULL ? owner_name : "<owner>",
                        source_slot_name != NULL ? source_slot_name : "<unknown>",
                        action_name,
                        object_slot_name != NULL ? object_slot_name : "<unknown>",
                        source_slot_name != NULL ? source_slot_name : "<unknown>",
                        object_slot_name != NULL ? object_slot_name : "<unknown>",
                        mapped_target);
                }
            }
        }
    }

    target_field_count = projection_source_field_count(target_decl);
    for (size_t i = 0; i < target_field_count; i++) {
        PgyDeclField target_field =
            projection_source_field_at(target_decl, i);
        Type *target_field_type;
        Type *source_field_type;
        const char *source_field_name;
        const char *source_path = NULL;
        PgyDomainProjectionPathSegmentFact *source_path_segments = NULL;
        size_t source_path_segment_count = 0;
        int source_status;

        if (target_field.name == NULL
            || target_field.type_ast == NULL) {
            continue;
        }

        source_field_name = projection_refresh_source_field_name(site,
            target_field.name);
        source_status =
            semantic_resolve_projection_source_field_path_with_segments(
            ctx, source_decl, source_field_name,
            &source_path, &source_field_type,
            &source_path_segments, &source_path_segment_count);
        if (source_status < 0) {
            semantic_error(ctx, site,
                "%s %s projection path identity allocation failed closed",
                owner_label != NULL ? owner_label : "Domain",
                action_name != NULL ? action_name : "projection");
            pgy_domain_projection_path_segments_destroy(
                source_path_segments, source_path_segment_count);
            continue;
        }
        if (source_status == 2) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, site,
                "%s %s target field '%s' is ambiguous in source slot '%s'.\n"
                "Contract source:\n"
                "- projection consumer path is target slot '%s' <- source slot '%s'\n"
                "- target projection declaration is '%s'\n"
                "Reason:\n"
                "- projection consumer path is target slot '%s' <- source slot '%s'\n"
                "- requested source field/path is '%s'\n"
                "- multiple projection source paths match field '%s'\n"
                "- automatic projection cannot choose one path safely\n"
                "- source declaration '%s' exposes overlapping candidate paths\n"
                "Fix:\n"
                "- rename one of the source fields to make the path unique\n"
                "- or expose the desired value through a dedicated object/tobject field",
                owner_label, action_name,
                target_field.name,
                source_slot_name != NULL ? source_slot_name : "<unknown>",
                object_slot_name != NULL ? object_slot_name : "<unknown>",
                source_slot_name != NULL ? source_slot_name : "<unknown>",
                target_type != NULL && target_type->name != NULL
                    ? target_type->name
                    : "<unknown>",
                object_slot_name != NULL ? object_slot_name : "<unknown>",
                source_slot_name != NULL ? source_slot_name : "<unknown>",
                source_field_name != NULL ? source_field_name : target_field.name,
                source_field_name != NULL ? source_field_name : target_field.name,
                source_type != NULL && source_type->name != NULL
                    ? source_type->name
                    : "<unknown>");
            pgy_domain_projection_path_segments_destroy(
                source_path_segments, source_path_segment_count);
            continue;
        }
        if (source_status == 0 || source_field_type == NULL) {
            if (source_field_name != NULL
                && strcmp(source_field_name, target_field.name) != 0) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, site,
                    "%s %s target field '%s' maps from missing source field '%s' in slot '%s'.\n"
                    "Contract source:\n"
                    "- target slot '%s' on %s '%s'\n"
                    "- source slot '%s' is driving this %s path\n"
                    "Reason:\n"
                    "- projection consumer path is target slot '%s' <- source slot '%s'\n"
                    "- field map explicitly requests '%s <- %s'\n"
                    "- source declaration '%s' does not expose '%s'\n"
                    "- target projection declaration is '%s'\n"
                    "Fix:\n"
                    "- add source field '%s' to '%s'\n"
                    "- or change the map entry to a field that actually exists",
                    owner_label, action_name,
                    target_field.name,
                    source_field_name,
                    source_slot_name != NULL ? source_slot_name : "<unknown>",
                    object_slot_name != NULL ? object_slot_name : "<unknown>",
                    owner_label != NULL ? owner_label : "<owner>",
                    owner_name != NULL ? owner_name : "<owner>",
                    source_slot_name != NULL ? source_slot_name : "<unknown>",
                    action_name,
                    object_slot_name != NULL ? object_slot_name : "<unknown>",
                    source_slot_name != NULL ? source_slot_name : "<unknown>",
                    target_field.name,
                    source_field_name,
                    source_type != NULL && source_type->name != NULL
                        ? source_type->name
                        : "<unknown>",
                    source_field_name,
                    target_type != NULL && target_type->name != NULL
                        ? target_type->name
                        : "<unknown>",
                    source_field_name,
                    source_type != NULL && source_type->name != NULL
                        ? source_type->name
                        : "<unknown>");
            } else {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, site,
                    "%s %s target field '%s' is missing from source slot '%s'.\n"
                    "Contract source:\n"
                    "- target slot '%s' on %s '%s'\n"
                    "- source slot '%s' is driving this %s path\n"
                    "Reason:\n"
                    "- projection consumer path is target slot '%s' <- source slot '%s'\n"
                    "- projection target '%s' expects field '%s'\n"
                    "- source declaration '%s' does not expose a matching field\n"
                    "- automatic field matching therefore cannot derive a safe source path\n"
                    "Fix:\n"
                    "- add field '%s' to source declaration '%s'\n"
                    "- or add an explicit field map to a different existing source field",
                    owner_label, action_name,
                    target_field.name,
                    source_slot_name != NULL ? source_slot_name : "<unknown>",
                    object_slot_name != NULL ? object_slot_name : "<unknown>",
                    owner_label != NULL ? owner_label : "<owner>",
                    owner_name != NULL ? owner_name : "<owner>",
                    source_slot_name != NULL ? source_slot_name : "<unknown>",
                    action_name,
                    object_slot_name != NULL ? object_slot_name : "<unknown>",
                    source_slot_name != NULL ? source_slot_name : "<unknown>",
                    object_slot_name != NULL ? object_slot_name : "<unknown>",
                    target_field.name,
                    source_type != NULL && source_type->name != NULL
                        ? source_type->name
                        : "<unknown>",
                    target_field.name,
                    source_type != NULL && source_type->name != NULL
                        ? source_type->name
                        : "<unknown>");
            }
            pgy_domain_projection_path_segments_destroy(
                source_path_segments, source_path_segment_count);
            continue;
        }

        target_field_type = domain_lookup_named_type_metadata(target_field.type_ast, ctx);
        if (!type_is_assignable(source_field_type, target_field_type)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, site,
                "%s %s target field '%s' cannot accept source path '%s' from slot '%s'.\n"
                "Contract source:\n"
                "- target slot '%s' on %s '%s'\n"
                "- source slot '%s' is driving this %s path\n"
                "Reason:\n"
                "- projection consumer path is target slot '%s' <- source slot '%s'\n"
                "- projection target slot '%s' expects field '%s' to have type '%s'\n"
                "- resolved source path '%s' from slot '%s' has type '%s'\n"
                "- projection sync cannot derive a safe coercion across this field boundary\n"
                "Fix:\n"
                "- change target field '%s' to type '%s'\n"
                "- or map '%s' from a source field/path whose type matches '%s'",
                owner_label, action_name,
                target_field.name,
                source_path != NULL ? source_path : source_field_name,
                source_slot_name != NULL ? source_slot_name : "<unknown>",
                object_slot_name != NULL ? object_slot_name : "<unknown>",
                owner_label != NULL ? owner_label : "<owner>",
                owner_name != NULL ? owner_name : "<owner>",
                source_slot_name != NULL ? source_slot_name : "<unknown>",
                action_name,
                object_slot_name != NULL ? object_slot_name : "<unknown>",
                source_slot_name != NULL ? source_slot_name : "<unknown>",
                object_slot_name != NULL ? object_slot_name : "<unknown>",
                target_field.name,
                type_name_or_unknown(target_field_type),
                source_path != NULL ? source_path : source_field_name,
                source_slot_name != NULL ? source_slot_name : "<unknown>",
                type_name_or_unknown(source_field_type),
                target_field.name,
                type_name_or_unknown(source_field_type),
                target_field.name,
                type_name_or_unknown(target_field_type));
            pgy_domain_projection_path_segments_destroy(
                source_path_segments, source_path_segment_count);
            continue;
        }

        (void)semantic_record_domain_projection_member_assignment(
            ctx, owner_decl, site, projection_slot, source_slot,
            target_decl, target_field.declaration_syntax_id,
            target_field.name, target_field_type, source_decl,
            projection_target_field_has_explicit_map(site, target_field.name),
            source_path, source_field_type,
            source_path_segments, source_path_segment_count);
        source_path_segments = NULL;
        source_path_segment_count = 0;
    }

    return true;
}
