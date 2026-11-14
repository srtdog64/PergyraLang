#include "collection_ownership_fact_internal.h"
#include "collection_ownership_effect_snapshot_internal.h"
#include "function_param_flow/function_param_publication_internal.h"
#include "function_param_flow/function_param_flow_summary_internal.h"
#include "indexed_string_borrow_owner.h"
#include "indexed_string_field_lineage_internal.h"
#include "indexed_string_nonowner_risk_internal.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>



PgyCollectionEffectSnapshot *semantic_collection_effect_snapshot_retain(
    PgyCollectionEffectSnapshot *snapshot)
{
    if (snapshot == NULL || snapshot->refs == 0 || snapshot->refs == SIZE_MAX) return NULL;
    snapshot->refs++;
    return snapshot;
}

void semantic_collection_effect_snapshot_destroy(PgyCollectionEffectSnapshot *snapshot)
{
    if (snapshot == NULL || snapshot->refs == 0 || --snapshot->refs != 0) return;
    for (size_t i = 0; i < snapshot->caller_count; i++)
        function_param_flow_claims_destroy(snapshot->claims[i]);
    if (snapshot->owns_source_payloads) {
        for (size_t i = 0; i < snapshot->string_definition_count; i++)
            semantic_indexed_string_definition_destroy(snapshot->string_definitions[i]);
        for (size_t i = 0; i < snapshot->permanent_element_count; i++)
            semantic_indexed_string_permanent_destroy(snapshot->permanent_elements[i]);
        for (size_t i = 0; i < snapshot->nonowner_risk_count; i++)
            semantic_indexed_string_nonowner_destroy(snapshot->nonowner_risks[i]);
        for (size_t i = 0; i < snapshot->source_use_count; i++) {
            PublishedNode payload = {.projection = snapshot->source_uses[i]->projection};
            semantic_param_publication_projection_destroy(&payload);
            semantic_indexed_string_field_destroy(snapshot->source_uses[i]);
        }
        for (size_t i = 0; i < snapshot->nominal_count; i++)
            semantic_nominal_result_snapshot_destroy(snapshot->nominals[i]);
    }
    free(snapshot->claims);
    free(snapshot->callers);
    free(snapshot->effects);
    free(snapshot->source_uses);
    free(snapshot->nominals);
    free(snapshot->nonowner_risks);
    free(snapshot->string_definitions);
    free(snapshot->permanent_elements);
    free(snapshot);
}

static bool collection_snapshot_claim_required(const PgyCollectionEffectSnapshot *s,
    size_t caller, uint32_t routine_id)
{
    if (routine_id == 0 || s->callers[caller].decl_id == routine_id) return true;
    for (size_t u = 0; u < s->string_definition_count; u++) {
        const IndexedStringDefinitionRecipe *r = s->string_definitions[u];
        if (r->caller_id != routine_id) continue;
        for (size_t input = 0; input <= r->assignment_count; input++) {
            const IndexedStringResultInputs *inputs = input == 0 ? &r->birth.named_inputs :
                &r->assignments[input - 1].rhs.named_inputs;
            for (size_t d = 0; d < inputs->dependency_count; d++)
                if (inputs->dependencies[d].decl_id == s->callers[caller].decl_id) return true;
        }
    }
    for (size_t u = 0; u < s->source_use_count; u++) {
        const IndexedStringFieldRecipe *r = s->source_uses[u];
        if (r->caller_id != routine_id) continue;
        for (size_t d = 0; d < r->dependency_count; d++)
            if (r->dependencies[d].decl_id == s->callers[caller].decl_id) return true;
    }
    for (size_t u = 0; u < s->permanent_element_count; u++) {
        size_t count = 0;
        const IndexedStringFieldDependency *deps =
            semantic_indexed_string_permanent_dependencies(s->permanent_elements[u], &count);
        bool reached = false;
        for (size_t d = 0; d < count; d++) if (deps[d].decl_id == routine_id) reached = true;
        if (!reached) continue;
        for (size_t d = 0; d < count; d++)
            if (deps[d].decl_id == s->callers[caller].decl_id) return true;
    }
    return false;
}

