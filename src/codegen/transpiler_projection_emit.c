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
#include "../compiler/mir_decl_headers.h"
#include "parser/ast_api.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_inventory_view.h"
#include "transpiler_projection.h"

static bool
projection_class_field_view_by_name(TranspilerCtx *ctx,
                                    const char *decl_name,
                                    TranspilerHostedFieldView *view)
{
    if (view == NULL)
        return false;
    memset(view, 0, sizeof(*view));

    if (ctx == NULL || decl_name == NULL)
        return false;

    *view = transpiler_hosted_class_field_view_from_decl(
        ctx, decl_name, NULL);
    if (transpiler_active_has_mir(ctx) && !view->uses_mir_metadata) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing projection class-field metadata for '%s'",
            decl_name);
        return false;
    }
    if (transpiler_hosted_field_view_missing_mir_metadata(view)) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing projection class-field metadata for '%s'",
            decl_name);
        return false;
    }
    return true;
}

static bool
projection_type_is_vessel(TranspilerCtx *ctx, const char *type_name)
{
    const MIRDeclHeader *header;
    ASTNode *decl;

    if (ctx == NULL || type_name == NULL)
        return false;

    header = transpiler_active_decl_header_of_type(
        ctx, AST_CLASS_DECL, type_name);
    if (header != NULL) {
        return mir_decl_header_nominal_kind_or(
            header, NOMINAL_DECL_CLASS) == NOMINAL_DECL_VESSEL;
    }
    if (transpiler_active_has_mir(ctx))
        return false;

    decl = transpiler_find_projection_nominal_decl_local(ctx, type_name);
    return decl != NULL
        && decl->type == AST_CLASS_DECL
        && ast_class_nominal_kind(decl) == NOMINAL_DECL_VESSEL;
}

static size_t
projection_class_field_count_by_name(TranspilerCtx *ctx,
                                     const char *decl_name)
{
    TranspilerHostedFieldView view;

    return projection_class_field_view_by_name(ctx, decl_name, &view)
        ? view.count : 0;
}

static const char *
projection_class_field_name_by_name(TranspilerCtx *ctx,
                                    const char *decl_name,
                                    size_t index)
{
    TranspilerHostedFieldView view;

    if (!projection_class_field_view_by_name(ctx, decl_name, &view))
        return NULL;
    return transpiler_hosted_field_view_name(&view, index);
}

static const char *
projection_class_field_type_name_by_name(TranspilerCtx *ctx,
                                         const char *decl_name,
                                         size_t index)
{
    TranspilerHostedFieldView view;
    const MIRDeclField *field;
    ASTNode *type_node;

    if (!projection_class_field_view_by_name(ctx, decl_name, &view))
        return NULL;
    field = transpiler_hosted_field_view_metadata(&view, index);
    if (field != NULL) {
        const char *type_name = transpiler_mir_decl_field_type_name(field);
        if (type_name != NULL && type_name[0] != '\0')
            return type_name;
        if (transpiler_active_has_mir(ctx)) {
            transpiler_set_mir_inventory_missing(ctx,
                "MIR-only C path missing projection class-field type-name metadata for '%s' index %zu",
                decl_name, index);
            return NULL;
        }
        type_node = transpiler_mir_decl_field_type(field);
    } else {
        if (transpiler_active_has_mir(ctx)) {
            transpiler_set_mir_inventory_missing(ctx,
                "MIR-only C path missing projection class-field metadata for '%s' index %zu",
                decl_name, index);
            return NULL;
        }
        type_node = transpiler_hosted_field_view_type(&view, index);
    }

    if (type_node != NULL && type_node->type == AST_TYPE)
        return ast_type_name(type_node);
    return NULL;
}

bool
transpiler_projection_type_is_struct_like(TranspilerCtx *ctx,
                                          const char *type_name)
{
    const MIRDeclHeader *header;
    ASTNode *decl;

    if (ctx == NULL || type_name == NULL)
        return false;

    header = transpiler_active_decl_header_of_type(
        ctx, AST_CLASS_DECL, type_name);
    if (header != NULL) {
        switch (mir_decl_header_nominal_kind_or(
            header, NOMINAL_DECL_CLASS)) {
        case NOMINAL_DECL_STRUCT:
        case NOMINAL_DECL_VESSEL:
        case NOMINAL_DECL_OBJECT:
        case NOMINAL_DECL_TOBJECT:
            return true;
        default:
            return false;
        }
    }
    if (transpiler_active_has_mir(ctx))
        return false;

    decl = transpiler_find_projection_nominal_decl_local(ctx, type_name);
    return decl != NULL && ast_class_is_struct(decl);
}

