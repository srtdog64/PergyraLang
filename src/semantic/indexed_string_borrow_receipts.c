/* Indexed-String borrow receipts: the storage source a borrowed String's
 * Array names, the receipt check at prepare and finalize, and whether an
 * owner may hand that Array on while a deferred read still uses it. */
#include "formal_string_store_landing_owner.h"
#include "array_storage_release_owner.h"
#include "array_storage_element_lifetime_owner.h"
#include "collection_ownership_fact.h"
#include "type_checker_internal.h"
#include "type_checker_resolution_internal.h"
#include "collection_owned_element_requirement_owner.h"
#include "indexed_string_named_call_owner.h"
#include "builtin_kind.h"
#include "diag_codes.h"
#include "slot_summary.h"
#include "array_storage_deferred_preservation_owner.h"
#include "function_param_publication_internal.h"
#include "function_param_flow_summary_internal.h"
#include "indexed_string_borrow_owner.h"
#include "indexed_string_field_lineage_internal.h"
#include "indexed_string_nonowner_risk_internal.h"
#include "indexed_string_definition_recipe.h"
#include "indexed_string_permanent_element.h"
#include "loan_copy_landing_owner.h"

#include <string.h>

bool
semantic_indexed_string_borrow_storage_source_current(ASTNode *expression,
    SemanticContext *ctx, uint32_t *source_id)
{
    if (expression == NULL || expression->type != AST_IDENTIFIER || source_id == NULL
        || !publication_generation_ready(ctx)) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, expression, false);
    if (row == NULL || row->id == 0 || row->id != ast_node_stable_id(expression)
        || !row->value_present || row->meta || row->value_shape != AST_IDENTIFIER
        || row->binding_id == 0 || row->binding_id != ast_identifier_binding_syntax_id(expression)
        || !type_is_constructed_named(row->value_type, "Array")
        || !type_equals(type_get_constructed_arg(row->value_type, 0), TYPE_STRING)
        || !row->indexed_array_source_present || row->indexed_array_source_id == 0
        || (semantic_param_publication_inputs_ready(ctx)
            && !semantic_param_publication_value(ctx, expression).ok)) return false;
    *source_id = row->indexed_array_source_id;
    return true;
}

