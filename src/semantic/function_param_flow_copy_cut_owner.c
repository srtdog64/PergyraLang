/* Copy placement for a returned carrier with fresh, uncopyable siblings.
 * The normal backing frames own origin membership. Typed constructor fields,
 * local definitions and return claims own the reverse projection; this query
 * builds no persistent graph and publishes no summary or admission. */
#include "function_param_flow_summary_internal.h"
#include "function_param_publication_internal.h"
#include "loan_copy_landing_owner.h"
#include "default_aggregate_move_rule.h"
#include "type_checker_internal.h"
#include <stdint.h>
#include <string.h>

typedef struct CopyCutFrame {
    ASTNode *routine;
    size_t input;
    FunctionParamFlowBackingFact *fact;
    FunctionParamFlowIndexClaims *claims;
    const struct CopyCutFrame *parent;
    ASTNode *call;
    size_t actual;
} CopyCutFrame;

typedef struct {
    SemanticContext *ctx;
    FunctionParamFlowSummaryStore *store;
    FunctionParamFlowLoanEscapes copies;
    size_t work;
    bool mutable_assignments;
} CopyCut;

static bool cut_frame(CopyCut *cut, ASTNode *routine, size_t input,
    CopyCutFrame *frame)
{
    size_t entry = function_param_flow_find(cut->store,
        ast_node_stable_id(routine), input);
    if (entry == SIZE_MAX ||
        cut->store->entries[entry].state != FUNCTION_PARAM_FLOW_COMPLETE ||
        !function_param_flow_output_current(cut->store, entry, false) ||
        !function_param_flow_backing_rows_current(cut->store, routine))
        return false;
    size_t at = function_param_flow_program_point_index_find(cut->store,
        ast_node_stable_id(routine));
    if (at == SIZE_MAX) return false;
    FunctionParamFlowProgramPointIndex *index =
        &cut->store->program_point_indexes[at];
    FunctionParamFlowBackingFact *fact = index->backing_frames != NULL &&
        input < index->backing_frame_count ? index->backing_frames[input] : NULL;
    FuncParam *formal = ast_func_param(routine, input);
    if (formal == NULL || fact == NULL || fact->input_ordinal != input ||
        fact->input_id != ast_func_param_stable_id(formal) ||
        fact->epoch == 0 || !fact->complete || fact->building || fact->pending ||
        fact->analysis_unsupported || fact->claims != index->claims ||
        !function_param_flow_index_claims_current(cut->ctx, index))
        return false;
    *frame = (CopyCutFrame){.routine = routine, .input = input,
        .fact = fact, .claims = index->claims};
    return true;
}

static bool cut_value(CopyCut *cut, const CopyCutFrame *frame, ASTNode *value,
    const FunctionParamFlowResultPath *path, size_t depth);

static bool cut_append_copy(CopyCut *cut, ASTNode *value)
{
    if (cut == NULL || value == NULL ||
        !semantic_loan_copy_landing_carrier_copies(cut->ctx, value))
        return false;
    for (size_t i = 0; i < cut->copies.count; i++)
        if (cut->copies.at[i] == value) return true;
    if (cut->copies.count == sizeof(cut->copies.at) /
            sizeof(*cut->copies.at))
        return false;
    cut->copies.at[cut->copies.count++] = value;
    return true;
}

static bool cut_original_projection(CopyCut *cut, const CopyCutFrame *frame,
    ASTNode *value, const FunctionParamFlowResultPath *path, bool *original)
{
    *original = false;
    if (path->count != 0 || (value->type != AST_MEMBER_ACCESS &&
            value->type != AST_ARRAY_ACCESS)) return true;
    ASTNode *caller = NULL, *root = NULL;
    if (!semantic_param_publication_projection_root_current(cut->ctx, value,
            &caller, &root) || caller != frame->routine || root == NULL)
        return false;
    FuncParam *formal = ast_func_param(frame->routine, frame->input);
    if (formal == NULL || formal->mode != PARAM_MODE_REF ||
        semantic_param_publication_binding(cut->ctx, root) !=
            ast_func_param_stable_id(formal)) return true;
    *original = true;
    SemanticPublishedValue published =
        semantic_param_publication_value(cut->ctx, value);
    return published.ok && !published.meta &&
        (type_equals(published.type, TYPE_STRING) ||
            semantic_default_aggregate_value(cut->ctx, published.type)) &&
        cut_append_copy(cut, value);
}

