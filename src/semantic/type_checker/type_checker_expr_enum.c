#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "type_checker_internal.h"
#include "../function_param_flow/function_param_publication_internal.h"
#include "../diag_codes.h"
#include "../../common/string_compat.h"

static Type *
expr_enum_resolve_type_ref(ASTNode *type_ref, SemanticContext *ctx)
{
    Type *resolved =
        semantic_type_resolution_lookup_metadata_type_ref(ctx, type_ref);
    return resolved != NULL ? resolved : TYPE_UNKNOWN;
}

static bool
expr_parse_enum_payload_field_index(const char *field_name, size_t *index_out)
{
    size_t value = 0;

    if (index_out != NULL)
        *index_out = 0;
    if (field_name == NULL || field_name[0] != '_' || field_name[1] == '\0')
        return false;
    for (const char *p = field_name + 1; *p != '\0'; p++) {
        if (*p < '0' || *p > '9')
            return false;
        value = value * 10u + (size_t)(*p - '0');
    }
    if (index_out != NULL)
        *index_out = value;
    return true;
}

static ASTNode *
expr_find_enum_decl_by_name(SemanticContext *ctx, const char *enum_name)
{
    return semantic_find_enum_decl_by_name(ctx, enum_name);
}

bool semantic_param_publication_record_checked_unit_enum_selection(
    SemanticContext *ctx, ASTNode *member, ASTNode *enum_decl,
    const Type *enum_type, size_t variant_ordinal)
{
    if (!semantic_param_publication_pending(ctx) || member == NULL ||
        member->type != AST_MEMBER_ACCESS || enum_decl == NULL ||
        enum_decl->type != AST_ENUM_DECL || enum_type == NULL ||
        enum_type == TYPE_UNKNOWN || enum_type->kind != TYPE_KIND_ENUM)
        return false;
    ASTNode *qualifier = ast_member_object(member);
    const char *qualifier_name = qualifier != NULL && qualifier->type == AST_IDENTIFIER
        ? ast_identifier_name(qualifier) : NULL;
    const char *member_name = ast_member_name(member);
    const char *enum_name = ast_enum_name(enum_decl);
    uint32_t decl_id = ast_node_stable_id(enum_decl);
    if (qualifier_name == NULL || member_name == NULL || enum_name == NULL ||
        ast_node_stable_id(qualifier) == 0 || ast_node_stable_id(member) == 0 ||
        decl_id == 0 || enum_type->name == NULL ||
        strcmp(qualifier_name, enum_name) != 0 ||
        strcmp(enum_type->name, enum_name) != 0 ||
        semantic_host_decl_for_type(ctx, enum_type) != enum_decl)
        return false;
    char *flat_name = flatten_static_member_access(member, '_');
    Symbol *owner = scope_lookup(ctx->scope, qualifier_name);
    Symbol *selected = flat_name != NULL ? scope_lookup(ctx->scope, flat_name) : NULL;
    if (selected == NULL)
        selected = scope_lookup(ctx->scope, member_name);
    free(flat_name);
    if (owner == NULL || owner->kind != SYMBOL_CLASS || owner->type != enum_type ||
        owner->decl_syntax_id != decl_id || selected == NULL ||
        selected->kind != SYMBOL_VARIABLE || selected->type != enum_type ||
        selected->decl_syntax_id != decl_id || selected->name == NULL ||
        strcmp(selected->name, member_name) != 0)
        return false;
    size_t variant_count = 0;
    char **variants = ast_enum_variants(enum_decl, &variant_count);
    if (variant_ordinal >= variant_count || variants == NULL ||
        variants[variant_ordinal] == NULL ||
        strcmp(variants[variant_ordinal], member_name) != 0 ||
        ast_enum_variant_param_count(enum_decl, variant_ordinal) != 0)
        return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, member, false);
    if (row == NULL || !row->value_present || !row->meta ||
        row->value_shape != AST_MEMBER_ACCESS || row->value_type != enum_type)
        return false;
    SemanticPublishedUnitEnumSelection *receipt = calloc(1, sizeof(*receipt));
    if (receipt == NULL) return false;
    receipt->qualifier_name = pergyra_strdup(qualifier_name);
    receipt->enum_name = pergyra_strdup(enum_name);
    receipt->member_name = pergyra_strdup(member_name);
    if (receipt->qualifier_name == NULL || receipt->enum_name == NULL ||
        receipt->member_name == NULL) {
        semantic_published_unit_enum_selection_destroy(receipt);
        return false;
    }
    receipt->qualifier = qualifier;
    receipt->qualifier_id = ast_node_stable_id(qualifier);
    receipt->qualifier_binding_id = ast_identifier_binding_syntax_id(qualifier);
    receipt->member_id = ast_node_stable_id(member);
    receipt->enum_decl = enum_decl;
    receipt->enum_decl_id = decl_id;
    receipt->enum_type = enum_type;
    receipt->variant_ordinal = variant_ordinal;
    receipt->variant_count = variant_count;
    receipt->payload_count = 0;
    semantic_published_unit_enum_selection_destroy(row->unit_enum_selection);
    row->unit_enum_selection = receipt;
    return true;
}

