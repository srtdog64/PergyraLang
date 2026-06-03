/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * C backend projection emit owner: class-field view helpers plus the
 * projection source-path resolver and literal emitter.  Split from
 * transpiler_projection.c (2026-06 owner-size closure) so the projection
 * type/predicate owner stays under the production owner LOC cap.
 */

#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "parser/ast_api.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_inventory_view.h"
#include "transpiler_projection.h"

static bool
projection_class_field_view(TranspilerCtx *ctx,
                            ASTNode *decl,
                            TranspilerHostedFieldView *view)
{
    const char *decl_name;

    if (view == NULL)
        return false;
    view->decl_header = NULL;
    view->ast_compat_fields = NULL;
    view->ast_compat_count = 0;
    view->count = 0;
    view->uses_mir_metadata = false;
    view->requires_mir_metadata = false;

    if (ctx == NULL || decl == NULL || decl->type != AST_CLASS_DECL)
        return false;

    decl_name = transpiler_decl_name_local(decl);
    *view = transpiler_hosted_class_field_view_from_decl(ctx, decl_name, decl);
    if (transpiler_hosted_field_view_missing_mir_metadata(view)) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing projection class-field metadata for '%s'",
            decl_name != NULL ? decl_name : "(anonymous-class)");
        return false;
    }
    return true;
}

static size_t
projection_class_field_count(TranspilerCtx *ctx, ASTNode *decl)
{
    TranspilerHostedFieldView view;

    return projection_class_field_view(ctx, decl, &view) ? view.count : 0;
}

static const char *
projection_class_field_name(TranspilerCtx *ctx, ASTNode *decl, size_t index)
{
    TranspilerHostedFieldView view;

    if (!projection_class_field_view(ctx, decl, &view))
        return NULL;
    return transpiler_hosted_field_view_name(&view, index);
}

static const char *
projection_class_field_type_name(TranspilerCtx *ctx, ASTNode *decl,
                                 size_t index)
{
    TranspilerHostedFieldView view;
    const MIRDeclField *field;
    ASTNode *type_node;

    if (!projection_class_field_view(ctx, decl, &view))
        return NULL;
    field = transpiler_hosted_field_view_metadata(&view, index);
    if (field != NULL) {
        const char *type_name = transpiler_mir_decl_field_type_name(field);
        if (type_name != NULL)
            return type_name;
        type_node = transpiler_mir_decl_field_type(field);
    } else {
        type_node = transpiler_hosted_field_view_type(&view, index);
    }

    if (type_node != NULL && type_node->type == AST_TYPE)
        return ast_type_name(type_node);
    return NULL;
}

int
resolve_projection_source_path_rec(TranspilerCtx *ctx, ASTNode *source_decl,
                                   const char *field_name, unsigned depth,
                                   char **path_out)
{
    size_t field_count;
    int match_count = 0;
    char *resolved_path = NULL;

    if (path_out != NULL)
        *path_out = NULL;
    if (ctx == NULL || source_decl == NULL || field_name == NULL || depth > 8)
        return 0;

    field_count = projection_class_field_count(ctx, source_decl);
    for (size_t i = 0; i < field_count; i++) {
        const char *candidate_name =
            projection_class_field_name(ctx, source_decl, i);
        if (candidate_name != NULL
            && strcmp(candidate_name, field_name) == 0) {
            if (path_out != NULL)
                *path_out = transpiler_scratch_strdup(ctx, field_name);
            return 1;
        }
    }

    for (size_t i = 0; i < field_count; i++) {
        ASTNode *vessel_decl;
        const char *candidate_name;
        const char *type_name;
        char *nested_path = NULL;
        char *prefixed_path;
        int nested_status;

        candidate_name = projection_class_field_name(ctx, source_decl, i);
        type_name = projection_class_field_type_name(ctx, source_decl, i);
        if (candidate_name == NULL || type_name == NULL) {
            continue;
        }

        vessel_decl = transpiler_find_projection_nominal_decl_local(
            ctx, type_name);
        if (vessel_decl == NULL
            || vessel_decl->type != AST_CLASS_DECL
            || ast_class_nominal_kind(vessel_decl) != NOMINAL_DECL_VESSEL) {
            continue;
        }

        nested_status = resolve_projection_source_path_rec(
            ctx, vessel_decl, field_name, depth + 1, &nested_path);
        if (nested_status != 1) {
            if (nested_status == 2)
                match_count = 2;
            continue;
        }

        prefixed_path = transpiler_scratch_fmt(ctx,
                                               "%s.%s",
                                               candidate_name,
                                               nested_path);
        if (prefixed_path == NULL)
            continue;

        match_count++;
        if (match_count == 1) {
            resolved_path = prefixed_path;
        } else {
            resolved_path = NULL;
        }
    }

    if (match_count == 1) {
        if (path_out != NULL)
            *path_out = resolved_path;
        return 1;
    }

    return match_count > 1 ? 2 : 0;
}

char *
emit_projection_literal(TranspilerCtx *ctx, ASTNode *target_decl, ASTNode *source_decl,
                        ASTNode *refresh, const char *target_type_name,
                        const char *source_expr)
{
    CodeBuf *buf;
    char *result;
    bool first = true;

    if (target_decl == NULL || source_decl == NULL
        || target_type_name == NULL || source_expr == NULL) {
        return pergyra_strdup("0");
    }

    buf = codebuf_create();
    codebuf_write(buf, "(%s){ ", target_type_name);

    size_t target_field_count = projection_class_field_count(ctx, target_decl);
    for (size_t i = 0; i < target_field_count; i++) {
        const char *target_field_name =
            projection_class_field_name(ctx, target_decl, i);
        const char *source_field_name = NULL;
        char *source_path = NULL;
        int source_status;

        if (target_field_name == NULL)
            continue;

        source_field_name = target_field_name;
        if (refresh != NULL && refresh->type == AST_ZONE_REFRESH) {
            for (size_t j = 0; j < ast_zone_refresh_field_map_count(refresh); j++) {
                const char *mapped_target =
                    ast_zone_refresh_mapped_target_field(refresh, j);
                const char *mapped_source =
                    ast_zone_refresh_mapped_source_field(refresh, j);
                if (mapped_target != NULL && mapped_source != NULL
                    && strcmp(mapped_target, target_field_name) == 0) {
                    source_field_name = mapped_source;
                    break;
                }
            }
        }

        source_status = resolve_projection_source_path_rec(
            ctx, source_decl, source_field_name, 0, &source_path);
        if (!first)
            codebuf_write(buf, ", ");
        first = false;

        if (source_status == 1 && source_path != NULL) {
            codebuf_write(buf, ".%s = %s.%s",
                target_field_name, source_expr, source_path);
        } else {
            codebuf_write(buf, ".%s = 0", target_field_name);
        }
    }

    codebuf_write(buf, " }");
    result = pergyra_strdup(buf->data);
    codebuf_destroy(buf);
    return result;
}
