#include "indexed_string_result_inputs.h"
#include "function_param_publication_internal.h"
#include "indexed_string_borrow_owner.h"
#include "indexed_string_option_payload.h"
#include "indexed_string_permanent_element.h"
#include "loan_copy_landing_owner.h"
#include "type_checker_internal.h"
#include <stdlib.h>
#include <string.h>

static bool field_id(const ASTNode *node, uint32_t id)
{ return node != NULL && id != 0 && ast_node_stable_id(node) == id; }

static char *field_copy(const char *value)
{
    if (value == NULL) return NULL;
    size_t size = strlen(value) + 1;
    char *copy = malloc(size);
    if (copy != NULL) memcpy(copy, value, size);
    return copy;
}

static bool field_text_same(const char *left, const char *right)
{ return (left == NULL && right == NULL) ||
    (left != NULL && right != NULL && strcmp(left, right) == 0); }

void indexed_string_field_call_destroy(IndexedStringFieldCall *row)
{
    free(row->callee_name);
    free(row->result_syntax);
    for (size_t i = 0; row->formals != NULL && i < row->actual_count; i++) {
        free(row->formals[i].name);
        free(row->formals[i].type_syntax);
        free(row->formals[i].argument_name);
    }
    free(row->formals);
}

bool indexed_string_field_call_syntax(const IndexedStringFieldCall *row)
{
    if (!field_id(row->call, row->call_id) ||
        !field_id(row->target, row->target_id) ||
        row->call->type != AST_CALL || ast_call_callee(row->call) != row->target ||
        row->target->type != AST_IDENTIFIER || row->target_shape != AST_IDENTIFIER ||
        ast_identifier_binding_syntax_id(row->target) != row->target_binding_id ||
        ast_call_semantic_callee_is_stdlib(row->call) != row->stdlib ||
        ast_call_semantic_callee_declared_callable(row->call) != row->declared_callable ||
        ast_call_semantic_callee_value_binding_id(row->call) != 0 ||
        ast_call_arg_count(row->call) != row->actual_count ||
        (row->actual_count != 0 && row->formals == NULL)) return false;
    if (row->builtin) {
        uint32_t kind = 0;
        if (row->callee != NULL || row->callee_id != 0 || row->body != NULL ||
            row->result_type != NULL || row->callee_name != NULL ||
            row->result_syntax != NULL || row->formal_count != 0 ||
            ast_call_semantic_callee_decl_id(row->call) != 0 ||
            !ast_call_semantic_callee_builtin_kind(row->call, &kind) ||
            kind != row->builtin_kind) return false;
    } else if (!field_id(row->callee, row->callee_id) ||
        !field_id(row->body, row->body_id) ||
        row->callee->type != AST_FUNC_DECL || row->callee->is_async_decl ||
        ast_func_body(row->callee) != row->body ||
        ast_func_param_count(row->callee) != row->formal_count ||
        row->formal_count != row->actual_count ||
        ast_func_return_type(row->callee) != row->result_type ||
        ast_node_stable_id(row->result_type) != row->result_type_id ||
        !field_text_same(ast_declaration_name(row->callee), row->callee_name) ||
        !row->declared_callable ||
        ast_call_semantic_callee_decl_id(row->call) != row->callee_id ||
        ast_call_semantic_callee_value_binding_id(row->call) != 0) return false;
    if (!row->builtin && row->result_type != NULL) {
        char *syntax = ast_capture_inline((ASTNode *)row->result_type);
        bool same = syntax != NULL && field_text_same(syntax, row->result_syntax);
        free(syntax);
        if (!same) return false;
    } else if (!row->builtin && row->result_syntax != NULL) return false;
    for (size_t i = 0; i < row->actual_count; i++) {
        const IndexedStringFieldFormal *saved = &row->formals[i];
        if (!field_id(saved->actual, saved->actual_id) ||
            ast_call_argument(row->call, i) != saved->actual ||
            !field_text_same(ast_call_argument_name(row->call, i),
                saved->argument_name)) return false;
        if (row->builtin) continue;
        FuncParam *formal = ast_func_param(row->callee, saved->formal_ordinal);
        if (formal == NULL || saved->formal_ordinal != i ||
            saved->formal_id == 0 || ast_func_param_stable_id(formal) != saved->formal_id ||
            formal->mode != saved->mode || formal->default_value != NULL ||
            formal->type != saved->type_node ||
            ast_node_stable_id(formal->type) != saved->type_id ||
            !field_text_same(formal->name, saved->name)) return false;
        char *syntax = ast_capture_inline(formal->type);
        bool same = syntax != NULL && field_text_same(syntax, saved->type_syntax);
        free(syntax);
        if (!same) return false;
    }
    return true;
}

