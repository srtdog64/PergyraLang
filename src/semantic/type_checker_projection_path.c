#include "type_checker_internal.h"
#include "../common/string_compat.h"

#include <stdint.h>
#include <stdlib.h>
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

static bool
projection_path_segment_init(PgyDomainProjectionPathSegmentFact *segment,
                             const PgyDeclField *field,
                             Type *field_type)
{
    const char *field_type_name;

    if (segment == NULL || field == NULL || field->name == NULL)
        return false;
    field_type_name = type_name_or_unknown(field_type);
    if (field_type_name == NULL)
        return false;
    memset(segment, 0, sizeof(*segment));
    segment->field_syntax_id = field->declaration_syntax_id;
    segment->field_name = pergyra_strdup(field->name);
    segment->field_type_name = pergyra_strdup(field_type_name);
    if (segment->field_name == NULL || segment->field_type_name == NULL) {
        free(segment->field_name);
        free(segment->field_type_name);
        memset(segment, 0, sizeof(*segment));
        return false;
    }
    return true;
}

/* Return values: 0 missing, 1 exact, 2 ambiguous, -1 allocation failure. */
static int
resolve_projection_source_field_path_rec(
    ASTNode *source_decl,
    const char *field_name,
    unsigned depth,
    SemanticContext *ctx,
    const char **path_out,
    Type **field_type_out,
    PgyDomainProjectionPathSegmentFact **segments_out,
    size_t *segment_count_out)
{
    PgyDeclField *fields = NULL;
    size_t field_count;
    size_t match_count = 0;
    bool ambiguous = false;
    const char *resolved_path = NULL;
    Type *resolved_type = NULL;
    PgyDomainProjectionPathSegmentFact *resolved_segments = NULL;
    size_t resolved_segment_count = 0;

    if (path_out != NULL)
        *path_out = NULL;
    if (field_type_out != NULL)
        *field_type_out = NULL;
    if (segments_out != NULL)
        *segments_out = NULL;
    if (segment_count_out != NULL)
        *segment_count_out = 0;

    if (source_decl == NULL || field_name == NULL || ctx == NULL || depth > 8)
        return 0;

    field_count = pgy_class_decl_field_model_build(source_decl, &fields);
    for (size_t i = 0; i < field_count; i++) {
        PgyDeclField *field = &fields[i];
        Type *field_type;
        const char *direct_path;
        PgyDomainProjectionPathSegmentFact *direct_segments = NULL;

        if (field->name == NULL || strcmp(field->name, field_name) != 0)
            continue;
        field_type = field->type_ast != NULL
            ? projection_resolve_type_ref(field->type_ast, ctx)
            : TYPE_UNKNOWN;
        direct_path = projection_path_scratch_strdup(ctx, field_name);
        if (direct_path == NULL) {
            pgy_decl_field_model_free(fields, field_count);
            return -1;
        }
        if (segments_out != NULL) {
            direct_segments = calloc(1, sizeof(*direct_segments));
            if (direct_segments == NULL
                || !projection_path_segment_init(
                    direct_segments, field, field_type)) {
                pgy_domain_projection_path_segments_destroy(
                    direct_segments, direct_segments != NULL ? 1 : 0);
                pgy_decl_field_model_free(fields, field_count);
                return -1;
            }
        }
        pgy_decl_field_model_free(fields, field_count);
        if (path_out != NULL)
            *path_out = direct_path;
        if (field_type_out != NULL)
            *field_type_out = field_type;
        if (segments_out != NULL)
            *segments_out = direct_segments;
        if (segment_count_out != NULL)
            *segment_count_out = segments_out != NULL ? 1 : 0;
        return 1;
    }

    for (size_t i = 0; i < field_count; i++) {
        PgyDeclField *field = &fields[i];
        ASTNode *vessel_decl;
        const char *nested_path = NULL;
        Type *nested_type = NULL;
        const char *prefixed_path;
        int nested_status;
        Type *field_type;
        PgyDomainProjectionPathSegmentFact *nested_segments = NULL;
        size_t nested_segment_count = 0;
        PgyDomainProjectionPathSegmentFact *candidate_segments = NULL;
        size_t candidate_segment_count = 0;

        if (!field->is_vessel_field
            || field->name == NULL || field->type_ast == NULL) {
            continue;
        }

        field_type = projection_resolve_type_ref(field->type_ast, ctx);
        vessel_decl = semantic_host_decl_for_type(ctx, field_type);
        if (vessel_decl == NULL || vessel_decl->type != AST_CLASS_DECL
            || ast_class_nominal_kind(vessel_decl) != NOMINAL_DECL_VESSEL) {
            continue;
        }

        nested_status = resolve_projection_source_field_path_rec(
            vessel_decl, field_name, depth + 1, ctx,
            &nested_path, &nested_type,
            segments_out != NULL ? &nested_segments : NULL,
            segments_out != NULL ? &nested_segment_count : NULL);
        if (nested_status < 0) {
            pgy_domain_projection_path_segments_destroy(
                nested_segments, nested_segment_count);
            pgy_domain_projection_path_segments_destroy(
                resolved_segments, resolved_segment_count);
            pgy_decl_field_model_free(fields, field_count);
            return -1;
        }
        if (nested_status != 1) {
            if (nested_status == 2)
                ambiguous = true;
            pgy_domain_projection_path_segments_destroy(
                nested_segments, nested_segment_count);
            continue;
        }

        prefixed_path = pgy_arena_fmt(&ctx->scratch_arena, "%s.%s",
            field->name, nested_path);
        if (prefixed_path == NULL) {
            pgy_domain_projection_path_segments_destroy(
                nested_segments, nested_segment_count);
            pgy_domain_projection_path_segments_destroy(
                resolved_segments, resolved_segment_count);
            pgy_decl_field_model_free(fields, field_count);
            return -1;
        }

        if (segments_out != NULL) {
            if (nested_segment_count == SIZE_MAX
                || nested_segment_count + 1
                    > SIZE_MAX / sizeof(*candidate_segments)) {
                pgy_domain_projection_path_segments_destroy(
                    nested_segments, nested_segment_count);
                pgy_domain_projection_path_segments_destroy(
                    resolved_segments, resolved_segment_count);
                pgy_decl_field_model_free(fields, field_count);
                return -1;
            }
            candidate_segment_count = nested_segment_count + 1;
            candidate_segments = calloc(
                candidate_segment_count, sizeof(*candidate_segments));
            if (candidate_segments == NULL
                || !projection_path_segment_init(
                    &candidate_segments[0], field, field_type)) {
                pgy_domain_projection_path_segments_destroy(
                    candidate_segments, candidate_segment_count);
                pgy_domain_projection_path_segments_destroy(
                    nested_segments, nested_segment_count);
                pgy_domain_projection_path_segments_destroy(
                    resolved_segments, resolved_segment_count);
                pgy_decl_field_model_free(fields, field_count);
                return -1;
            }
            if (nested_segment_count > 0) {
                memcpy(&candidate_segments[1], nested_segments,
                       nested_segment_count * sizeof(*nested_segments));
            }
            free(nested_segments);
            nested_segments = NULL;
            nested_segment_count = 0;
        }

        match_count++;
        if (match_count == 1) {
            resolved_path = prefixed_path;
            resolved_type = nested_type;
            resolved_segments = candidate_segments;
            resolved_segment_count = candidate_segment_count;
        } else {
            pgy_domain_projection_path_segments_destroy(
                candidate_segments, candidate_segment_count);
            pgy_domain_projection_path_segments_destroy(
                resolved_segments, resolved_segment_count);
            resolved_segments = NULL;
            resolved_segment_count = 0;
            resolved_path = NULL;
            resolved_type = NULL;
        }
    }

    pgy_decl_field_model_free(fields, field_count);
    if (ambiguous || match_count > 1) {
        pgy_domain_projection_path_segments_destroy(
            resolved_segments, resolved_segment_count);
        return 2;
    }
    if (match_count == 1) {
        if (path_out != NULL)
            *path_out = resolved_path;
        if (field_type_out != NULL)
            *field_type_out = resolved_type;
        if (segments_out != NULL)
            *segments_out = resolved_segments;
        if (segment_count_out != NULL)
            *segment_count_out = resolved_segment_count;
        return 1;
    }
    return 0;
}

int
semantic_resolve_projection_source_field_path(SemanticContext *ctx,
                                              ASTNode *source_decl,
                                              const char *field_name,
                                              const char **path_out,
                                              Type **field_type_out)
{
    return resolve_projection_source_field_path_rec(source_decl, field_name, 0,
        ctx, path_out, field_type_out, NULL, NULL);
}

int
semantic_resolve_projection_source_field_path_with_segments(
    SemanticContext *ctx,
    ASTNode *source_decl,
    const char *field_name,
    const char **path_out,
    Type **field_type_out,
    PgyDomainProjectionPathSegmentFact **segments_out,
    size_t *segment_count_out)
{
    return resolve_projection_source_field_path_rec(source_decl, field_name, 0,
        ctx, path_out, field_type_out, segments_out, segment_count_out);
}