/* An Array literal allocates the descriptor returned by its routine. Copy the
 * exact input-dependent element at that construction boundary rather than
 * cloning the completed Array at its caller. A whole-result Clone would leave
 * the original temporary descriptor without a cleanup consumer. */
static bool cut_array_literal(CopyCut *cut, const CopyCutFrame *frame,
    ASTNode *literal, const FunctionParamFlowResultPath *path, size_t depth)
{
    if (path->count != 0 || literal == NULL ||
        literal->type != AST_ARRAY_LITERAL)
        return false;
    for (size_t i = 0; i < ast_array_literal_count(literal); i++) {
        ASTNode *element = ast_array_literal_element(literal, i);
        bool carries = false;
        if (element == NULL ||
            !function_param_flow_backing_value_origin_current(cut->store,
                frame->fact, element, &carries))
            return false;
        if (!carries) continue;
        if (cut_append_copy(cut, element)) continue;
        if (!cut_value(cut, frame, element, path, depth + 1))
            return false;
    }
    return true;
}

static bool cut_local(CopyCut *cut, const CopyCutFrame *frame, ASTNode *value,
    const FunctionParamFlowResultPath *path, size_t depth)
{
    uint32_t binding = semantic_param_publication_binding(cut->ctx, value);
    FuncParam *formal = ast_func_param(frame->routine, frame->input);
    if (binding == 0 || formal == NULL) return false;
    if (binding == ast_func_param_stable_id(formal))
        return frame->parent != NULL && cut_value(cut, frame->parent,
            ast_call_argument(frame->call, frame->actual), path, depth + 1);
    ASTNode *definition = NULL;
    for (size_t c = 0; c < frame->claims->count; c++) {
        const FunctionParamFlowIndexClaim *row = &frame->claims->rows[c];
        if (row->shape == AST_ASSIGNMENT && row->node != NULL) {
            ASTNode *target = ast_assignment_target(row->node);
            if (target != NULL && target->type == AST_IDENTIFIER &&
                semantic_param_publication_binding(cut->ctx, target) == binding) {
                if (!cut->mutable_assignments) return false;
                if (!row->execution_present) return false;
                if (!row->reachable) continue;
                if (!cut_value(cut, frame,
                        ast_assignment_value(row->node), path, depth + 1))
                    return false;
                continue;
            }
            ASTNode *caller = NULL, *root = NULL;
            if (target != NULL &&
                semantic_param_publication_projection_root_current(
                    cut->ctx, target, &caller, &root) &&
                caller == frame->routine && root != NULL &&
                semantic_param_publication_binding(cut->ctx, root) == binding)
                return false;
        }
        if (row->id != binding || row->shape != AST_LET_DECL) continue;
        /* The claims above reject actual replacements when this cut cannot
         * follow them; a mutable declaration alone is not a replacement. */
        if (definition != NULL || row->node == NULL ||
            ast_let_is_alias(row->node))
            return false;
        definition = row->node;
    }
    return definition != NULL && cut_value(cut, frame,
        ast_let_initializer(definition), path, depth + 1);
}

