#include "type_checker_internal.h"

#include <string.h>

static const char *
projection_path_scratch_strdup(SemanticContext *ctx, const char *text)
{
    if (ctx == NULL || text == NULL)
        return NULL;
    return pgy_arena_strdup(&ctx->scratch_arena, text);
}

size_t
projection_source_field_count(ASTNode *decl)
{
    if (decl == NULL || decl->type != AST_CLASS_DECL)
        return 0;
    /* F2 (docs/144) Phase 3: count via the pre-semantic field-shape model. */
    PgyDeclField *fields = NULL;
    size_t field_count = pgy_class_decl_field_model_build(decl, &fields);
    pgy_decl_field_model_free(fields, field_count);
    return field_count;
}

PgyDeclField
projection_source_field_at(ASTNode *decl, size_t index)
{
    PgyDeclField empty = {0};
    if (decl == NULL || decl->type != AST_CLASS_DECL)
        return empty;
    PgyDeclField *fields = NULL;
    size_t field_count = pgy_class_decl_field_model_build(decl, &fields);
    PgyDeclField result = (index < field_count) ? fields[index] : empty;
    pgy_decl_field_model_free(fields, field_count);
    return result;
}

Type *
projection_resolve_type_ref(ASTNode *type_ref, SemanticContext *ctx)
{
    Type *resolved =
        semantic_type_resolution_lookup_metadata_type_ref(ctx, type_ref);
    return resolved != NULL ? resolved : TYPE_UNKNOWN;
}

static int
resolve_projection_source_field_path_rec(ASTNode *source_decl,
                                         const char *field_name,
                                         unsigned depth,
                                         SemanticContext *ctx,
                                         const char **path_out,
                                         Type **field_type_out)
{
    size_t field_count;
    int match_count = 0;
    const char *resolved_path = NULL;
    Type *resolved_type = NULL;

    if (path_out != NULL)
        *path_out = NULL;
    if (field_type_out != NULL)
        *field_type_out = NULL;

    if (source_decl == NULL || field_name == NULL || ctx == NULL || depth > 8)
        return 0;

    field_count = projection_source_field_count(source_decl);
    for (size_t i = 0; i < field_count; i++) {
        PgyDeclField field = projection_source_field_at(source_decl, i);
        if (field.name != NULL
            && strcmp(field.name, field_name) == 0) {
            if (path_out != NULL)
                *path_out = projection_path_scratch_strdup(ctx, field_name);
            if (field_type_out != NULL)
                *field_type_out = field.type_ast != NULL
                    ? projection_resolve_type_ref(field.type_ast, ctx)
                    : TYPE_UNKNOWN;
            return 1;
        }
    }

    for (size_t i = 0; i < field_count; i++) {
        PgyDeclField field = projection_source_field_at(source_decl, i);
        ASTNode *vessel_decl;
        const char *nested_path = NULL;
        Type *nested_type = NULL;
        const char *prefixed_path;
        int nested_status;
        Type *field_type;

        if (!field.is_vessel_field
            || field.name == NULL || field.type_ast == NULL) {
            continue;
        }

        field_type = projection_resolve_type_ref(field.type_ast, ctx);
        vessel_decl = semantic_host_decl_for_type(ctx, field_type);
        if (vessel_decl == NULL || vessel_decl->type != AST_CLASS_DECL
            || ast_class_nominal_kind(vessel_decl) != NOMINAL_DECL_VESSEL) {
            continue;
        }

        nested_status = resolve_projection_source_field_path_rec(
            vessel_decl, field_name, depth + 1, ctx,
            &nested_path, &nested_type);
        if (nested_status != 1) {
            if (nested_status == 2)
                match_count = 2;
            continue;
        }

        prefixed_path = pgy_arena_fmt(&ctx->scratch_arena, "%s.%s",
            field.name, nested_path);
        if (prefixed_path == NULL)
            continue;

        match_count++;
        if (match_count == 1) {
            resolved_path = prefixed_path;
            resolved_type = nested_type;
        } else {
            resolved_path = NULL;
            resolved_type = NULL;
        }
    }

    if (match_count == 1) {
        if (path_out != NULL)
            *path_out = resolved_path;
        if (field_type_out != NULL)
            *field_type_out = resolved_type;
        return 1;
    }

    return match_count > 1 ? 2 : 0;
}

int
semantic_resolve_projection_source_field_path(SemanticContext *ctx,
                                              ASTNode *source_decl,
                                              const char *field_name,
                                              const char **path_out,
                                              Type **field_type_out)
{
    return resolve_projection_source_field_path_rec(source_decl, field_name, 0,
        ctx, path_out, field_type_out);
}