/* Option inputs borrow these exact snapshot-owned receipts. A matching
 * dependency or a second allocation is not a replacement for that identity. */
static bool collection_snapshot_permanent_inputs_owned(const PgyCollectionEffectSnapshot *s,
    const IndexedStringResultInputs *inputs)
{
    size_t count = 0;
    if (!semantic_indexed_string_result_inputs_permanent_count(inputs, &count)) return false;
    for (size_t r = 0; r < count; r++) {
        const IndexedStringPermanentElementRecipe *receipt =
            semantic_indexed_string_result_inputs_permanent_at(inputs, r);
        size_t matches = 0;
        if (receipt == NULL) return false;
        for (size_t u = 0; u < s->permanent_element_count; u++)
            if (s->permanent_elements[u] == receipt) matches++;
        if (matches != 1) return false;
    }
    return true;
}

static bool collection_effect_snapshot_current_at(const PgyCollectionEffectSnapshot *s,
    const ASTNode *program, uint32_t routine_id, PgyCollectionEffectView *out)
{
    if (out != NULL) *out = (PgyCollectionEffectView){0};
    if (s == NULL || s->refs == 0 || program == NULL || program->type != AST_PROGRAM ||
        s->program != program || s->program_id == 0 ||
        s->program_id != ast_node_stable_id(program) ||
        s->caller_count != s->sealed_callers || s->effect_count != s->sealed_effects ||
        s->source_use_count != s->sealed_source_uses ||
        s->string_definition_count != s->sealed_string_definitions ||
        s->nonowner_risk_count != s->sealed_nonowner_risks ||
        s->permanent_element_count != s->sealed_permanent_elements ||
        s->independent_string_result_count != s->sealed_independent_string_results ||
        (s->caller_count != 0 && (s->callers == NULL || s->claims == NULL)) ||
        (s->effect_count != 0 && s->effects == NULL) ||
        (s->source_use_count != 0 && s->source_uses == NULL) ||
        (s->string_definition_count != 0 && s->string_definitions == NULL) ||
        (s->permanent_element_count != 0 && s->permanent_elements == NULL) ||
        (s->nonowner_risk_count != 0 && s->nonowner_risks == NULL)) return false;
    size_t string_write_count = 0;
    for (size_t u = 0; u < s->string_definition_count; u++) {
        const IndexedStringDefinitionRecipe *r = s->string_definitions[u];
        if (!semantic_indexed_string_definition_syntax_current(r) ||
            r->assignment_count == SIZE_MAX ||
            string_write_count > SIZE_MAX - 1 - r->assignment_count) return false;
        string_write_count += 1 + r->assignment_count;
        for (size_t v = 0; v < u; v++)
            if (s->string_definitions[v] == r || (s->string_definitions[v]->caller_id == r->caller_id &&
                    s->string_definitions[v]->binding_id == r->binding_id)) return false;
        size_t matches = 0;
        for (size_t c = 0; c < s->caller_count; c++)
            if (s->callers[c].decl == r->caller && s->callers[c].decl_id == r->caller_id &&
                s->callers[c].body == r->body && s->callers[c].body_id == r->body_id &&
                s->claims[c] == r->claims) matches++;
        if (matches != 1) return false;
        for (size_t input = 0; input <= r->assignment_count; input++) {
            const IndexedStringResultInputs *inputs = input == 0 ? &r->birth.named_inputs :
                &r->assignments[input - 1].rhs.named_inputs;
            size_t writes = 0;
            if (!collection_snapshot_permanent_inputs_owned(s, inputs) ||
                !semantic_indexed_string_result_inputs_source_write_count(inputs, &writes) ||
                writes > SIZE_MAX - string_write_count) return false;
            string_write_count += writes;
            for (size_t d = 0; d < inputs->dependency_count; d++) {
                const IndexedStringFieldDependency *dep = &inputs->dependencies[d];
                matches = 0;
                for (size_t c = 0; c < s->caller_count; c++)
                    if (s->callers[c].decl == dep->decl && s->callers[c].decl_id == dep->decl_id &&
                        s->callers[c].body == dep->body && s->callers[c].body_id == dep->body_id &&
                        s->claims[c] == dep->claims) matches++;
                if (matches != 1) return false;
            }
        }
    }
    for (size_t u = 0; u < s->permanent_element_count; u++) {
        const IndexedStringPermanentElementRecipe *recipe = s->permanent_elements[u];
        size_t count = 0, writes = 0;
        const IndexedStringFieldDependency *deps =
            semantic_indexed_string_permanent_dependencies(recipe, &count);
        if (!semantic_indexed_string_permanent_syntax_current(recipe) || count == 0 || deps == NULL ||
            !collection_snapshot_permanent_inputs_owned(s,
                semantic_indexed_string_permanent_inputs(recipe)) ||
            !semantic_indexed_string_permanent_source_write_count(recipe, &writes) ||
            writes > SIZE_MAX - string_write_count) return false;
        string_write_count += writes;
        for (size_t v = 0; v < u; v++) if (s->permanent_elements[v] == recipe) return false;
        for (size_t d = 0; d < count; d++) {
            const IndexedStringFieldDependency *dep = &deps[d];
            size_t matches = 0;
            for (size_t c = 0; c < s->caller_count; c++)
                if (s->callers[c].decl == dep->decl && s->callers[c].decl_id == dep->decl_id &&
                    s->callers[c].body == dep->body && s->callers[c].body_id == dep->body_id &&
                    s->claims[c] == dep->claims) matches++;
            if (matches != 1) return false;
        }
    }
    for (size_t u = 0; u < s->nonowner_risk_count; u++) {
        const IndexedStringNonownerRisk *risk = s->nonowner_risks[u];
        if (!semantic_indexed_string_nonowner_syntax_current(risk)) return false;
        for (size_t v = 0; v < u; v++)
            if (s->nonowner_risks[v] == risk || (s->nonowner_risks[v]->caller_id == risk->caller_id &&
                    s->nonowner_risks[v]->binding_id == risk->binding_id)) return false;
        size_t matches = 0;
        for (size_t c = 0; c < s->caller_count; c++)
            if (s->callers[c].decl == risk->caller && s->callers[c].decl_id == risk->caller_id &&
                s->callers[c].body_id == risk->body_id && s->claims[c] == risk->claims) matches++;
        if (matches != 1) return false;
    }
    for (size_t u = 0; u < s->source_use_count; u++) {
        const IndexedStringFieldRecipe *recipe = s->source_uses[u];
        if (!semantic_indexed_string_field_syntax_current(recipe)) return false;
        for (size_t d = 0; d < recipe->dependency_count; d++) {
            const IndexedStringFieldDependency *dep = &recipe->dependencies[d];
            size_t matches = 0;
            for (size_t c = 0; c < s->caller_count; c++)
                if (s->callers[c].decl == dep->decl && s->callers[c].decl_id == dep->decl_id &&
                    s->callers[c].body == dep->body && s->callers[c].body_id == dep->body_id &&
                    s->claims[c] == dep->claims) matches++;
            if (matches != 1) return false;
        }
    }
    /* Identity and sealed totals are global; only reached routines replay claims. */
    size_t independent_results = 0;
    for (size_t i = 0; i < s->caller_count; i++) {
        const PgyCollectionEffectCaller *c = &s->callers[i];
        for (size_t j = 0; j < i; j++)
            if (s->callers[j].decl == c->decl || s->callers[j].decl_id == c->decl_id ||
                s->claims[j] == s->claims[i]) return false;
        if (c->decl == NULL || c->decl->type != AST_FUNC_DECL || c->decl_id == 0 ||
            c->decl_id != ast_node_stable_id(c->decl) || c->body == NULL ||
            c->body != ast_func_body(c->decl) || c->body_id == 0 ||
            c->body_id != ast_node_stable_id(c->body) || s->claims[i] == NULL) return false;
        if (c->independent_string_result_count > SIZE_MAX - independent_results) return false;
        independent_results += c->independent_string_result_count;
        if (c->independent_string_write_count > SIZE_MAX - string_write_count ||
            c->independent_string_execution_count > c->independent_string_result_count ||
            c->independent_string_write_execution_count > c->independent_string_write_count ||
            (c->independent_string_result_count == 0 && c->independent_string_write_count != 0)) return false;
        string_write_count += c->independent_string_write_count;
        if (!collection_snapshot_claim_required(s, i, routine_id)) continue;
        if (!function_param_flow_syntax_claims_current(NULL, s->claims[i])) return false;
        size_t current_independent_results = 0, current_independent_writes = 0;
        size_t current_required_results = 0, current_required_writes = 0;
        if (!function_param_flow_readfile_claim_count(s->claims[i], &current_independent_results) ||
            current_independent_results != c->independent_string_result_count ||
            !function_param_flow_readfile_write_count(s->claims[i], &current_independent_writes) ||
            current_independent_writes != c->independent_string_write_count ||
            !function_param_flow_readfile_execution_counts(s->claims[i],
                &current_required_results, &current_required_writes) ||
            current_required_results != c->independent_string_execution_count ||
            current_required_writes != c->independent_string_write_execution_count) return false;
        bool root = false;
        for (size_t r = 0; r < s->claims[i]->count; r++)
            if (s->claims[i]->rows[r].node == c->body &&
                s->claims[i]->rows[r].id == c->body_id) root = true;
        if (!root) return false;
    }
    if (independent_results != s->independent_string_result_count) return false;
    for (size_t i = 0; i < s->effect_count; i++) {
        const PgyCollectionExpectedEffect *e = &s->effects[i];
        uint32_t kind = 0, receiver = 0, source = 0;
        if (e->caller >= s->caller_count || e->call == NULL || e->call->type != AST_CALL ||
            e->id == 0 || e->id != ast_node_stable_id(e->call) ||
            e->kind != PGY_COLLECTION_EFFECT_SHALLOW_MUTATION || e->receiver == 0 || e->source != 0)
            return false;
        if (routine_id != 0 && s->callers[e->caller].decl_id != routine_id) continue;
        if (!ast_call_semantic_collection_effect(e->call, &kind, &receiver, &source) ||
            kind != e->kind || receiver != e->receiver || source != e->source) return false;
        bool member = false;
        const FunctionParamFlowIndexClaims *claims = s->claims[e->caller];
        for (size_t r = 0; r < claims->count; r++)
            if (claims->rows[r].node == e->call && claims->rows[r].id == e->id) member = true;
        if (!member) return false;
    }
    if (out != NULL) *out = (PgyCollectionEffectView){.callers = s->callers,
        .caller_count = s->caller_count, .effects = s->effects, .effect_count = s->effect_count,
        .source_use_count = s->source_use_count, .nonowner_risk_count = s->nonowner_risk_count,
        .string_definition_count = s->string_definition_count, .string_write_count = string_write_count,
        .permanent_element_count = s->permanent_element_count,
        .independent_string_result_count = s->independent_string_result_count};
    return true;
}