bool semantic_param_publication_checked_unit_enum_selection(
    const SemanticContext *ctx, const ASTNode *member)
{
    if (!semantic_param_publication_inputs_ready(ctx) || member == NULL ||
        member->type != AST_MEMBER_ACCESS) return false;
    PublishedNode *row = publication_current_value(ctx, member);
    const SemanticPublishedUnitEnumSelection *receipt =
        row != NULL ? row->unit_enum_selection : NULL;
    const ASTNode *qualifier = ast_member_object(member);
    const char *qualifier_name = qualifier != NULL && qualifier->type == AST_IDENTIFIER
        ? ast_identifier_name(qualifier) : NULL;
    const char *member_name = ast_member_name(member);
    if (receipt == NULL || !row->meta || row->value_type != receipt->enum_type ||
        qualifier == NULL || qualifier != receipt->qualifier ||
        qualifier_name == NULL || member_name == NULL ||
        receipt->member_id == 0 || receipt->member_id != ast_node_stable_id(member) ||
        receipt->qualifier_id == 0 ||
        receipt->qualifier_id != ast_node_stable_id(qualifier) ||
        receipt->qualifier_binding_id != ast_identifier_binding_syntax_id(qualifier) ||
        receipt->qualifier_name == NULL ||
        strcmp(receipt->qualifier_name, qualifier_name) != 0 ||
        receipt->member_name == NULL || strcmp(receipt->member_name, member_name) != 0 ||
        receipt->enum_decl == NULL || receipt->enum_decl->type != AST_ENUM_DECL ||
        receipt->enum_decl_id == 0 ||
        receipt->enum_decl_id != ast_node_stable_id(receipt->enum_decl) ||
        receipt->enum_name == NULL || ast_enum_name(receipt->enum_decl) == NULL ||
        strcmp(receipt->enum_name, ast_enum_name(receipt->enum_decl)) != 0 ||
        strcmp(receipt->enum_name, qualifier_name) != 0 ||
        receipt->enum_type == NULL || receipt->enum_type == TYPE_UNKNOWN ||
        receipt->enum_type->kind != TYPE_KIND_ENUM || receipt->enum_type->name == NULL ||
        strcmp(receipt->enum_type->name, receipt->enum_name) != 0 ||
        semantic_host_decl_for_type((SemanticContext *)ctx, receipt->enum_type) != receipt->enum_decl ||
        receipt->payload_count != 0)
        return false;
    char *flat_name = flatten_static_member_access(member, '_');
    Symbol *owner = scope_lookup(ctx->scope, qualifier_name);
    Symbol *selected = flat_name != NULL ? scope_lookup(ctx->scope, flat_name) : NULL;
    if (selected == NULL)
        selected = scope_lookup(ctx->scope, member_name);
    free(flat_name);
    if (owner == NULL || owner->kind != SYMBOL_CLASS ||
        owner->decl_syntax_id != receipt->enum_decl_id || owner->type != receipt->enum_type ||
        selected == NULL || selected->kind != SYMBOL_VARIABLE ||
        selected->decl_syntax_id != receipt->enum_decl_id || selected->type != receipt->enum_type ||
        selected->name == NULL || strcmp(selected->name, member_name) != 0)
        return false;
    size_t variant_count = 0;
    char **variants = ast_enum_variants(receipt->enum_decl, &variant_count);
    return receipt->variant_count == variant_count &&
        receipt->variant_ordinal < variant_count && variants != NULL &&
        variants[receipt->variant_ordinal] != NULL &&
        strcmp(variants[receipt->variant_ordinal], receipt->member_name) == 0 &&
        ast_enum_variant_param_count(receipt->enum_decl, receipt->variant_ordinal) == 0;
}