bool indexed_string_field_call_current(SemanticContext *ctx, const IndexedStringFieldCall *row)
{
    if (!indexed_string_field_call_syntax(row) || !semantic_param_publication_checked_call(ctx, row->call) ||
        semantic_param_publication_direct_callee(ctx, row->call) != row->callee ||
        (!row->builtin && !semantic_param_publication_routine_inputs_current(ctx, row->callee, true))) return false;
    for (size_t a = 0; !row->builtin && a < row->actual_count; a++) {
        size_t formal = SIZE_MAX;
        if (!semantic_param_publication_formal_for_actual(ctx, row->call, a, &formal) ||
            formal != row->formals[a].formal_ordinal) return false;
    }
    return true;
}

bool indexed_string_field_call_capture(SemanticContext *ctx, ASTNode *call, IndexedStringFieldCall *out)
{
    if (out == NULL || call == NULL || call->type != AST_CALL ||
        !semantic_param_publication_checked_call(ctx, call)) return false;
    ASTNode *decl = semantic_param_publication_direct_callee(ctx, call);
    uint32_t kind = 0;
    bool builtin = decl == NULL && ast_call_semantic_callee_builtin_kind(call, &kind);
    ASTNode *target = ast_call_callee(call);
    if ((decl == NULL && !builtin) || target == NULL || target->type != AST_IDENTIFIER ||
        (decl != NULL && (decl->is_async_decl || ast_func_body(decl) == NULL ||
         ast_call_arg_count(call) != ast_func_param_count(decl) ||
         !semantic_param_publication_routine_inputs_current(ctx, decl, true)))) return false;
    IndexedStringFieldCall row = {0};
    row.call = call; row.call_id = ast_node_stable_id(call);
    row.target = target; row.target_id = ast_node_stable_id(target);
    row.target_shape = target->type;
    row.target_binding_id = ast_identifier_binding_syntax_id(target);
    row.stdlib = ast_call_semantic_callee_is_stdlib(call);
    row.declared_callable = ast_call_semantic_callee_declared_callable(call);
    row.callee = decl; row.callee_id = ast_node_stable_id(decl);
    row.body = decl == NULL ? NULL : ast_func_body(decl);
    row.body_id = ast_node_stable_id(row.body);
    row.result_type = decl == NULL ? NULL : ast_func_return_type(decl);
    row.result_type_id = ast_node_stable_id(row.result_type);
    row.actual_count = ast_call_arg_count(call);
    row.formal_count = decl == NULL ? 0 : ast_func_param_count(decl);
    row.builtin = builtin; row.builtin_kind = builtin ? kind : 0;
    row.callee_name = decl == NULL ? NULL : field_copy(ast_declaration_name(decl));
    row.result_syntax = row.result_type == NULL ? NULL :
        ast_capture_inline((ASTNode *)row.result_type);
    if (row.actual_count > SIZE_MAX / sizeof(*row.formals)) goto failed;
    row.formals = row.actual_count == 0 ? NULL : calloc(row.actual_count,
        sizeof(*row.formals));
    if (row.call_id == 0 || row.target_id == 0 ||
        (decl != NULL && (row.callee_id == 0 || row.body_id == 0 ||
            row.callee_name == NULL || (row.result_type != NULL &&
                (row.result_type_id == 0 || row.result_syntax == NULL)))) ||
        (row.actual_count != 0 && row.formals == NULL)) goto failed;
    for (size_t a = 0; a < row.actual_count; a++) {
        IndexedStringFieldFormal *saved = &row.formals[a];
        saved->actual = ast_call_argument(call, a);
        saved->actual_id = ast_node_stable_id(saved->actual);
        saved->argument_name = field_copy(ast_call_argument_name(call, a));
        if (saved->actual_id == 0 || (ast_call_argument_name(call, a) != NULL &&
            saved->argument_name == NULL)) goto failed;
        if (decl == NULL) continue;
        size_t formal_at = SIZE_MAX;
        if (!semantic_param_publication_formal_for_actual(ctx, call, a, &formal_at) ||
            formal_at != a) goto failed;
        FuncParam *formal = ast_func_param(decl, formal_at);
        if (formal == NULL || formal->name == NULL || formal->type == NULL ||
            formal->default_value != NULL) goto failed;
        saved->formal_ordinal = formal_at;
        saved->formal_id = ast_func_param_stable_id(formal);
        saved->mode = formal->mode;
        saved->type_node = formal->type;
        saved->type_id = ast_node_stable_id(formal->type);
        saved->name = field_copy(formal->name);
        saved->type_syntax = ast_capture_inline(formal->type);
        if (saved->formal_id == 0 || saved->type_id == 0 || saved->name == NULL ||
            saved->type_syntax == NULL) goto failed;
    }
    if (!indexed_string_field_call_syntax(&row)) goto failed;
    *out = row;
    return true;
failed:
    indexed_string_field_call_destroy(&row);
    return false;
}