static bool cut_call(CopyCut *cut, const CopyCutFrame *frame, ASTNode *call,
    const FunctionParamFlowResultPath *path, size_t depth)
{
    PublishedNode *published = publication_result_current(cut->ctx, call);
    const SemanticNominalResultSnapshot *nominal = published != NULL
        ? published->nominal_result : NULL;
    if (nominal != NULL) {
        if (!semantic_nominal_result_snapshot_current(cut->ctx, call, nominal) ||
            nominal->field_count != ast_call_arg_count(call)) return false;
        for (size_t a = 0; a < nominal->field_count; a++) {
            if (path->count != 0 && nominal->fields[a].field_id != path->fields[0])
                continue;
            FunctionParamFlowResultPath inside = *path;
            if (inside.count != 0) {
                inside.count--;
                memmove(inside.fields, &inside.fields[1],
                    inside.count * sizeof(*inside.fields));
            }
            if (!cut_value(cut, frame, ast_call_argument(call, a), &inside,
                    depth + 1)) return false;
            if (path->count != 0) return true;
        }
        return path->count == 0;
    }
    ASTNode *callee = semantic_param_publication_direct_callee(cut->ctx, call);
    ASTNode *receiver = NULL;
    if (callee == NULL || !semantic_param_publication_checked_call(cut->ctx, call) ||
        !semantic_param_publication_instance_receiver(cut->ctx, call, &receiver) ||
        receiver != NULL) return false;
    bool reached = false;
    for (size_t a = 0; a < ast_call_arg_count(call); a++) {
        bool carries = false;
        ASTNode *actual = ast_call_argument(call, a);
        if (!function_param_flow_backing_value_origin_current(cut->store,
                frame->fact, actual, &carries)) return false;
        if (!carries) continue;
        size_t input = SIZE_MAX;
        if (!semantic_param_publication_formal_for_actual(cut->ctx, call, a,
                &input)) return false;
        FuncParam *formal = ast_func_param(callee, input);
        size_t entry = function_param_flow_find(cut->store,
            ast_node_stable_id(callee), input);
        unsigned outward = SLOT_PARAM_SUMMARY_NONE;
        if (formal == NULL || (formal->mode != PARAM_MODE_DEFAULT &&
                formal->mode != PARAM_MODE_REF) || entry == SIZE_MAX ||
            !function_param_flow_entry_outward_effects_current(cut->store,
                entry, &outward)) return false;
        const unsigned escapes = SLOT_PARAM_SUMMARY_RETURN_ESCAPE |
            SLOT_PARAM_SUMMARY_CALL_ESCAPE | SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE;
        if (outward == 0) continue;
        if (outward != SLOT_PARAM_SUMMARY_RETURN_ESCAPE ||
            (cut->store->entries[entry].mask &
                ~(SLOT_PARAM_SUMMARY_READ | escapes)) != 0 ||
            cut->store->entries[entry].array_storage_unproved ||
            cut->store->entries[entry].array_buffer_unkept) return false;
        CopyCutFrame child;
        if (!cut_frame(cut, callee, input, &child)) return false;
        child.parent = frame; child.call = call; child.actual = a;
        bool returned = false;
        for (size_t c = 0; c < child.claims->count; c++) {
            const FunctionParamFlowIndexClaim *row = &child.claims->rows[c];
            if (row->shape != AST_RETURN) continue;
            if (!row->execution_present || row->node == NULL) return false;
            if (!row->reachable) continue;
            returned = true;
            if (!cut_value(cut, &child, ast_return_value(row->node), path,
                    depth + 1)) return false;
        }
        if (!returned) return false;
        reached = true;
    }
    return reached;
}

static bool cut_value(CopyCut *cut, const CopyCutFrame *frame, ASTNode *value,
    const FunctionParamFlowResultPath *path, size_t depth)
{
    if (value == NULL || depth > FUNCTION_PARAM_FLOW_RESULT_PATH_LIMIT ||
        ++cut->work > cut->store->work_budget ||
        !function_param_flow_result_path_valid(path) || !path->present ||
        path->branch_count != 0) return false;
    bool carries = false;
    if (!function_param_flow_backing_value_origin_current(cut->store,
            frame->fact, value, &carries)) return false;
    if (!carries) return true;
    bool original = false;
    if (!cut_original_projection(cut, frame, value, path, &original)) return false;
    if (original) return true;
    if (value->type == AST_IDENTIFIER)
        return cut_local(cut, frame, value, path, depth);
    if (value->type == AST_MEMBER_ACCESS) {
        ASTNode *caller = NULL, *receiver = NULL, *root = NULL;
        SemanticPublishedSelectedField field = {0};
        FunctionParamFlowResultPath selected = {0};
        return semantic_param_publication_member_projection_field_current(
                cut->ctx, value, &caller, &receiver, &root, &field) &&
            caller == frame->routine &&
            function_param_flow_result_path_prepend(field.field_id, path,
                &selected) && cut_value(cut, frame, receiver, &selected, depth + 1);
    }
    if (value->type == AST_ARRAY_LITERAL)
        return cut_array_literal(cut, frame, value, path, depth);
    return value->type == AST_CALL && cut_call(cut, frame, value, path, depth);
}

