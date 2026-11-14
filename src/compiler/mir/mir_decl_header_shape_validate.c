#include "mir_decl_header_shape_validate.h"

#include "mir_fact_validate_internal.h"

static bool
mir_decl_header_type_requires_pointer_self(ASTNodeType type)
{
    switch (type) {
    case AST_PARTY_DECL:
    case AST_ROSTER_DECL:
    case AST_WORLD_DECL:
    case AST_RELATION_DECL:
    case AST_EFFECT_DECL:
    case AST_ROLE_DECL:
    case AST_ZONE_DECL:
        return true;
    default:
        return false;
    }
}

bool
mir_validate_decl_header_shape_metadata(const MIRDeclHeader *header,
                                        size_t header_index,
                                        char **error_message)
{
    if (header == NULL)
        return false;
    if (header->name == NULL) {
        if (error_message != NULL) {
            *error_message = mir_fact_strdup_fmt(
                "MIR declaration header[%zu] has no declaration name metadata",
                header_index);
        }
        return false;
    }
    if (header->ast_type == AST_PROGRAM) {
        if (error_message != NULL) {
            *error_message = mir_fact_strdup_fmt(
                "MIR declaration header[%zu] '%s' has invalid declaration type metadata",
                header_index, header->name);
        }
        return false;
    }
    if (header->ast_type == AST_TYPE_ALIAS
        && header->type_alias_target_type_name == NULL) {
        if (error_message != NULL) {
            *error_message = mir_fact_strdup_fmt(
                "MIR declaration header[%zu] '%s' type-alias target metadata drift",
                header_index, header->name);
        }
        return false;
    }
    if (header->ast_type == AST_INTENT_DECL
        && header->intent_retry_count < 0) {
        if (error_message != NULL) {
            *error_message = mir_fact_strdup_fmt(
                "MIR declaration header[%zu] '%s' intent retry metadata drift",
                header_index, header->name);
        }
        return false;
    }
    if (mir_decl_header_type_requires_pointer_self(header->ast_type)
        && !header->uses_pointer_self) {
        if (error_message != NULL) {
            *error_message = mir_fact_strdup_fmt(
                "MIR declaration header[%zu] '%s' pointer-self ABI metadata drift",
                header_index, header->name);
        }
        return false;
    }
    if (header->ast_type == AST_CLASS_DECL
        && (header->nominal_kind == NOMINAL_DECL_SUBJECT
            || header->nominal_kind == NOMINAL_DECL_VESSEL)
        && !header->uses_pointer_self) {
        if (error_message != NULL) {
            *error_message = mir_fact_strdup_fmt(
                "MIR declaration header[%zu] '%s' nominal pointer-self metadata drift",
                header_index, header->name);
        }
        return false;
    }
    return true;
}