bool semantic_collection_effect_snapshot_current(const PgyCollectionEffectSnapshot *s,
    const ASTNode *program, PgyCollectionEffectView *out)
{
    return collection_effect_snapshot_current_at(s, program, 0, out);
}

bool semantic_collection_effect_snapshot_routine_current(const PgyCollectionEffectSnapshot *s,
    const ASTNode *program, uint32_t routine_id, PgyCollectionEffectView *out)
{
    if (routine_id == 0) {
        if (out != NULL) *out = (PgyCollectionEffectView){0};
        return false;
    }
    return collection_effect_snapshot_current_at(s, program, routine_id, out);
}

/* The executable visitor owns its seen bits. The original claims own which
 * selected static occurrences are required, including direct definitions. */
bool semantic_collection_effect_snapshot_source_execution_complete(
    const PgyCollectionEffectSnapshot *s, uint32_t caller_id,
    const bool *occurrences, size_t occurrence_count, const bool *writes, size_t write_count)
{
    if (s == NULL || s->refs == 0 || caller_id == 0 ||
        s->caller_count != s->sealed_callers ||
        s->independent_string_result_count != s->sealed_independent_string_results ||
        occurrence_count != s->independent_string_result_count ||
        (s->caller_count != 0 && (s->callers == NULL || s->claims == NULL)) ||
        (occurrence_count != 0 && occurrences == NULL) ||
        (write_count != 0 && writes == NULL)) return false;
    size_t all_writes = 0, all_calls = 0;
    for (size_t c = 0; c < s->caller_count; c++) {
        const PgyCollectionEffectCaller *caller = &s->callers[c];
        if (caller->decl == NULL || caller->decl->type != AST_FUNC_DECL ||
            caller->decl_id == 0 || caller->decl_id != ast_node_stable_id(caller->decl) ||
            caller->body == NULL || caller->body != ast_func_body(caller->decl) ||
            caller->body_id == 0 || caller->body_id != ast_node_stable_id(caller->body) ||
            s->claims[c] == NULL ||
            caller->independent_string_result_count > SIZE_MAX - all_calls ||
            caller->independent_string_write_count > SIZE_MAX - all_writes ||
            caller->independent_string_execution_count > caller->independent_string_result_count ||
            caller->independent_string_write_execution_count > caller->independent_string_write_count ||
            (caller->independent_string_result_count == 0 && caller->independent_string_write_count != 0))
            return false;
        all_calls += caller->independent_string_result_count;
        all_writes += caller->independent_string_write_count;
    }
    if (all_calls != occurrence_count || all_writes > write_count) return false;
    size_t call_offset = 0, write_offset = write_count - all_writes;
    bool found = false;
    for (size_t c = 0; c < s->caller_count; c++) {
        const PgyCollectionEffectCaller *caller = &s->callers[c];
        size_t calls = caller->independent_string_result_count;
        size_t direct_writes = caller->independent_string_write_count;
        if (calls > occurrence_count - call_offset || direct_writes > write_count - write_offset)
            return false;
        if (caller->decl_id == caller_id) {
            size_t required_calls = 0, required_writes = 0;
            if (found || s->claims[c] == NULL ||
                !function_param_flow_readfile_execution_counts(s->claims[c],
                    &required_calls, &required_writes) ||
                required_calls != caller->independent_string_execution_count ||
                required_writes != caller->independent_string_write_execution_count)
                return false;
            found = true;
            for (size_t i = 0; i < calls; i++) {
                bool required = false;
                if (!function_param_flow_readfile_occurrence_required(s->claims[c], i, &required) ||
                    occurrences[call_offset + i] != required) return false;
            }
            for (size_t i = 0; i < direct_writes; i++) {
                bool required = false;
                if (!function_param_flow_readfile_write_required(s->claims[c], i, &required) ||
                    writes[write_offset + i] != required) return false;
            }
        }
        call_offset += calls;
        write_offset += direct_writes;
    }
    return call_offset == occurrence_count && write_offset == write_count;
}