bool function_param_flow_local_ref_copy_cut_current(SemanticContext *ctx,
    ASTNode *routine, const ASTNode *value, FunctionParamFlowLoanEscapes *copies)
{
    if (copies != NULL) *copies = (FunctionParamFlowLoanEscapes){0};
    size_t input = SIZE_MAX;
    if (copies == NULL ||
        function_param_flow_local_ref_carrier_copy_source_current(ctx, routine,
            value) == NULL ||
        !function_param_flow_value_backing_formal_current(ctx, routine, value,
            &input) || ast_func_param(routine, input)->mode != PARAM_MODE_REF)
        return false;
    CopyCut cut = {.ctx = ctx, .store = ctx->function_param_flow_summaries};
    CopyCutFrame frame;
    FunctionParamFlowResultPath path = {.present = true};
    if (!cut_frame(&cut, routine, input, &frame) ||
        !cut_value(&cut, &frame, (ASTNode *)value, &path, 0) ||
        cut.copies.count == 0) return false;
    *copies = cut.copies;
    return true;
}

bool function_param_flow_ref_call_copy_cut_current(SemanticContext *ctx,
    ASTNode *call, size_t actual, FunctionParamFlowLoanEscapes *copies)
{
    if (copies != NULL) *copies = (FunctionParamFlowLoanEscapes){0};
    if (ctx == NULL || copies == NULL || call == NULL ||
        call->type != AST_CALL || actual >= ast_call_arg_count(call) ||
        !semantic_param_publication_checked_argument(ctx, call, actual))
        return false;

    ASTNode *callee = semantic_param_publication_callee(ctx, call, actual);
    size_t formal = SIZE_MAX;
    if (callee == NULL || ast_func_body(callee) == NULL ||
        !semantic_param_publication_formal_for_actual(ctx, call, actual,
            &formal) || formal >= ast_func_param_count(callee))
        return false;
    FuncParam *parameter = ast_func_param(callee, formal);
    if (parameter == NULL || parameter->mode != PARAM_MODE_REF)
        return false;

    CopyCut cut = {.ctx = ctx, .store = ctx->function_param_flow_summaries,
        .mutable_assignments = true};
    CopyCutFrame frame;
    if (cut.store == NULL || cut.store->failed || cut.store->solving ||
        !cut_frame(&cut, callee, formal, &frame))
        return false;
    size_t entry = function_param_flow_find(cut.store,
        ast_node_stable_id(callee), formal);
    unsigned outward = SLOT_PARAM_SUMMARY_NONE;
    const unsigned escapes = SLOT_PARAM_SUMMARY_RETURN_ESCAPE |
        SLOT_PARAM_SUMMARY_CALL_ESCAPE | SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE;
    if (entry == SIZE_MAX ||
        !function_param_flow_entry_outward_effects_current(cut.store, entry,
            &outward) || outward != SLOT_PARAM_SUMMARY_RETURN_ESCAPE ||
        (cut.store->entries[entry].mask &
            ~(SLOT_PARAM_SUMMARY_READ | escapes)) != 0 ||
        cut.store->entries[entry].array_storage_unproved ||
        cut.store->entries[entry].array_buffer_unkept)
        return false;

    FunctionParamFlowResultPath path = {.present = true};
    bool returned = false;
    for (size_t c = 0; c < frame.claims->count; c++) {
        const FunctionParamFlowIndexClaim *row = &frame.claims->rows[c];
        if (row->shape != AST_RETURN) continue;
        if (!row->execution_present || row->node == NULL) return false;
        if (!row->reachable) continue;
        returned = true;
        if (!cut_value(&cut, &frame, ast_return_value(row->node), &path, 0))
            return false;
    }
    if (!returned || cut.copies.count == 0) return false;
    *copies = cut.copies;
    return true;
}