typedef struct StringResultSourceVisit {
    const ASTNode *call;
    const struct StringResultSourceVisit *parent;
} StringResultSourceVisit;

static bool string_result_source_at(SemanticContext *ctx, const ASTNode *call,
    uint32_t *source, const StringResultSourceVisit *parent, bool prepare,
    FunctionParamFlowInputCapture capture, void *user, bool option_result,
    bool project_selected_edges, bool *used_selected_edge)
{
    if (source == NULL || ctx == NULL || ctx->has_error || call == NULL ||
        call->type != AST_CALL || !semantic_param_publication_inputs_ready(ctx))
        return false;
    for (const StringResultSourceVisit *p = parent; p != NULL; p = p->parent)
        if (p->call == call) return false;
    const Type *type = NULL;
    ASTNode *decl = semantic_param_publication_declared_result(ctx, call, &type);
    if (decl == NULL || (option_result ? !type_is_constructed_named(type, "Option") ||
        type_constructed_arg_count(type) != 1 || type_constructed_arg(type, 0) != TYPE_STRING :
        type != TYPE_STRING))
        return false;
    FunctionParamFlowSummaryStore *store = ctx->function_param_flow_summaries;
    if (store == NULL || store->failed || store->solving)
        return false;
    if (capture != NULL && !capture(ctx, decl, NULL, call, user))
        return false;
    StringResultSourceVisit visit = {call, parent};
    uint32_t joined = 0;
    for (size_t p = 0; p < ast_call_arg_count(call); p++) {
        ASTNode *argument = ast_call_argument(call, p);
        SemanticPublishedValue actual = semantic_param_publication_value(ctx, argument);
        size_t formal = 0;
        if (!actual.ok || actual.meta || !semantic_param_publication_checked_argument(ctx, call, p)
            || semantic_param_publication_callee(ctx, call, p) != decl
            || !semantic_param_publication_formal_for_actual(ctx, call, p, &formal))
            return false;
        /* Physical scalars carry no backing storage into a String result.
         * The flow store intentionally has no backing frame for such a
         * formal, so requiring one here turns a proven non-source into a
         * missing-fact failure. */
        if (semantic_param_publication_value_is_physical_scalar(ctx, argument))
            continue;
        if (prepare) (void)function_param_flow_summary_demand(
            &(SlotFunctionLookup){ctx, ctx->program_root}, decl, formal);
        size_t at = function_param_flow_find(store, ast_node_stable_id(decl), formal);
        if (store->failed || at == SIZE_MAX || store->entries[at].function_decl != decl
            || store->entries[at].param_index != formal
            || (store->entries[at].mask & ~(unsigned)SLOT_PARAM_SUMMARY_ALL) != 0
            || store->entries[at].state != FUNCTION_PARAM_FLOW_COMPLETE
            || store->entries[at].analysis_unsupported || store->entries[at].returned_backing_unproved
            || store->entries[at].returned_backing_pending
            || !function_param_flow_output_current(store, at, false))
            return false;
        unsigned outward_effects = SLOT_PARAM_SUMMARY_NONE;
        if (!function_param_flow_entry_outward_effects_current(store, at,
                &outward_effects))
            return false;
        if ((outward_effects & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) == 0)
            continue;
        if (actual.type != TYPE_STRING)
            return false;
        const ASTNode *dependency = NULL;
        if (argument->type == AST_CALL && ast_call_semantic_callee_declared_callable(argument)
            && ast_call_semantic_callee_decl_id(argument) != 0) dependency = argument;
        else if (argument->type == AST_IDENTIFIER
            && !semantic_indexed_string_borrow_result_dependency_current(ctx, argument, &dependency))
            return false;
        uint32_t incoming;
        if (project_selected_edges &&
            semantic_loan_copy_landing_selected(ctx, argument) &&
            semantic_loan_copy_landing_carrier_copies(ctx, argument)) {
            /* This is the exact argument edge that the next parse wraps in
             * Clone.  Project only that edge as source zero; the original
             * node remains nonzero everywhere else in this analysis. */
            incoming = 0;
            if (used_selected_edge != NULL) *used_selected_edge = true;
        } else if (dependency != NULL ? !string_result_source_at(ctx,
                dependency, &incoming, &visit, prepare, capture, user, false,
                project_selected_edges, used_selected_edge)
            : !semantic_indexed_string_borrow_source_current(argument, ctx,
                &incoming)) return false;
        if (capture != NULL && dependency == NULL && argument->type == AST_CALL &&
            !capture(ctx, decl, NULL, argument, user))
            return false;
        if (incoming == UINT32_MAX || (joined != 0 && incoming != 0 && joined != incoming))
            return false;
        if (joined == 0) joined = incoming;
    }
    if (function_param_flow_declared_result_frame_current(
            store, (ASTNode *)call, NULL) == NULL)
        return false;
    if (!function_param_flow_declared_result_rows_capture(
            store, decl, capture, user))
        return false;
    *source = joined;
    return true;
}

