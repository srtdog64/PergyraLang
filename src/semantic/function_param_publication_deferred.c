/* One all-typed publication boundary. Pending rows own diagnostic strings,
 * borrow compilation-owned AST/Type identity, and never retain scope Symbols. */
#include "function_param_publication_internal.h"
#include "typed_map_query_fact_owner.h"
#include "type_checker_internal.h"
#include "type_checker_resolution_internal.h"
#include "type_checker_ownership_internal.h"
#include "slot_analyzer_internal.h"
#include "function_param_flow_summary_internal.h"
#include "callable_capability_inference.h"
#include "region_retention_summary.h"
#include "indexed_string_borrow_owner.h"
#include "collection_ownership_fact.h"
#include "../common/string_compat.h"
#include <stdlib.h>
#include <string.h>
void semantic_param_publication_retire_deferred(PublishedNode *row)
{
    if (row == NULL) return;
    for (size_t c = 0; c < row->deferred_capture_count; c++) {
        free(row->deferred_captures[c].name); free(row->deferred_captures[c].type_name);
    }
    free(row->deferred_captures);
    function_param_flow_claims_destroy(row->deferred_syntax);
    row->deferred_captures = NULL; row->deferred_capture_count = 0;
    row->deferred_syntax = NULL; row->deferred_scan = row->deferred_checked = false;
}
static bool publication_deferred_kind(ASTNodeType kind)
{
    return kind == AST_LAMBDA_EXPR || kind == AST_ASYNC_BLOCK || kind == AST_SPAWN_EXPR ||
        kind == AST_EVENT_SUBSCRIBE || kind == AST_EVENT_UNSUBSCRIBE || kind == AST_EVENT_INVOKE;
}
bool semantic_param_publication_record_deferred_capture(SemanticContext *ctx, ASTNode *lambda,
    ASTNode *source, uint32_t binding_id, const Type *type, LambdaCaptureMode mode)
{
    if (!semantic_param_publication_pending(ctx)) return true;
    if (ctx->has_error || lambda == NULL || lambda->type != AST_LAMBDA_EXPR ||
        source == NULL || source->type != AST_IDENTIFIER || ast_node_stable_id(source) == 0 ||
        binding_id == 0 || type == NULL || type->kind != TYPE_KIND_PRIMITIVE ||
        type->name == NULL || ast_identifier_name(source) == NULL || mode != LAMBDA_CAPTURE_COPY) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, lambda, true);
    if (row == NULL) return false;
    for (size_t c = 0; c < row->deferred_capture_count; c++)
        if (row->deferred_captures[c].binding_id == binding_id)
            return row->deferred_captures[c].mode == mode && row->deferred_captures[c].type == type;
    size_t count = row->deferred_capture_count;
    if (count == SIZE_MAX / sizeof(*row->deferred_captures)) return false;
    PublishedDeferredCapture *captures = realloc(row->deferred_captures, (count + 1) * sizeof(*captures));
    if (captures == NULL) return false;
    row->deferred_captures = captures;
    captures[count] = (PublishedDeferredCapture){.source = source, .source_id = ast_node_stable_id(source),
        .binding_id = binding_id, .category = semantic_classify_ownership_type(type, ctx),
        .type = type, .mode = mode, .copy_escape = publication_physical_scalar(type),
        .name = pergyra_strdup(ast_identifier_name(source)), .type_name = pergyra_strdup(type->name)};
    row->deferred_capture_count++;
    return captures[count].name != NULL && captures[count].type_name != NULL;
}
bool semantic_param_publication_record_deferred_scan(SemanticContext *ctx, ASTNode *boundary)
{
    if (!semantic_param_publication_pending(ctx)) return true;
    if (ctx->has_error || boundary == NULL || !publication_deferred_kind(boundary->type)) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, boundary, true);
    if (row == NULL) return false;
    row->deferred_scan = true; row->deferred_checked = false; row->deferred_shape = boundary->type;
    return true;
}
static bool publication_capture_metadata_current(const ASTNode *lambda, const PublishedNode *row)
{
    if (row->deferred_capture_count != ast_lambda_capture_count(lambda) ||
        (row->deferred_capture_count != 0 && row->deferred_captures == NULL)) return false;
    for (size_t c = 0; c < row->deferred_capture_count; c++) {
        const PublishedDeferredCapture *capture = &row->deferred_captures[c];
        const char *name = ast_lambda_capture_name(lambda, c), *type = ast_lambda_capture_type_name(lambda, c);
        if (capture->source == NULL || capture->source->type != AST_IDENTIFIER ||
            capture->source_id == 0 || capture->source_id != ast_node_stable_id(capture->source) ||
            capture->binding_id == 0 || capture->type == NULL || capture->type->kind != TYPE_KIND_PRIMITIVE ||
            capture->type->name == NULL || capture->name == NULL || capture->type_name == NULL ||
            ast_identifier_name(capture->source) == NULL ||
            name == NULL || type == NULL || capture->mode != LAMBDA_CAPTURE_COPY ||
            capture->mode != ast_lambda_capture_mode(lambda, c) ||
            strcmp(name, capture->name) != 0 || strcmp(type, capture->type_name) != 0 ||
            strcmp(ast_identifier_name(capture->source), capture->name) != 0 ||
            strcmp(capture->type->name, capture->type_name) != 0 ||
            capture->copy_escape != publication_physical_scalar(capture->type)) return false;
    }
    return true;
}
bool semantic_param_publication_record_checked_deferred(SemanticContext *ctx, ASTNode *boundary)
{
    if (!semantic_param_publication_pending(ctx)) return true;
    if (ctx->has_error || boundary == NULL) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, boundary, false);
    if (row == NULL || !row->deferred_scan || row->deferred_shape != boundary->type ||
        !publication_deferred_kind(boundary->type) ||
        (boundary->type == AST_LAMBDA_EXPR && !publication_capture_metadata_current(boundary, row))) return false;
    if (boundary->type == AST_SPAWN_EXPR) {
        ASTNode *call = ast_spawn_function(boundary);
        PublishedNode *checked = publication_node(ctx->function_param_publications, call, false);
        if (ast_spawn_arg_count(boundary) != 0 || call == NULL || call->type != AST_CALL ||
            !publication_call_ready(checked) || checked->value_binding != 0) return false;
    }
    FunctionParamFlowIndexClaims *syntax = NULL;
    if (!function_param_flow_syntax_claims_snapshot(ctx, &syntax, boundary)) {
        function_param_flow_claims_destroy(syntax); return false;
    }
    row = publication_node(ctx->function_param_publications, boundary, false);
    if (row == NULL) { function_param_flow_claims_destroy(syntax); return false; }
    function_param_flow_claims_destroy(row->deferred_syntax);
    row->deferred_syntax = syntax; row->deferred_checked = true;
    return true;
}
bool semantic_param_publication_query_deferred(const SemanticContext *ctx,
    const ASTNode *boundary, SemanticPublishedDeferred *view)
{
    if (view == NULL) return false;
    *view = (SemanticPublishedDeferred){0};
    if (!semantic_param_publication_inputs_ready(ctx) || boundary == NULL) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, boundary, false);
    if (row == NULL || !row->deferred_scan || !row->deferred_checked ||
        row->deferred_shape != boundary->type || !publication_deferred_kind(boundary->type) ||
        !function_param_flow_syntax_claims_current(ctx, row->deferred_syntax)) return false;
    if ((boundary->type == AST_LAMBDA_EXPR || boundary->type == AST_SPAWN_EXPR) &&
        publication_current_value(ctx, boundary) == NULL) return false;
    if (boundary->type == AST_LAMBDA_EXPR) {
        if (!publication_capture_metadata_current(boundary, row)) return false;
        for (size_t c = 0; c < row->deferred_capture_count; c++) {
            const PublishedDeferredCapture *capture = &row->deferred_captures[c];
            PublishedNode *source = publication_current_value(ctx, capture->source);
            if (source == NULL || source->binding_id != capture->binding_id ||
                capture->binding_id != ast_identifier_binding_syntax_id(capture->source) ||
                capture->category != source->category || capture->copy_escape != source->copy_escape ||
                capture->category != OWNERSHIP_TYPE_COPY_ONLY) return false;
        }
    } else if (row->deferred_capture_count != 0) return false;
    if (boundary->type == AST_SPAWN_EXPR &&
        (ast_spawn_arg_count(boundary) != 0 || ast_spawn_function(boundary) == NULL ||
         ast_spawn_function(boundary)->type != AST_CALL ||
         !semantic_param_publication_checked_call(ctx, ast_spawn_function(boundary)))) return false;
    if (boundary->type == AST_EVENT_SUBSCRIBE || boundary->type == AST_EVENT_UNSUBSCRIBE ||
        boundary->type == AST_EVENT_INVOKE) {
        const ASTNode *event = boundary->type == AST_EVENT_INVOKE ?
            ast_event_invoke_event(boundary) : ast_event_op_event(boundary);
        if (publication_current_value(ctx, event) == NULL) return false;
        size_t count = boundary->type == AST_EVENT_INVOKE ? ast_event_invoke_arg_count(boundary) : 1;
        for (size_t a = 0; a < count; a++) {
            const ASTNode *arg = boundary->type == AST_EVENT_INVOKE ?
                ast_event_invoke_argument(boundary, a) : ast_event_op_handler(boundary);
            if (publication_current_value(ctx, arg) == NULL) return false;
            if (arg->type == AST_LAMBDA_EXPR) {
                SemanticPublishedDeferred handler;
                if (!semantic_param_publication_query_deferred(ctx, arg, &handler)) return false;
            }
        }
    }
    view->captures = row->deferred_captures; view->capture_count = row->deferred_capture_count;
    return true;
}