bool semantic_collection_effect_snapshot_call(const PgyCollectionEffectSnapshot *s,
    size_t caller, const ASTNode *call, size_t *effect)
{
    if (effect == NULL) return false;
    *effect = SIZE_MAX;
    if (s == NULL || caller >= s->caller_count || s->caller_count != s->sealed_callers ||
        s->effect_count != s->sealed_effects || s->claims == NULL ||
        call == NULL || call->type != AST_CALL || ast_node_stable_id(call) == 0 ||
        (s->effect_count != 0 && s->effects == NULL)) return false;
    const FunctionParamFlowIndexClaims *claims = s->claims[caller];
    if (claims == NULL || claims->failed || claims->rows == NULL) return false;
    const FunctionParamFlowIndexClaim *matched = NULL;
    for (size_t r = 0; r < claims->count; r++) {
        const FunctionParamFlowIndexClaim *row = &claims->rows[r];
        if (row->node != call) continue;
        if (matched != NULL || row->shape != AST_CALL || row->id != ast_node_stable_id(call) ||
            !row->snapshot_complete || row->snapshot_active) return false;
        matched = row;
    }
    if (matched == NULL) return false;
    uint32_t kind = 0, receiver = 0, source = 0;
    bool present = ast_call_semantic_collection_effect(call, &kind, &receiver, &source);
    for (size_t i = 0; i < s->effect_count; i++) {
        const PgyCollectionExpectedEffect *e = &s->effects[i];
        if (e->call != call) continue;
        if (*effect != SIZE_MAX || e->caller != caller || e->id != matched->id ||
            !present || e->kind != kind || e->receiver != receiver || e->source != source) return false;
        *effect = i;
    }
    return true;
}