bool function_param_flow_string_result_source_current(SemanticContext *ctx,
    const ASTNode *call, uint32_t *source)
{
    bool previous;
    if (!function_param_flow_current_begin(ctx, &previous)) return false;
    bool current = string_result_source_at(ctx, call, source, NULL, false,
        NULL, NULL, false, false, NULL);
    function_param_flow_current_end(ctx, previous);
    return current;
}

bool function_param_flow_string_result_source_prepare(SemanticContext *ctx,
    const ASTNode *call, uint32_t *source)
{
    if (ctx == NULL || ctx->has_error || (ctx->function_param_flow_summaries != NULL &&
        ctx->function_param_flow_summaries->validating_current)) return false;
    return function_param_flow_store_get(&(SlotFunctionLookup){ctx, ctx->program_root}) != NULL &&
        string_result_source_at(ctx, call, source, NULL, true, NULL, NULL,
            false, false, NULL);
}

bool function_param_flow_string_result_source_after_selected_edges(
    SemanticContext *ctx, const ASTNode *call, uint32_t *source)
{
    bool used_selected_edge = false;
    if (source == NULL || ctx == NULL || ctx->has_error ||
        (ctx->function_param_flow_summaries != NULL &&
         ctx->function_param_flow_summaries->validating_current) ||
        function_param_flow_store_get(
            &(SlotFunctionLookup){ctx, ctx->program_root}) == NULL)
        return false;
    const Type *result_type = NULL;
    if (semantic_loan_copy_landing_selected(ctx, call)
        && semantic_loan_copy_landing_carrier_copies(ctx, call)
        && semantic_param_publication_declared_result(
            ctx, call, &result_type) != NULL
        && type_equals(result_type, TYPE_STRING)) {
        /* The landing wraps this exact call result, rather than one of its
         * arguments, in a whole-value Clone.  Project that selected result
         * edge as source zero; replay still owns the actual proof. */
        *source = 0;
        return true;
    }
    bool current = string_result_source_at(ctx, call, source, NULL, true,
        NULL, NULL, false, true, &used_selected_edge);
    return current && used_selected_edge;
}

typedef struct {
    IndexedStringResultInputs *inputs;
    bool prepare;
} StringResultInputCapture;