static bool indexed_string_borrow_receipts(SemanticContext *ctx, bool prepare)
{
    if (ctx == NULL || ctx->has_error || !semantic_param_publication_ready(ctx)) return false;
    FunctionParamPublicationStore *store = ctx->function_param_publications;
    size_t issued = 0, private_issued = 0;
    ASTNode *first_copy = NULL, *first_private_copy = NULL;
    if (!semantic_indexed_string_nonowner_receipts(ctx, prepare)) return false;
    if (!semantic_indexed_string_field_receipts(ctx, prepare)) return false;
    if (!semantic_indexed_string_permanent_receipts(ctx, prepare)) return false;
    if (!semantic_indexed_string_definition_receipts(ctx, prepare)) return false;
    for (size_t i = 0; i < store->node_capacity; i++) {
        PublishedNode *row = &store->nodes[i];
        if (!row->indexed_string_shallow_retention) continue;
        if (prepare) {
            const ASTNode *dependency = row->node;
            bool lineage = dependency != NULL && (dependency->type != AST_IDENTIFIER ||
                semantic_indexed_string_borrow_result_dependency_current(ctx, (ASTNode *)dependency, &dependency));
            uint32_t ignored_source = UINT32_MAX;
            if (lineage && dependency != NULL && dependency->type == AST_CALL &&
                ast_call_semantic_callee_declared_callable(dependency))
                (void)function_param_flow_string_result_source_prepare(ctx, dependency, &ignored_source);
        }
        uint32_t source = UINT32_MAX;
        bool current = issued != SIZE_MAX && row->node != NULL && row->id != 0
            && row->id == ast_node_stable_id(row->node)
            && semantic_indexed_string_borrow_source_current((ASTNode *)row->node, ctx, &source)
            && source == 0;
        if (!current && prepare && row->node != NULL && row->id != 0 &&
            row->id == ast_node_stable_id(row->node) && row->value_present &&
            !row->meta && row->string_value && type_equals(row->value_type, TYPE_STRING) &&
            semantic_loan_copy_landing_select_string(ctx, row->node)) {
            if (first_copy == NULL) first_copy = (ASTNode *)row->node;
            continue;
        }
        if (!current) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                (ASTNode *)row->node,
                "A shallow String store has no current independent-result proof");
            return false;
        }
        issued++;
    }
    if (first_copy != NULL) {
        semantic_loan_copy_landing_error_with_hints(ctx,
            PGY_CODE_SEM_BORROW_ESCAPE, PGY_CAUSE_BORROW_ESCAPE,
            PGY_FIX_USE_MOVE_OR_RETAIN_BINDING, first_copy,
            "A retained String without an independent-result proof is copied where it is stored");
        return false;
    }
    if (issued != store->indexed_string_shallow_retention_count) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
            PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
            ctx->program_root, "A shallow String store receipt was withdrawn");
        return false;
    }
    for (size_t i = 0; i < store->node_capacity; i++) {
        PublishedNode *row = &store->nodes[i];
        if (row->private_string_push == NULL) continue;
        bool current = private_issued != SIZE_MAX &&
            (!prepare || semantic_indexed_string_prepare_private_push(ctx, row->node)) &&
            semantic_indexed_string_private_push_current(ctx, row->node);
        if (!current && prepare &&
            semantic_formal_string_store_landing_select_owned(ctx, row->node)) {
            if (first_private_copy == NULL) first_private_copy = (ASTNode *)row->node;
            continue;
        }
        if (!current) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                (ASTNode *)row->node,
                "A private String push has no current nonlocal source proof");
            return false;
        }
        private_issued++;
    }
    if (first_private_copy != NULL) {
        semantic_loan_copy_landing_error_with_hints(ctx,
            PGY_CODE_SEM_BORROW_ESCAPE, PGY_CAUSE_BORROW_ESCAPE,
            PGY_FIX_USE_MOVE_OR_RETAIN_BINDING, first_private_copy,
            "A private String push without a current source proof stores an owned copy");
        return false;
    }
    if (private_issued != store->private_string_push_count) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
            PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
            ctx->program_root, "A private String push receipt was withdrawn");
        return false;
    }
    return true;
}

bool semantic_indexed_string_borrow_prepare(SemanticContext *ctx)
{
    return indexed_string_borrow_receipts(ctx, true);
}
bool semantic_indexed_string_borrow_finalize(SemanticContext *ctx)
{
    return indexed_string_borrow_receipts(ctx, false);
}

bool semantic_indexed_string_borrow_owner_transfer_allowed(ASTNode *receiver, SemanticContext *ctx)
{
    if (ctx == NULL || ctx->has_error) return false;
    bool tracked = false;
    uint32_t source_id = 0;
    if (!semantic_indexed_string_borrow_array_receiver_source(receiver, ctx,
            &tracked, &source_id)) return false;
    if (!tracked) return true;
    for (Scope *s = ctx->scope; s != NULL; s = s->kind == SCOPE_FUNCTION ? NULL : s->parent) {
        if (s->indexed_string_deferred_read_count > s->indexed_string_deferred_read_capacity
            || (s->indexed_string_deferred_read_count != 0 && s->indexed_string_deferred_reads == NULL))
            return semantic_array_storage_deferred_read_failure(receiver, ctx, "missing active read storage");
        for (size_t i = 0; i < s->indexed_string_deferred_read_count; i++) {
            const IndexedStringDeferredRead *row = &s->indexed_string_deferred_reads[i];
            if (!semantic_array_storage_deferred_read_current(row, ctx))
                return semantic_array_storage_deferred_read_failure(receiver, ctx, "stale deferred read identity");
            if (row->source_id == 0) continue;
            if (!row->owner_release && !row->nested && ctx->in_defer_cleanup
                && row->body == ctx->indexed_string_defer_body)
                continue; /* Earlier reads in this cleanup obey ordinary sequential use. */
            if (source_id == 0 || source_id == UINT32_MAX
                || row->source_id == UINT32_MAX || row->source_id == source_id)
                return semantic_array_storage_deferred_read_failure(receiver, ctx,
                    "a later cleanup still reads this source");
        }
    }
    return true;
}
