/* Slice views of an excluded empty Array<String>. The exclusion rests on the
 * Array holding no String of its own. Once its stores are promoted to owned
 * copies it does, and a String read through a Slice view of it has the
 * unknown source, so a recorded String of unknown source may be one of its
 * payloads. This owner decides when that holds and which uses then need an
 * independent copy (decision
 * 2026-11-20_unknown_string_store_copied_past_list_or_promoted_array). */
#include "indexed_string_nonowner_risk_internal.h"

#include <string.h>

#include "function_param_flow/function_param_publication_owner.h"
#include "loan_copy_landing_owner.h"
#include "type_checker/type_checker_internal.h"

/* The row reads a String element through a Slice view: `view[i]`, or a
 * for-in over a Slice<String>. */
static bool nonowner_row_reads_view(const FunctionParamFlowIndexClaims *claims,
    const FunctionParamFlowIndexClaim *row)
{
    if (row->node == NULL ||
        (row->shape != AST_ARRAY_ACCESS && row->shape != AST_FOR_LOOP)) return false;
    const ASTNode *base = row->shape == AST_ARRAY_ACCESS
        ? ast_array_access_array(row->node) : ast_for_iterable(row->node);
    for (size_t c = 0; base != NULL && c < row->child_count; c++) {
        if (row->children[c] != base || row->child_claim_ordinals == NULL ||
            row->child_claim_ordinals[c] >= claims->count) continue;
        const Type *type = claims->rows[row->child_claim_ordinals[c]].value_type;
        return type_is_constructed_named(type, "Slice") &&
            type_equals(type_get_constructed_arg(type, 0), TYPE_STRING);
    }
    return false;
}

bool semantic_indexed_string_nonowner_view_reaches(
    const IndexedStringNonownerRisk *risk,
    const FunctionParamFlowIndexClaims *claims)
{
    bool viewed = false, read = false;
    for (size_t i = 0; i < claims->count && !(viewed && read); i++) {
        const FunctionParamFlowIndexClaim *row = &claims->rows[i];
        read = read || nonowner_row_reads_view(claims, row);
        if (row->shape != AST_MEMBER_ACCESS || row->node == NULL) continue;
        /* `xs[a..b]` is `xs.Slice(a, b)`. */
        ASTNode *object = ast_member_object(row->node);
        const char *member = ast_member_name(row->node);
        viewed = viewed || (object != NULL && object->type == AST_IDENTIFIER &&
            ast_identifier_binding_syntax_id(object) == risk->binding_id &&
            member != NULL && strcmp(member, "Slice") == 0);
    }
    return viewed && read;
}

/* The use stores its String into the excluded Array itself: one of the
 * Array's own owned stores once promoted. */
static bool nonowner_use_stored_in_receiver(const IndexedStringNonownerRisk *risk,
    const IndexedStringNonownerUse *use)
{
    uint32_t kind = 0, receiver = 0, source = 0;
    ASTNode *target = use->sink != NULL && ast_call_arg_count(use->sink) != 0
        ? ast_call_argument(use->sink, 0) : NULL;
    return use->actual_ordinal != 0 && target != NULL &&
        target->type == AST_IDENTIFIER &&
        ast_identifier_binding_syntax_id(target) == risk->binding_id &&
        ast_call_semantic_collection_effect(use->sink, &kind, &receiver, &source) &&
        receiver == risk->binding_id;
}

bool semantic_indexed_string_nonowner_use_needs_copy(SemanticContext *ctx,
    const IndexedStringNonownerRisk *risk, const IndexedStringNonownerUse *use)
{
    bool depends = true, retains = true;
    if (nonowner_use_stored_in_receiver(risk, use) ||
        (semantic_loan_copy_landing_selected(ctx, use->value) &&
            semantic_loan_copy_landing_carrier_copies(ctx, use->value)))
        return false;
    /* A declared callee whose checked result neither depends on nor retains
     * the String only reads it while the Array still holds it. */
    return !(ast_call_semantic_callee_declared_callable(use->sink) &&
        semantic_param_publication_checked_result_relation(ctx, use->sink,
            use->actual_ordinal, &depends, &retains) && !depends && !retains);
}

bool semantic_indexed_string_nonowner_select_view_copies(SemanticContext *ctx,
    const IndexedStringNonownerRisk *risk, ASTNode **landing)
{
    *landing = NULL;
    for (size_t u = 0; u < risk->use_count; u++) {
        const IndexedStringNonownerUse *use = &risk->uses[u];
        if (!semantic_indexed_string_nonowner_use_needs_copy(ctx, risk, use))
            continue;
        if (*landing == NULL) *landing = (ASTNode *)use->value;
        if (!semantic_loan_copy_landing_carrier_copies(ctx, use->value) ||
            !semantic_loan_copy_landing_select_string(ctx, use->value)) {
            *landing = (ASTNode *)use->value;
            return false;
        }
    }
    return *landing != NULL;
}