int
resolve_projection_source_path_by_name(TranspilerCtx *ctx,
                                       const char *source_type_name,
                                       const char *field_name,
                                       unsigned depth,
                                       char **path_out)
{
    size_t field_count;
    int match_count = 0;
    char *resolved_path = NULL;

    if (path_out != NULL)
        *path_out = NULL;
    if (ctx == NULL || source_type_name == NULL || field_name == NULL
        || depth > 8)
        return 0;

    field_count = projection_class_field_count_by_name(ctx, source_type_name);
    for (size_t i = 0; i < field_count; i++) {
        const char *candidate_name =
            projection_class_field_name_by_name(ctx, source_type_name, i);
        if (candidate_name != NULL
            && strcmp(candidate_name, field_name) == 0) {
            if (path_out != NULL)
                *path_out = transpiler_scratch_strdup(ctx, field_name);
            return 1;
        }
    }

    for (size_t i = 0; i < field_count; i++) {
        const char *candidate_name;
        const char *type_name;
        char *nested_path = NULL;
        char *prefixed_path;
        int nested_status;

        candidate_name =
            projection_class_field_name_by_name(ctx, source_type_name, i);
        type_name =
            projection_class_field_type_name_by_name(ctx, source_type_name, i);
        if (candidate_name == NULL || type_name == NULL) {
            continue;
        }

        if (!projection_type_is_vessel(ctx, type_name)) {
            continue;
        }

        nested_status = resolve_projection_source_path_by_name(
            ctx, type_name, field_name, depth + 1, &nested_path);
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

static const char *
projection_mir_refresh_mapped_source(const MIRDeclZoneRefresh *refresh,
                                     const char *target_field_name)
{
    if (refresh == NULL || target_field_name == NULL)
        return NULL;

    for (size_t j = 0; j < mir_decl_zone_refresh_field_map_count(refresh); j++) {
        const char *mapped_target =
            mir_decl_zone_refresh_mapped_target_field(refresh, j);
        const char *mapped_source =
            mir_decl_zone_refresh_mapped_source_field(refresh, j);
        if (mapped_target != NULL && mapped_source != NULL
            && strcmp(mapped_target, target_field_name) == 0) {
            return mapped_source;
        }
    }
    return NULL;
}

static char *
emit_projection_literal_by_name_internal(TranspilerCtx *ctx,
                                         const char *target_type_name,
                                         const char *source_type_name,
                                         const MIRDeclZoneRefresh *mir_refresh,
                                         const char *source_expr)
{
    CodeBuf *buf;
    char *result;
    bool first = true;

    if (target_type_name == NULL || source_type_name == NULL
        || source_expr == NULL) {
        return NULL;
    }

    buf = codebuf_create();
    codebuf_write(buf, "(%s){ ", target_type_name);

    size_t target_field_count =
        projection_class_field_count_by_name(ctx, target_type_name);
    for (size_t i = 0; i < target_field_count; i++) {
        const char *target_field_name =
            projection_class_field_name_by_name(ctx, target_type_name, i);
        const char *source_field_name = NULL;
        char *source_path = NULL;
        int source_status;

        if (target_field_name == NULL)
            continue;

        source_field_name =
            projection_mir_refresh_mapped_source(mir_refresh,
                target_field_name);
        if (source_field_name == NULL)
            source_field_name = target_field_name;

        source_status = resolve_projection_source_path_by_name(
            ctx, source_type_name, source_field_name, 0, &source_path);
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

char *
emit_projection_literal_by_name(TranspilerCtx *ctx,
                                const char *target_type_name,
                                const char *source_type_name,
                                const char *source_expr)
{
    return emit_projection_literal_by_name_internal(ctx,
        target_type_name,
        source_type_name,
        NULL,
        source_expr);
}

char *
emit_projection_literal_by_zone_refresh_metadata(
    TranspilerCtx *ctx,
    const char *target_type_name,
    const char *source_type_name,
    const MIRDeclZoneRefresh *refresh,
    const char *source_expr)
{
    return emit_projection_literal_by_name_internal(ctx,
        target_type_name,
        source_type_name,
        refresh,
        source_expr);
}