static bool result_input_capture(SemanticContext *ctx, ASTNode *decl,
    FunctionParamFlowIndexClaims *claims, const ASTNode *call, void *user)
{
    StringResultInputCapture *capture = user;
    IndexedStringResultInputs *inputs = capture->inputs;
    if (call != NULL) {
        /* Nominal constructors already belong to the typed claims dependency
         * for this routine.  They are not callable-function edges and must
         * not be re-encoded as IndexedStringFieldCall rows.  Require the
         * exact captured claims row so this is a delegation to its owner, not
         * an unchecked constructor exemption. */
        if (semantic_param_publication_nominal_result_complete(ctx, call)) {
            for (size_t d = 0; d < inputs->dependency_count; d++) {
                const IndexedStringFieldDependency *saved =
                    &inputs->dependencies[d];
                if (saved->decl != decl || saved->decl_id == 0 ||
                    saved->decl_id != ast_node_stable_id(decl) ||
                    saved->claims == NULL)
                    continue;
                for (size_t r = 0; r < saved->claims->count; r++) {
                    const FunctionParamFlowIndexClaim *claim =
                        &saved->claims->rows[r];
                    if (claim->node == call &&
                        claim->id == ast_node_stable_id(call) &&
                        claim->shape == AST_CALL &&
                        claim->semantic_promoted &&
                        claim->snapshot_semantic_complete &&
                        claim->value_ready)
                        return true;
                }
            }
            return false;
        }
        ASTNode *selected = semantic_param_publication_direct_callee(ctx, call);
        if (ast_call_generic_arg_count(call) != 0 || (selected != NULL &&
            ast_generic_param_count(ast_func_generic_params(selected)) != 0)) {
            return false;
        }
        for (size_t c = 0; c < inputs->call_count; c++) {
            IndexedStringFieldCall *saved = &inputs->calls[c];
            if (saved->call != call) continue;
            return indexed_string_field_call_current(ctx, saved);
        }
        if (!capture->prepare || inputs->call_count == SIZE_MAX / sizeof(*inputs->calls)) {
            return false;
        }
        IndexedStringFieldCall row = {0};
        if (!indexed_string_field_call_capture(ctx, (ASTNode *)call, &row)) {
            return false;
        }
        size_t count = inputs->call_count + 1;
        IndexedStringFieldCall *grown = realloc(inputs->calls, count * sizeof(*grown));
        if (grown == NULL) { indexed_string_field_call_destroy(&row); return false; }
        inputs->calls = grown;
        inputs->calls[inputs->call_count++] = row;
        inputs->call_capacity = count;
        return true;
    }
    if (decl == NULL || decl->type != AST_FUNC_DECL || claims == NULL ||
        ctx->function_param_flow_summaries == NULL) return false;
    FunctionParamFlowSummaryStore *store = ctx->function_param_flow_summaries;
    FunctionParamFlowProgramPointIndex *index = NULL;
    for (size_t i = 0; i < store->program_point_index_count; i++)
        if (store->program_point_indexes[i].function_decl == decl) {
            if (index != NULL) return false;
            index = &store->program_point_indexes[i];
        }
    if (index == NULL || index->claims != claims || !function_param_flow_index_claims_current(ctx, index)) return false;
    bool found = false;
    for (size_t d = 0; d < inputs->dependency_count; d++) {
        const IndexedStringFieldDependency *saved = &inputs->dependencies[d];
        if (saved->decl != decl) continue;
        if (!(saved->decl_id == ast_node_stable_id(decl) && saved->body == ast_func_body(decl) &&
            saved->body_id == ast_node_stable_id(saved->body) && saved->claims == claims &&
            saved->no_store_call_count == index->no_store_call_count)) return false;
        found = true;
        break;
    }
    if (!found) {
        if (!capture->prepare || inputs->dependency_count == SIZE_MAX / sizeof(*inputs->dependencies)) return false;
        size_t count = inputs->dependency_count + 1;
        IndexedStringFieldDependency *grown = realloc(inputs->dependencies, count * sizeof(*grown));
        if (grown == NULL) return false;
        inputs->dependencies = grown;
        inputs->dependencies[inputs->dependency_count++] = (IndexedStringFieldDependency){
            decl, ast_func_body(decl), ast_node_stable_id(decl), ast_node_stable_id(ast_func_body(decl)), claims,
            index->no_store_call_count};
        inputs->dependency_capacity = count;
    }
    /* The dependency row owns the exact claims identity and its current
     * typed projection.  Calls are captured by the result-frame and no-store
     * traversals when those closures actually reach them.  Re-enumerating
     * every call in the routine here would make unrelated constructors and
     * dead branches premises of one String result. */
    return true;
}

bool indexed_string_result_derive(SemanticContext *ctx, const ASTNode *call,
    uint32_t *source, bool prepare, bool option_result, IndexedStringResultInputs *inputs)
{
    StringResultInputCapture capture = {inputs, prepare};
    return string_result_source_at(ctx, call, source, NULL, prepare,
        result_input_capture, &capture, option_result, false, NULL);
}

