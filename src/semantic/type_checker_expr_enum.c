#include <stdio.h>
#include <string.h>

#include "type_checker_internal.h"
#include "diag_codes.h"

static Type *
expr_enum_resolve_type_ref(ASTNode *type_ref, SemanticContext *ctx)
{
    Type *resolved =
        semantic_type_resolution_lookup_type_ref_or_materialize(ctx, type_ref);
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
    if (ctx == NULL || ctx->program_root == NULL
        || ctx->program_root->type != AST_PROGRAM || enum_name == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < ctx->program_root->data.program.count; i++) {
        ASTNode *stmt = ctx->program_root->data.program.statements[i];
        if (stmt != NULL && stmt->type == AST_ENUM_DECL
            && ast_enum_name(stmt) != NULL
            && strcmp(ast_enum_name(stmt), enum_name) == 0) {
            return stmt;
        }
    }
    return NULL;
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
        size_t param_count = decl->data.enum_decl.variant_param_counts != NULL
            ? decl->data.enum_decl.variant_param_counts[i] : 0;
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

    if (ctx == NULL || ctx->program_root == NULL
        || ctx->program_root->type != AST_PROGRAM) {
        return NULL;
    }
    if (sep == NULL || !expr_parse_enum_payload_field_index(field_name,
            &field_index)) {
        return NULL;
    }

    for (size_t i = 0; i < ctx->program_root->data.program.count; i++) {
        ASTNode *decl = ctx->program_root->data.program.statements[i];
        size_t enum_len = (size_t)(sep - payload_name);
        const char *variant_name = sep + 1;

        if (decl == NULL || decl->type != AST_ENUM_DECL
            || ast_enum_name(decl) == NULL
            || strlen(ast_enum_name(decl)) != enum_len
            || strncmp(ast_enum_name(decl), payload_name, enum_len) != 0) {
            continue;
        }

        size_t variant_count = 0;
        char **variants = ast_enum_variants(decl, &variant_count);
        for (size_t v = 0; v < variant_count; v++) {
            const char *candidate = variants != NULL ? variants[v] : NULL;
            size_t param_count = decl->data.enum_decl.variant_param_counts != NULL
                ? decl->data.enum_decl.variant_param_counts[v] : 0;

            if (candidate == NULL || strcmp(candidate, variant_name) != 0)
                continue;
            if (field_index >= param_count
                || decl->data.enum_decl.variant_params == NULL
                || decl->data.enum_decl.variant_params[v] == NULL) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_UNDEFINED_SYMBOL,
                    PGY_CAUSE_SYMBOL_UNDEFINED,
                    PGY_FIX_MATCH_BUILTIN_SIGNATURE, site,
                    "Unknown enum payload field '%s.%s'",
                    payload_name, field_name);
                return TYPE_UNKNOWN;
            }
            return expr_enum_resolve_type_ref(
                decl->data.enum_decl.variant_params[v][field_index], ctx);
        }
    }

    return NULL;
}
