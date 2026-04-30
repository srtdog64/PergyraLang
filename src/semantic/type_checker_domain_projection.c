#include "type_checker_internal.h"
#include "diag_codes.h"

#include <stdlib.h>
#include <string.h>

static ASTNode *
find_named_class_decl_local(ASTNode *program, const char *name)
{
    if (program == NULL || program->type != AST_PROGRAM || name == NULL)
        return NULL;

    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        if (stmt == NULL || stmt->type != AST_CLASS_DECL
            || stmt->data.class_decl.name == NULL) {
            continue;
        }
        if (strcmp(stmt->data.class_decl.name, name) == 0)
            return stmt;
    }

    return NULL;
}

bool
type_check_projection_contract(ASTNode **slots,
                               size_t slot_count,
                               const char *owner_label,
                               const char *owner_name,
                               ASTNode *site,
                               const char *object_slot_name,
                               const char *source_slot_name,
                               SemanticContext *ctx,
                               const char *action_name)
{
    ASTNode *object_slot;
    ASTNode *source_slot;
    bool requires_dto;
    Type *target_type;
    Type *source_type;
    ASTNode *target_decl;
    ASTNode *source_decl;

    object_slot = find_domain_slot_local(slots, slot_count, object_slot_name);
    source_slot = find_domain_slot_local(slots, slot_count, source_slot_name);
    if (object_slot == NULL || source_slot == NULL || ctx == NULL)
        return false;

    if (object_slot->data.domain_slot.is_subject
        || object_slot->data.domain_slot.is_vessel) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, site,
            "%s %s target slot '%s' is not a projection slot.\n"
            "Contract source:\n"
            "- target slot '%s' on %s '%s'\n"
            "- source slot '%s' is driving this %s path\n"
            "Reason:\n"
            "- source slot '%s' is driving this %s path\n"
            "- %s only writes into object/tobject projection targets\n"
            "- subject/vessel slots are ownership anchors, not projection sinks\n"
            "- current owner surface is '%s'\n"
            "Fix:\n"
            "- choose an object slot for local projection sync\n"
            "- or choose a tobject slot for boundary publication",
            owner_label, action_name,
            object_slot_name != NULL ? object_slot_name : "<unknown>",
            object_slot_name != NULL ? object_slot_name : "<unknown>",
            owner_label,
            owner_name != NULL ? owner_name : "<owner>",
            source_slot_name != NULL ? source_slot_name : "<unknown>",
            action_name,
            source_slot_name != NULL ? source_slot_name : "<unknown>",
            action_name,
            action_name,
            owner_label);
        return true;
    }
    requires_dto =
        site != NULL && site->type == AST_ZONE_REFRESH
        && (site->data.zone_refresh.derive_target_kind
            ? object_slot->data.domain_slot.is_tobject
            : site->data.zone_refresh.requires_dto);
    if (requires_dto && !object_slot->data.domain_slot.is_tobject) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, site,
            "%s %s target slot '%s' must be a tobject slot.\n"
            "Contract source:\n"
            "- target slot '%s' on %s '%s'\n"
            "- source slot '%s' is driving this %s path\n"
            "Reason:\n"
            "- source slot '%s' is driving this %s path\n"
            "- publish writes a boundary transfer snapshot\n"
            "- object slots are local projections and cannot be published as transfer targets\n"
            "- current owner surface is '%s'\n"
            "Fix:\n"
            "- change '%s' to a tobject slot\n"
            "- or use refresh/bind into an object slot instead",
            owner_label, action_name,
            object_slot_name != NULL ? object_slot_name : "<unknown>",
            object_slot_name != NULL ? object_slot_name : "<unknown>",
            owner_label,
            owner_name != NULL ? owner_name : "<owner>",
            source_slot_name != NULL ? source_slot_name : "<unknown>",
            action_name,
            source_slot_name != NULL ? source_slot_name : "<unknown>",
            action_name,
            owner_label,
            object_slot_name != NULL ? object_slot_name : "<slot>");
        return true;
    }
    target_type = domain_resolve_slot_type(object_slot, ctx);
    source_type = domain_resolve_slot_type(source_slot, ctx);
    if (target_type == NULL || source_type == NULL
        || target_type == TYPE_UNKNOWN || source_type == TYPE_UNKNOWN) {
        return true;
    }

    target_decl = find_named_class_decl_local(ctx->program_root, target_type->name);
    if (target_decl == NULL || !target_decl->data.class_decl.is_struct) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, site,
            "%s %s target slot '%s' does not use a projection declaration.\n"
            "Contract source:\n"
            "- target slot '%s' on %s '%s'\n"
            "- source slot '%s' is driving this %s path\n"
            "Reason:\n"
            "- source slot '%s' is driving this %s path\n"
            "- projection sync requires an object/tobject nominal target\n"
            "- '%s' is not an object/tobject declaration\n"
            "- target slot '%s' is declared on %s '%s'\n"
            "Fix:\n"
            "- change the target slot type to an object or tobject declaration\n"
            "- or move this data through a plain assignment path instead",
            owner_label, action_name,
            object_slot_name != NULL ? object_slot_name : "<unknown>",
            object_slot_name != NULL ? object_slot_name : "<unknown>",
            owner_label,
            owner_name != NULL ? owner_name : "<owner>",
            source_slot_name != NULL ? source_slot_name : "<unknown>",
            action_name,
            source_slot_name != NULL ? source_slot_name : "<unknown>",
            action_name,
            target_type->name != NULL ? target_type->name : "<unknown>",
            object_slot_name != NULL ? object_slot_name : "<unknown>",
            owner_label,
            owner_name != NULL ? owner_name : "<owner>");
        return true;
    }
    if (site != NULL && site->type == AST_ZONE_REFRESH) {
        NominalDeclKind expected_kind =
            requires_dto ? NOMINAL_DECL_TOBJECT : NOMINAL_DECL_OBJECT;
        const char *expected_label =
            requires_dto ? "tobject" : "object";

        if (target_decl->data.class_decl.nominal_kind != expected_kind) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, site,
                "%s %s target slot '%s' uses the wrong projection kind.\n"
                "Contract source:\n"
                "- target slot '%s' on %s '%s'\n"
                "- source slot '%s' is driving this %s path\n"
                "Reason:\n"
                "- source slot '%s' is driving this %s path\n"
                "- %s requires target slot '%s' to use %s declaration\n"
                "- actual target type '%s' uses a different projection kind\n"
                "- target projection declaration currently comes from '%s'\n"
                "Fix:\n"
                "- change slot '%s' to use %s declaration\n"
                "- or switch to the matching projection operation",
                owner_label, action_name,
                object_slot_name != NULL ? object_slot_name : "<unknown>",
                object_slot_name != NULL ? object_slot_name : "<unknown>",
                owner_label,
                owner_name != NULL ? owner_name : "<owner>",
                source_slot_name != NULL ? source_slot_name : "<unknown>",
                action_name,
                source_slot_name != NULL ? source_slot_name : "<unknown>",
                action_name,
                action_name,
                object_slot_name != NULL ? object_slot_name : "<slot>",
                expected_label,
                target_type->name != NULL ? target_type->name : "<unknown>",
                target_type->name != NULL ? target_type->name : "<unknown>",
                object_slot_name != NULL ? object_slot_name : "<slot>",
                expected_label);
            return true;
        }
    }

    if (source_slot->data.domain_slot.is_tobject) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, site,
            "%s %s source slot '%s' cannot be a tobject slot.\n"
            "Contract source:\n"
            "- target slot '%s' on %s '%s'\n"
            "- source slot '%s' is driving this %s path\n"
            "Reason:\n"
            "- projection consumer path is target slot '%s' <- source slot '%s'\n"
            "- tobject is already a boundary snapshot\n"
            "- projection sync must read from a subject/object source, not from a transfer snapshot\n"
            "- source slot '%s' currently resolves to tobject '%s'\n"
            "- projection contract originates from target slot '%s' on %s '%s'\n"
            "Fix:\n"
            "- use the original subject/object slot as the source\n"
            "- or materialize a new object/tobject from the original source instead",
            owner_label, action_name,
            source_slot_name != NULL ? source_slot_name : "<unknown>",
            object_slot_name != NULL ? object_slot_name : "<unknown>",
            owner_label != NULL ? owner_label : "<owner>",
            owner_name != NULL ? owner_name : "<owner>",
            source_slot_name != NULL ? source_slot_name : "<unknown>",
            action_name,
            object_slot_name != NULL ? object_slot_name : "<unknown>",
            source_slot_name != NULL ? source_slot_name : "<unknown>",
            source_slot_name != NULL ? source_slot_name : "<unknown>",
            source_type != NULL && source_type->name != NULL
                ? source_type->name
                : "<unknown>",
            object_slot_name != NULL ? object_slot_name : "<unknown>",
            owner_label != NULL ? owner_label : "<owner>",
            owner_name != NULL ? owner_name : "<owner>");
        return true;
    }

    source_decl = find_subject_host_decl_by_name(ctx->program_root, source_type->name);
    if (!decl_is_projection_source(source_decl))
        source_decl = find_named_class_decl_local(ctx->program_root, source_type->name);
    if (!decl_is_projection_source(source_decl)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, site,
            "%s %s source slot '%s' is not a valid projection source.\n"
            "Contract source:\n"
            "- target slot '%s' on %s '%s'\n"
            "- source slot '%s' is driving this %s path\n"
            "Reason:\n"
            "- projection consumer path is target slot '%s' <- source slot '%s'\n"
            "- projection sync reads fields from a subject/object declaration\n"
            "- this slot does not point at a subject/object source host\n"
            "- resolved source type is '%s'\n"
            "- projection contract originates from target slot '%s' on %s '%s'\n"
            "Fix:\n"
            "- use a subject slot as the source\n"
            "- or use an object slot that mirrors the desired fields",
            owner_label, action_name,
            source_slot_name != NULL ? source_slot_name : "<unknown>",
            object_slot_name != NULL ? object_slot_name : "<unknown>",
            owner_label != NULL ? owner_label : "<owner>",
            owner_name != NULL ? owner_name : "<owner>",
            source_slot_name != NULL ? source_slot_name : "<unknown>",
            action_name,
            object_slot_name != NULL ? object_slot_name : "<unknown>",
            source_slot_name != NULL ? source_slot_name : "<unknown>",
            source_type != NULL && source_type->name != NULL
                ? source_type->name
                : "<unknown>",
            object_slot_name != NULL ? object_slot_name : "<unknown>",
            owner_label != NULL ? owner_label : "<owner>",
            owner_name != NULL ? owner_name : "<owner>");
        return true;
    }

    if (site != NULL && site->type == AST_ZONE_REFRESH
        && site->data.zone_refresh.field_map_count > 0) {
        for (size_t i = 0; i < site->data.zone_refresh.field_map_count; i++) {
            const char *mapped_target =
                site->data.zone_refresh.mapped_target_fields[i];

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

            for (size_t j = i + 1; j < site->data.zone_refresh.field_map_count; j++) {
                const char *other_target =
                    site->data.zone_refresh.mapped_target_fields[j];
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

    for (size_t i = 0; i < target_decl->data.class_decl.field_count; i++) {
        ClassField *target_field = target_decl->data.class_decl.fields[i];
        Type *target_field_type;
        Type *source_field_type;
        const char *source_field_name;
        char *source_path = NULL;
        int source_status;

        if (target_field == NULL || target_field->name == NULL
            || target_field->type == NULL) {
            continue;
        }

        source_field_name = projection_refresh_source_field_name(site,
            target_field->name);
        source_status = resolve_projection_source_field_path(
            ctx->program_root, source_decl, source_field_name, ctx,
            &source_path, &source_field_type);
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
                target_field->name,
                source_slot_name != NULL ? source_slot_name : "<unknown>",
                object_slot_name != NULL ? object_slot_name : "<unknown>",
                source_slot_name != NULL ? source_slot_name : "<unknown>",
                target_type != NULL && target_type->name != NULL
                    ? target_type->name
                    : "<unknown>",
                object_slot_name != NULL ? object_slot_name : "<unknown>",
                source_slot_name != NULL ? source_slot_name : "<unknown>",
                source_field_name != NULL ? source_field_name : target_field->name,
                source_field_name != NULL ? source_field_name : target_field->name,
                source_type != NULL && source_type->name != NULL
                    ? source_type->name
                    : "<unknown>");
            continue;
        }
        if (source_status == 0 || source_field_type == NULL) {
            if (source_field_name != NULL
                && strcmp(source_field_name, target_field->name) != 0) {
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
                    target_field->name,
                    source_field_name,
                    source_slot_name != NULL ? source_slot_name : "<unknown>",
                    object_slot_name != NULL ? object_slot_name : "<unknown>",
                    owner_label != NULL ? owner_label : "<owner>",
                    owner_name != NULL ? owner_name : "<owner>",
                    source_slot_name != NULL ? source_slot_name : "<unknown>",
                    action_name,
                    object_slot_name != NULL ? object_slot_name : "<unknown>",
                    source_slot_name != NULL ? source_slot_name : "<unknown>",
                    target_field->name,
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
                    target_field->name,
                    source_slot_name != NULL ? source_slot_name : "<unknown>",
                    object_slot_name != NULL ? object_slot_name : "<unknown>",
                    owner_label != NULL ? owner_label : "<owner>",
                    owner_name != NULL ? owner_name : "<owner>",
                    source_slot_name != NULL ? source_slot_name : "<unknown>",
                    action_name,
                    object_slot_name != NULL ? object_slot_name : "<unknown>",
                    source_slot_name != NULL ? source_slot_name : "<unknown>",
                    object_slot_name != NULL ? object_slot_name : "<unknown>",
                    target_field->name,
                    source_type != NULL && source_type->name != NULL
                        ? source_type->name
                        : "<unknown>",
                    target_field->name,
                    source_type != NULL && source_type->name != NULL
                        ? source_type->name
                        : "<unknown>");
            }
            continue;
        }

        target_field_type = domain_resolve_named_type_ref(target_field->type, ctx);
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
                target_field->name,
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
                target_field->name,
                type_name_or_unknown(target_field_type),
                source_path != NULL ? source_path : source_field_name,
                source_slot_name != NULL ? source_slot_name : "<unknown>",
                type_name_or_unknown(source_field_type),
                target_field->name,
                type_name_or_unknown(source_field_type),
                target_field->name,
                type_name_or_unknown(target_field_type));
            free(source_path);
            continue;
        }
        free(source_path);
    }

    return true;
}

bool
type_check_zone_projection_contract(ASTNode *zone,
                                    ASTNode *site,
                                    const char *object_slot_name,
                                    const char *source_slot_name,
                                    SemanticContext *ctx,
                                    const char *action_name)
{
    if (zone == NULL)
        return false;
    return type_check_projection_contract(zone->data.zone_decl.slots,
        zone->data.zone_decl.slot_count, "Zone",
        zone->data.zone_decl.name, site,
        object_slot_name, source_slot_name, ctx, action_name);
}