bool indexed_string_result_capture_dependency(SemanticContext *ctx, ASTNode *decl,
    FunctionParamFlowIndexClaims *claims, IndexedStringResultInputs *inputs, bool prepare)
{
    StringResultInputCapture capture = {inputs, prepare};
    return result_input_capture(ctx, decl, claims, NULL, &capture);
}

bool indexed_string_result_capture_call(SemanticContext *ctx, const ASTNode *call,
    IndexedStringResultInputs *inputs, bool prepare)
{
    StringResultInputCapture capture = {inputs, prepare};
    return result_input_capture(ctx, NULL, NULL, call, &capture);
}

bool indexed_string_result_capture_permanent(SemanticContext *ctx,
    const IndexedStringPermanentElementRecipe *recipe, IndexedStringResultInputs *inputs, bool prepare)
{
    if (!semantic_indexed_string_permanent_current(ctx, recipe)) return false;
    size_t count = 0;
    const IndexedStringFieldDependency *dependencies = semantic_indexed_string_permanent_dependencies(recipe, &count);
    if (dependencies == NULL || count == 0) return false;
    for (size_t d = 0; d < count; d++) if (!indexed_string_result_capture_dependency(ctx,
        (ASTNode *)dependencies[d].decl, dependencies[d].claims, inputs, prepare)) return false;
    for (size_t p = 0; p < inputs->permanent_count; p++) if (inputs->permanents[p] == recipe) return true;
    if (!prepare || inputs->permanent_count >= SIZE_MAX / sizeof(*inputs->permanents)) return false;
    const IndexedStringPermanentElementRecipe **grown = realloc(inputs->permanents,
        (inputs->permanent_count + 1) * sizeof(*grown));
    if (grown == NULL) return false;
    inputs->permanents = grown; grown[inputs->permanent_count++] = recipe;
    inputs->permanent_capacity = inputs->permanent_count;
    return true;
}

bool semantic_indexed_string_result_inputs_syntax_current(const IndexedStringResultInputs *inputs)
{
    if (inputs == NULL || !inputs->prepared || inputs->call_count != inputs->sealed_calls ||
        inputs->dependency_count != inputs->sealed_dependencies || inputs->call_count > inputs->call_capacity ||
        inputs->dependency_count > inputs->dependency_capacity ||
        (inputs->call_count != 0 && inputs->calls == NULL) ||
        (inputs->dependency_count != 0 && inputs->dependencies == NULL) ||
        inputs->permanent_count != inputs->sealed_permanents || inputs->permanent_count > inputs->permanent_capacity ||
        (inputs->permanent_count != 0 && inputs->permanents == NULL)) return false;
    for (size_t p = 0; p < inputs->permanent_count; p++) {
        if (!semantic_indexed_string_permanent_syntax_current(inputs->permanents[p])) return false;
        for (size_t q = 0; q < p; q++) if (inputs->permanents[q] == inputs->permanents[p]) return false;
    }
    for (size_t c = 0; c < inputs->call_count; c++) {
        const IndexedStringFieldCall *row = &inputs->calls[c];
        if (!indexed_string_field_call_syntax(row) || ast_call_generic_arg_count(row->call) != 0 ||
            (row->callee != NULL && ast_generic_param_count(ast_func_generic_params(row->callee)) != 0)) return false;
        for (size_t p = 0; p < c; p++) if (inputs->calls[p].call == inputs->calls[c].call) return false;
    }
    for (size_t d = 0; d < inputs->dependency_count; d++) {
        const IndexedStringFieldDependency *row = &inputs->dependencies[d];
        if (!field_id(row->decl, row->decl_id) || !field_id(row->body, row->body_id) ||
            row->decl->type != AST_FUNC_DECL || ast_func_body(row->decl) != row->body ||
            !function_param_flow_syntax_claims_current(NULL, row->claims)) return false;
        bool body = false;
        for (size_t r = 0; r < row->claims->count; r++)
            if (row->claims->rows[r].node == row->body && row->claims->rows[r].id == row->body_id) body = true;
        if (!body) return false;
        for (size_t p = 0; p < d; p++) if (inputs->dependencies[p].decl == row->decl ||
            inputs->dependencies[p].decl_id == row->decl_id || inputs->dependencies[p].claims == row->claims) return false;
    }
    return inputs->call_count != 0 && inputs->dependency_count != 0 &&
        (inputs->option == NULL || indexed_string_option_syntax(inputs));
}