Type *
expr_type_for_enum_variant_projection(SemanticContext *ctx, ASTNode *site,
                                      const Type *enum_type,
                                      const char *variant_name)
{
    ASTNode *decl = expr_find_enum_decl_by_name(ctx,
        enum_type != NULL ? enum_type->name : NULL);
    char payload_name[256];

    if (decl == NULL || variant_name == NULL)
        return NULL;

    size_t variant_count = 0;
    char **variants = ast_enum_variants(decl, &variant_count);
    for (size_t i = 0; i < variant_count; i++) {
        const char *candidate = variants != NULL ? variants[i] : NULL;
        size_t param_count = ast_enum_variant_param_count(decl, i);
        int written;

        if (candidate == NULL || strcmp(candidate, variant_name) != 0)
            continue;
        if (param_count == 0) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_UNDEFINED_SYMBOL,
                PGY_CAUSE_SYMBOL_UNDEFINED, PGY_FIX_MATCH_BUILTIN_SIGNATURE,
                site,
                "Enum variant '%s.%s' has no payload fields to access",
                enum_type->name != NULL ? enum_type->name : "<enum>",
                variant_name);
            return TYPE_UNKNOWN;
        }
        written = snprintf(payload_name, sizeof(payload_name), "%s$%s",
            enum_type->name != NULL ? enum_type->name : "<enum>",
            variant_name);
        if (written < 0 || (size_t)written >= sizeof(payload_name)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_UNKNOWN_TYPE,
                PGY_CAUSE_TYPE_UNKNOWN, PGY_FIX_REFACTOR_OR_RAISE_LIMIT, site,
                "Enum payload type name is too long for '%s.%s'",
                enum_type->name != NULL ? enum_type->name : "<enum>",
                variant_name);
            return TYPE_UNKNOWN;
        }
        return create_overlay_nominal_type(payload_name);
    }

    return NULL;
}

Type *
expr_type_for_enum_payload_field(SemanticContext *ctx, ASTNode *site,
                                 const Type *payload_type,
                                 const char *field_name)
{
    const char *payload_name = payload_type != NULL ? payload_type->name : NULL;
    const char *sep = payload_name != NULL ? strchr(payload_name, '$') : NULL;
    size_t field_index = 0;

    if (sep == NULL || !expr_parse_enum_payload_field_index(field_name,
            &field_index)) {
        return NULL;
    }

    size_t enum_len = (size_t)(sep - payload_name);
    const char *variant_name = sep + 1;
    char *enum_name = calloc(enum_len + 1, 1);
    if (enum_name == NULL)
        return NULL;
    memcpy(enum_name, payload_name, enum_len);

    ASTNode *decl = semantic_find_enum_decl_by_name(ctx, enum_name);
    free(enum_name);
    if (decl == NULL)
        return NULL;

    size_t variant_count = 0;
    char **variants = ast_enum_variants(decl, &variant_count);
    for (size_t v = 0; v < variant_count; v++) {
        const char *candidate = variants != NULL ? variants[v] : NULL;
        size_t param_count = ast_enum_variant_param_count(decl, v);

        if (candidate == NULL || strcmp(candidate, variant_name) != 0)
            continue;
        if (field_index >= param_count
            || ast_enum_variant_param(decl, v, field_index) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_UNDEFINED_SYMBOL,
                PGY_CAUSE_SYMBOL_UNDEFINED,
                PGY_FIX_MATCH_BUILTIN_SIGNATURE, site,
                "Unknown enum payload field '%s.%s'",
                payload_name, field_name);
            return TYPE_UNKNOWN;
        }
        return expr_enum_resolve_type_ref(
            ast_enum_variant_param(decl, v, field_index), ctx);
    }

    return NULL;
}
