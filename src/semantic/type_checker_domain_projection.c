#include "type_checker_internal.h"
#include "diag_codes.h"
#include "parser/ast_api.h"

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

    if (ast_domain_slot_is_subject(object_slot)
        || ast_domain_slot_is_vessel(object_slot)) {
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
        && (ast_zone_refresh_derives_target_kind(site)
            ? ast_domain_slot_is_tobject(object_slot)
            : ast_zone_refresh_requires_dto(site));
    if (requires_dto && !ast_domain_slot_is_tobject(object_slot)) {
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
    target_type = domain_lookup_slot_type_metadata(object_slot, ctx);
    source_type = domain_lookup_slot_type_metadata(source_slot, ctx);
    if (target_type == NULL || source_type == NULL
        || target_type == TYPE_UNKNOWN || source_type == TYPE_UNKNOWN) {
        return true;
    }

    target_decl = semantic_find_class_decl_by_name(ctx, target_type->name);
    if (target_decl == NULL || !ast_class_is_struct(target_decl)) {
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

        if (ast_class_nominal_kind(target_decl) != expected_kind) {
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

    if (ast_domain_slot_is_tobject(source_slot)) {
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

    source_decl = semantic_host_decl_for_type(ctx, source_type);
    if (!decl_is_projection_source(source_decl))
        source_decl = semantic_find_class_decl_by_name(ctx, source_type->name);
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

    return type_check_projection_field_contracts(target_decl, source_decl,
        target_type, source_type, owner_label, owner_name, site,
        object_slot_name, source_slot_name, ctx, action_name);
}

bool
type_check_zone_projection_contract(ASTNode *zone,
                                    ASTNode *site,
                                    const char *object_slot_name,
                                    const char *source_slot_name,
                                    SemanticContext *ctx,
                                    const char *action_name)
{
    size_t slot_count = 0;
    ASTNode **slots;

    if (zone == NULL)
        return false;
    slots = ast_zone_slots(zone, &slot_count);
    return type_check_projection_contract(slots, slot_count, "Zone",
        ast_zone_name(zone), site,
        object_slot_name, source_slot_name, ctx, action_name);
}