void semantic_indexed_string_result_inputs_destroy(IndexedStringResultInputs *inputs)
{
    if (inputs == NULL) return;
    for (size_t c = 0; c < inputs->call_count; c++) indexed_string_field_call_destroy(&inputs->calls[c]);
    free(inputs->calls);
    free(inputs->dependencies);
    free(inputs->permanents);
    indexed_string_option_destroy(inputs->option);
    *inputs = (IndexedStringResultInputs){0};
}

const IndexedStringFieldDependency *semantic_indexed_string_result_inputs_dependencies(
    const IndexedStringResultInputs *inputs, size_t *count)
{
    if (count == NULL) return NULL;
    *count = 0;
    if (!semantic_indexed_string_result_inputs_syntax_current(inputs)) return NULL;
    *count = inputs->dependency_count;
    return inputs->dependencies;
}

bool semantic_indexed_string_result_inputs_permanent_count(const IndexedStringResultInputs *inputs, size_t *count)
{
    if (count == NULL) return false;
    *count = 0;
    if (inputs == NULL) return true;
    if (!inputs->prepared) return inputs->calls == NULL && inputs->dependencies == NULL && inputs->option == NULL &&
        inputs->call_count == 0 && inputs->call_capacity == 0 && inputs->sealed_calls == 0 &&
        inputs->dependency_count == 0 && inputs->dependency_capacity == 0 && inputs->sealed_dependencies == 0 &&
        inputs->permanent_count == 0 && inputs->permanent_capacity == 0 && inputs->sealed_permanents == 0 && inputs->permanents == NULL;
    if (!semantic_indexed_string_result_inputs_syntax_current(inputs)) return false;
    *count = inputs->permanent_count;
    return true;
}

const IndexedStringPermanentElementRecipe *semantic_indexed_string_result_inputs_permanent_at(
    const IndexedStringResultInputs *inputs, size_t ordinal)
{
    return semantic_indexed_string_result_inputs_syntax_current(inputs) && ordinal < inputs->permanent_count ?
        inputs->permanents[ordinal] : NULL;
}

bool semantic_indexed_string_result_source_prepare(SemanticContext *ctx,
    const ASTNode *call, uint32_t *source, IndexedStringResultInputs *inputs)
{
    if (inputs == NULL) return function_param_flow_string_result_source_prepare(ctx, call, source);
    if (inputs->prepared || inputs->call_count != 0 || inputs->dependency_count != 0 ||
        inputs->calls != NULL || inputs->dependencies != NULL || inputs->call_capacity != 0 ||
        inputs->dependency_capacity != 0 || inputs->sealed_calls != 0 || inputs->sealed_dependencies != 0 ||
        inputs->option != NULL || inputs->permanents != NULL || inputs->permanent_count != 0 ||
        inputs->permanent_capacity != 0 || inputs->sealed_permanents != 0 ||
        ctx == NULL || ctx->has_error || (ctx->function_param_flow_summaries != NULL &&
        ctx->function_param_flow_summaries->validating_current)) return false;
    if (function_param_flow_store_get(&(SlotFunctionLookup){ctx, ctx->program_root}) == NULL ||
        !(semantic_indexed_string_option_unwrap_candidate(ctx, call) ?
          indexed_string_option_source(ctx, call, source, inputs, true) :
          indexed_string_result_derive(ctx, call, source, true, false, inputs))) return false;
    inputs->sealed_calls = inputs->call_count;
    inputs->sealed_dependencies = inputs->dependency_count;
    inputs->sealed_permanents = inputs->permanent_count;
    inputs->prepared = true;
    return semantic_indexed_string_result_inputs_syntax_current(inputs);
}

bool semantic_indexed_string_result_source_current(SemanticContext *ctx,
    const ASTNode *call, uint32_t *source, IndexedStringResultInputs *inputs)
{
    if (inputs == NULL) return function_param_flow_string_result_source_current(ctx, call, source);
    if (!semantic_indexed_string_result_inputs_syntax_current(inputs)) return false;
    bool previous;
    if (!function_param_flow_current_begin(ctx, &previous)) return false;
    bool current = inputs->option != NULL ? indexed_string_option_source(ctx, call, source, inputs, false) :
        indexed_string_result_derive(ctx, call, source, false, false, inputs);
    function_param_flow_current_end(ctx, previous);
    return current;
}
