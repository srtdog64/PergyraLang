#include "type_checker_internal.h"
#include "../common/string_compat.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *
projection_path_strdup_fmt(const char *fmt, ...)
{
    va_list ap;
    va_list ap2;
    int needed;
    char *buf;

    va_start(ap, fmt);
    va_copy(ap2, ap);
    needed = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (needed < 0) {
        va_end(ap2);
        return NULL;
    }

    buf = malloc((size_t)needed + 1);
    if (buf == NULL) {
        va_end(ap2);
        return NULL;
    }

    vsnprintf(buf, (size_t)needed + 1, fmt, ap2);
    va_end(ap2);
    return buf;
}

size_t
projection_source_field_count(ASTNode *decl)
{
    if (decl == NULL)
        return 0;
    if (decl->type == AST_CLASS_DECL)
        return decl->data.class_decl.field_count;
    return 0;
}

ClassField *
projection_source_field_at(ASTNode *decl, size_t index)
{
    if (decl == NULL)
        return NULL;
    if (decl->type == AST_CLASS_DECL) {
        if (index < decl->data.class_decl.field_count)
            return decl->data.class_decl.fields[index];
        return NULL;
    }
    return NULL;
}

static Type *
projection_path_resolve_type_ref(ASTNode *type_ref, SemanticContext *ctx)
{
    return semantic_type_resolution_lookup_or_materialize(ctx, type_ref);
}

static int
resolve_projection_source_field_path_rec(ASTNode *program_root,
                                         ASTNode *source_decl,
                                         const char *field_name,
                                         unsigned depth,
                                         SemanticContext *ctx,
                                         char **path_out,
                                         Type **field_type_out)
{
    size_t field_count;
    int match_count = 0;
    char *resolved_path = NULL;
    Type *resolved_type = NULL;

    if (path_out != NULL)
        *path_out = NULL;
    if (field_type_out != NULL)
        *field_type_out = NULL;

    if (program_root == NULL || source_decl == NULL || field_name == NULL || depth > 8)
        return 0;

    field_count = projection_source_field_count(source_decl);
    for (size_t i = 0; i < field_count; i++) {
        ClassField *field = projection_source_field_at(source_decl, i);
        if (field != NULL && field->name != NULL
            && strcmp(field->name, field_name) == 0) {
            if (path_out != NULL)
                *path_out = pergyra_strdup(field_name);
            if (field_type_out != NULL)
                *field_type_out = field->type != NULL
                    ? projection_path_resolve_type_ref(field->type, ctx)
                    : TYPE_UNKNOWN;
            return 1;
        }
    }

    for (size_t i = 0; i < field_count; i++) {
        ClassField *field = projection_source_field_at(source_decl, i);
        ASTNode *vessel_decl;
        char *nested_path = NULL;
        Type *nested_type = NULL;
        char *prefixed_path;
        int nested_status;

        if (field == NULL || !field->is_vessel_field
            || field->name == NULL || field->type == NULL
            || field->type->type != AST_TYPE
            || field->type->data.type.name == NULL) {
            continue;
        }

        vessel_decl = find_type_decl_by_name(program_root, field->type->data.type.name);
        if (vessel_decl == NULL || vessel_decl->type != AST_CLASS_DECL
            || vessel_decl->data.class_decl.nominal_kind != NOMINAL_DECL_VESSEL) {
            continue;
        }

        nested_status = resolve_projection_source_field_path_rec(
            program_root, vessel_decl, field_name, depth + 1, ctx,
            &nested_path, &nested_type);
        if (nested_status != 1) {
            if (nested_path != NULL)
                free(nested_path);
            if (nested_status == 2)
                match_count = 2;
            continue;
        }

        prefixed_path = projection_path_strdup_fmt("%s.%s", field->name, nested_path);
        free(nested_path);
        if (prefixed_path == NULL)
            continue;

        match_count++;
        if (match_count == 1) {
            resolved_path = prefixed_path;
            resolved_type = nested_type;
        } else {
            free(prefixed_path);
            free(resolved_path);
            resolved_path = NULL;
            resolved_type = NULL;
        }
    }

    if (match_count == 1) {
        if (path_out != NULL)
            *path_out = resolved_path;
        else
            free(resolved_path);
        if (field_type_out != NULL)
            *field_type_out = resolved_type;
        return 1;
    }

    if (resolved_path != NULL)
        free(resolved_path);
    return match_count > 1 ? 2 : 0;
}

int
resolve_projection_source_field_path(ASTNode *program_root,
                                     ASTNode *source_decl,
                                     const char *field_name,
                                     SemanticContext *ctx,
                                     char **path_out,
                                     Type **field_type_out)
{
    return resolve_projection_source_field_path_rec(program_root, source_decl,
        field_name, 0, ctx, path_out, field_type_out);
}
