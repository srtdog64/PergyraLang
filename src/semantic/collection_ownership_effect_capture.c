#include "collection_ownership_fact_internal.h"
#include "collection_ownership_effect_snapshot_internal.h"
#include "function_param_publication_internal.h"
#include "function_param_flow_summary_internal.h"
#include "indexed_string_borrow_owner.h"
#include "indexed_string_field_lineage_internal.h"
#include "indexed_string_nonowner_risk_internal.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>



/* Capture the existing source family, then MOVE its claims and receipts once. */
static bool collection_snapshot_add_caller(PgyCollectionEffectSnapshot *s,
    FunctionParamFlowSummaryStore *flow, const ASTNode *decl,
    FunctionParamFlowIndexClaims *expected, size_t capacity, size_t *out)
{
    if (flow == NULL || flow->failed || flow->solving || decl == NULL || decl->type != AST_FUNC_DECL) return false;
    size_t c = 0;
    for (; c < s->caller_count; c++) if (s->callers[c].decl == decl) break;
    if (c != s->caller_count) {
        if (expected != NULL && s->claims[c] != expected) return false;
        if (out != NULL) *out = c;
        return true;
    }
    FunctionParamFlowProgramPointIndex *index = NULL;
    for (size_t p = 0; p < flow->program_point_index_count; p++)
        if (flow->program_point_indexes[p].function_decl == decl) {
            if (index != NULL) return false;
            index = &flow->program_point_indexes[p];
        }
    if (c >= capacity || index == NULL || index->function_id != ast_node_stable_id(decl) ||
        index->dependency_body != ast_func_body(decl) ||
        index->dependency_body_id != ast_node_stable_id(index->dependency_body) ||
        (expected != NULL && index->claims != expected) ||
        !function_param_flow_syntax_claims_current(NULL, index->claims)) return false;
    size_t independent_results = 0, independent_writes = 0;
    size_t required_results = 0, required_writes = 0;
    if (!function_param_flow_readfile_claim_count(index->claims, &independent_results) ||
        !function_param_flow_readfile_write_count(index->claims, &independent_writes) ||
        !function_param_flow_readfile_execution_counts(index->claims, &required_results, &required_writes) ||
        (independent_results != 0 && !function_param_flow_index_claims_current(flow->ctx, index))) return false;
    s->callers[c] = (PgyCollectionEffectCaller){.decl = decl, .body = index->dependency_body,
        .decl_id = index->function_id, .body_id = index->dependency_body_id,
        .independent_string_result_count = independent_results,
        .independent_string_write_count = independent_writes,
        .independent_string_execution_count = required_results,
        .independent_string_write_execution_count = required_writes};
    s->claims[c] = index->claims;
    s->caller_count++;
    if (out != NULL) *out = c;
    return true;
}

static bool collection_snapshot_add_nominal(PgyCollectionEffectSnapshot *s,
    FunctionParamPublicationStore *pub, const ASTNode *call,
    SemanticNominalResultSnapshot *mapping, size_t capacity, PublishedNode **owners)
{
    PublishedNode *row = publication_node(pub, call, false);
    if (row == NULL || mapping == NULL || row->nominal_result != mapping) return false;
    size_t m = 0;
    for (; m < s->nominal_count; m++)
        if (s->nominals[m] == mapping) return owners[m] == row;
    if (m >= capacity) return false;
    s->nominals[m] = mapping;
    owners[m] = row;
    s->nominal_count++;
    return true;
}

bool semantic_collection_effect_snapshot_take(SemanticContext *ctx,
    PgyCollectionEffectSnapshot **out)
{
    if (out == NULL || *out != NULL || ctx == NULL || ctx->has_error ||
        !semantic_param_publication_ready(ctx)) return false;
    FunctionParamPublicationStore *pub = ctx->function_param_publications;
    size_t count = pub->private_string_push_count, fields = pub->indexed_string_field_count;
    size_t risks = pub->indexed_string_nonowner_count;
    size_t definitions = pub->indexed_string_definition_count, definition_seen = 0;
    size_t permanents = pub->indexed_string_permanent_count, permanent_seen = 0;
    size_t independent_results = 0;
    if (risks > SIZE_MAX - count) return false;
    size_t caller_capacity = count + risks, nominal_capacity = 0, field_seen = 0, risk_seen = 0;
    if (definitions > SIZE_MAX - caller_capacity) return false;
    caller_capacity += definitions;
    for (size_t n = 0; n < pub->node_capacity; n++) {
        const PublishedNode *value = &pub->nodes[n];
        /* Only function-owned indexed-String admission consumes this facet.
         * Intent boundary calls have their own owner, not a function index. */
        if (value->caller != NULL && value->caller->type == AST_FUNC_DECL &&
            semantic_param_publication_checked_readfile_independent(ctx, value->node)) {
            uint32_t source = UINT32_MAX;
            if (independent_results == SIZE_MAX || caller_capacity == SIZE_MAX ||
                !semantic_indexed_string_borrow_source_current(value->node, ctx, &source) ||
                source != 0) return false;
            independent_results++;
            caller_capacity++;
        }
        IndexedStringPermanentElementRecipe *permanent = pub->nodes[n].indexed_string_permanent;
        if (permanent != NULL) {
            size_t dependencies = 0;
            const IndexedStringFieldDependency *deps =
                semantic_indexed_string_permanent_dependencies(permanent, &dependencies);
            if (permanent_seen == SIZE_MAX ||
                !semantic_indexed_string_permanent_current(ctx, permanent) ||
                pub->nodes[n].node == NULL || pub->nodes[n].node->type != AST_CALL ||
                publication_node(pub, pub->nodes[n].node, false) != &pub->nodes[n] ||
                semantic_indexed_string_permanent_for_use(ctx, pub->nodes[n].node, 0,
                    ast_call_argument(pub->nodes[n].node, 0)) != permanent ||
                dependencies == 0 || deps == NULL || dependencies > SIZE_MAX - caller_capacity) return false;
            permanent_seen++;
            caller_capacity += dependencies;
        }
        IndexedStringDefinitionRecipe *definition = pub->nodes[n].indexed_string_definition;
        if (definition != NULL && definition->selected) {
            uint32_t current_source = UINT32_MAX;
            if (definition_seen == SIZE_MAX || pub->nodes[n].node != definition->declaration ||
                pub->nodes[n].id != definition->binding_id ||
                publication_node(pub, definition->declaration, false) != &pub->nodes[n] ||
                !semantic_indexed_string_definition_syntax_current(definition) ||
                !semantic_indexed_string_definition_source_current(ctx,
                    definition->uses[0].value, &current_source) || current_source != 0) return false;
            definition_seen++;
            for (size_t input = 0; input <= definition->assignment_count; input++) {
                const IndexedStringResultInputs *inputs = input == 0 ? &definition->birth.named_inputs :
                    &definition->assignments[input - 1].rhs.named_inputs;
                if (inputs->dependency_count > SIZE_MAX - caller_capacity) return false;
                caller_capacity += inputs->dependency_count;
            }
        }
        IndexedStringNonownerRisk *risk = pub->nodes[n].indexed_string_nonowner;
        if (risk != NULL) {
            if (risk_seen == SIZE_MAX || pub->nodes[n].node != risk->definition ||
                pub->nodes[n].id != risk->binding_id ||
                publication_node(pub, risk->definition, false) != &pub->nodes[n] ||
                !semantic_indexed_string_nonowner_current(ctx, risk) ||
                risk->terminal_count > SIZE_MAX - nominal_capacity) return false;
            risk_seen++;
            nominal_capacity += risk->terminal_count;
        }
        IndexedStringFieldRecipe *r = pub->nodes[n].indexed_string_field;
        if (r == NULL) continue;
        if (field_seen == SIZE_MAX || !semantic_indexed_string_field_current(ctx, r) ||
            r->dependency_count > SIZE_MAX - caller_capacity ||
            r->return_count > SIZE_MAX - nominal_capacity) return false;
        field_seen++;
        caller_capacity += r->dependency_count;
        nominal_capacity += r->return_count;
    }
    if (field_seen != fields || risk_seen != risks || definition_seen != definitions ||
        permanent_seen != permanents ||
        count > SIZE_MAX / sizeof(PgyCollectionExpectedEffect) ||
        caller_capacity > SIZE_MAX / sizeof(PgyCollectionEffectCaller) ||
        caller_capacity > SIZE_MAX / sizeof(FunctionParamFlowIndexClaims *) ||
        fields > SIZE_MAX / sizeof(IndexedStringFieldRecipe *) ||
        nominal_capacity > SIZE_MAX / sizeof(SemanticNominalResultSnapshot *) ||
        nominal_capacity > SIZE_MAX / sizeof(PublishedNode *) ||
        risks > SIZE_MAX / sizeof(IndexedStringNonownerRisk *) ||
        definitions > SIZE_MAX / sizeof(IndexedStringDefinitionRecipe *) ||
        permanents > SIZE_MAX / sizeof(IndexedStringPermanentElementRecipe *) ||
        permanents > SIZE_MAX / sizeof(PublishedNode *)) return false;
    PgyCollectionEffectSnapshot *s = calloc(1, sizeof(*s));
    if (s == NULL) return false;
    s->refs = 1;
    s->program = pub->program;
    s->program_id = pub->program_id;
    s->effects = count == 0 ? NULL : calloc(count, sizeof(*s->effects));
    s->callers = caller_capacity == 0 ? NULL : calloc(caller_capacity, sizeof(*s->callers));
    s->claims = caller_capacity == 0 ? NULL : calloc(caller_capacity, sizeof(*s->claims));
    s->source_uses = fields == 0 ? NULL : calloc(fields, sizeof(*s->source_uses));
    s->nominals = nominal_capacity == 0 ? NULL : calloc(nominal_capacity, sizeof(*s->nominals));
    PublishedNode **nominal_owners = nominal_capacity == 0 ? NULL : calloc(nominal_capacity, sizeof(*nominal_owners));
    s->nonowner_risks = risks == 0 ? NULL : calloc(risks, sizeof(*s->nonowner_risks));
    s->string_definitions = definitions == 0 ? NULL : calloc(definitions, sizeof(*s->string_definitions));
    s->permanent_elements = permanents == 0 ? NULL : calloc(permanents, sizeof(*s->permanent_elements));
    PublishedNode **permanent_owners = permanents == 0 ? NULL : calloc(permanents, sizeof(*permanent_owners));
    if ((count != 0 && s->effects == NULL) ||
        (caller_capacity != 0 && (s->callers == NULL || s->claims == NULL)) ||
        (fields != 0 && s->source_uses == NULL) ||
        (nominal_capacity != 0 && (s->nominals == NULL || nominal_owners == NULL)) ||
        (risks != 0 && s->nonowner_risks == NULL) ||
        (definitions != 0 && s->string_definitions == NULL) ||
        (permanents != 0 && (s->permanent_elements == NULL || permanent_owners == NULL))) goto fail;
    for (size_t n = 0; n < pub->node_capacity; n++) {
        PublishedNode *row = &pub->nodes[n];
        if (row->indexed_string_permanent != NULL) {
            if (s->permanent_element_count >= permanents) goto fail;
            permanent_owners[s->permanent_element_count] = row;
            s->permanent_elements[s->permanent_element_count++] = row->indexed_string_permanent;
        }
        if (row->indexed_string_definition != NULL && row->indexed_string_definition->selected) {
            if (s->string_definition_count >= definitions) goto fail;
            s->string_definitions[s->string_definition_count++] = row->indexed_string_definition;
        }
        if (row->indexed_string_nonowner != NULL) {
            if (s->nonowner_risk_count >= risks) goto fail;
            s->nonowner_risks[s->nonowner_risk_count++] = row->indexed_string_nonowner;
        }
        if (row->private_string_push != NULL) {
            if (s->effect_count >= count) goto fail;
            s->effects[s->effect_count++].call = row->node;
        }
        if (row->indexed_string_field != NULL) {
            if (s->source_use_count >= fields) goto fail;
            s->source_uses[s->source_use_count++] = row->indexed_string_field;
        }
    }
    if (s->effect_count != count || s->source_use_count != fields || s->nonowner_risk_count != risks ||
        s->string_definition_count != definitions || s->permanent_element_count != permanents) goto fail;
    for (size_t i = 0; i < count; i++)
        if (!semantic_indexed_string_private_push_current(ctx, s->effects[i].call)) goto fail;
    if (pub->private_string_push_count != count || pub->indexed_string_field_count != fields ||
        pub->indexed_string_nonowner_count != risks || pub->indexed_string_definition_count != definitions ||
        pub->indexed_string_permanent_count != permanents) goto fail;
    SlotFunctionLookup lookup = {ctx, ctx->program_root};
    FunctionParamFlowSummaryStore *flow = independent_results == 0
        ? ctx->function_param_flow_summaries : function_param_flow_store_get(&lookup);
    for (size_t n = 0; n < pub->node_capacity; n++) {
        const PublishedNode *value = &pub->nodes[n];
        if (value->caller == NULL || value->caller->type != AST_FUNC_DECL ||
            !semantic_param_publication_checked_readfile_independent(ctx, value->node)) continue;
        FunctionParamFlowProgramPointIndex *index = NULL;
        if (!function_param_flow_program_point_index_build(flow, value->caller, &index) ||
            index == NULL || index->function_decl != value->caller) goto fail;
        size_t caller = SIZE_MAX, matched = 0;
        if (!collection_snapshot_add_caller(s, flow, value->caller, NULL,
                caller_capacity, &caller)) goto fail;
        FunctionParamFlowIndexClaims *claims = s->claims[caller];
        for (size_t r = 0; r < claims->count; r++) {
            const FunctionParamFlowIndexClaim *claim = &claims->rows[r];
            if (claim->node != value->node) continue;
            if (++matched != 1 || !claim->readfile_result || claim->id != value->id ||
                !function_param_flow_readfile_claim_current(ctx, claims, claim)) goto fail;
        }
        if (matched != 1 || s->independent_string_result_count == SIZE_MAX) goto fail;
        s->independent_string_result_count++;
    }
    if (s->independent_string_result_count != independent_results) goto fail;
    for (size_t u = 0; u < permanents; u++) {
        size_t dependencies = 0;
        const IndexedStringFieldDependency *deps =
            semantic_indexed_string_permanent_dependencies(s->permanent_elements[u], &dependencies);
        for (size_t d = 0; d < dependencies; d++)
            if (!collection_snapshot_add_caller(s, flow, deps[d].decl, deps[d].claims,
                    caller_capacity, NULL)) goto fail;
    }
    for (size_t u = 0; u < definitions; u++) {
        const IndexedStringDefinitionRecipe *r = s->string_definitions[u];
        if (!collection_snapshot_add_caller(s, flow, r->caller, r->claims, caller_capacity, NULL)) goto fail;
        for (size_t input = 0; input <= r->assignment_count; input++) {
            const IndexedStringResultInputs *inputs = input == 0 ? &r->birth.named_inputs :
                &r->assignments[input - 1].rhs.named_inputs;
            for (size_t d = 0; d < inputs->dependency_count; d++) {
                const IndexedStringFieldDependency *dep = &inputs->dependencies[d];
                if (!collection_snapshot_add_caller(s, flow, dep->decl, dep->claims,
                        caller_capacity, NULL)) goto fail;
            }
        }
    }
    for (size_t u = 0; u < risks; u++) {
        const IndexedStringNonownerRisk *r = s->nonowner_risks[u];
        if (!collection_snapshot_add_caller(s, flow, r->caller, r->claims, caller_capacity, NULL)) goto fail;
        for (size_t t = 0; t < r->terminal_count; t++)
            if (!collection_snapshot_add_nominal(s, pub, r->terminals[t].call,
                    r->terminals[t].mapping, nominal_capacity, nominal_owners)) goto fail;
    }
    for (size_t i = 0; i < count; i++) {
        PgyCollectionExpectedEffect *e = &s->effects[i];
        PublishedNode *row = publication_node(pub, e->call, false);
        if (!collection_snapshot_add_caller(s, flow, row == NULL ? NULL : row->caller,
                NULL, caller_capacity, &e->caller)) goto fail;
        /* The flow claim contributes exact call membership.  The collection
         * ledger below owns the post-landing effect payload. */
        e->id = ast_node_stable_id(e->call);
        if (!ast_call_semantic_collection_effect(e->call, &e->kind, &e->receiver, &e->source)) goto fail;
    }
    for (size_t u = 0; u < fields; u++) {
        IndexedStringFieldRecipe *r = s->source_uses[u];
        PublishedNode *value = publication_node(pub, r->value, false);
        if (value == NULL || value->indexed_string_field != r ||
            memcmp(&value->projection, &r->projection, sizeof(r->projection)) != 0) goto fail;
        for (size_t d = 0; d < r->dependency_count; d++) {
            const IndexedStringFieldDependency *dep = &r->dependencies[d];
            if (!collection_snapshot_add_caller(s, flow, dep->decl, dep->claims,
                    caller_capacity, NULL)) goto fail;
        }
        for (size_t t = 0; t < r->return_count; t++) {
            const IndexedStringFieldReturn *ret = &r->returns[t];
            if (!collection_snapshot_add_nominal(s, pub, ret->constructor, ret->mapping,
                    nominal_capacity, nominal_owners)) goto fail;
        }
    }
    s->sealed_callers = s->caller_count;
    s->sealed_effects = s->effect_count;
    s->sealed_source_uses = s->source_use_count;
    s->sealed_nonowner_risks = s->nonowner_risk_count;
    s->sealed_string_definitions = s->string_definition_count;
    s->sealed_permanent_elements = s->permanent_element_count;
    s->sealed_independent_string_results = s->independent_string_result_count;
    if (!semantic_collection_effect_snapshot_current(s, ctx->program_root, NULL)) goto fail;
    /* All fallible validation/allocation precedes this single ownership handoff. */
    for (size_t c = 0; c < s->caller_count; c++)
        for (size_t p = 0; p < flow->program_point_index_count; p++)
            if (flow->program_point_indexes[p].claims == s->claims[c])
                flow->program_point_indexes[p].claims = NULL;
    for (size_t u = 0; u < risks; u++) {
        PublishedNode *definition = publication_node(pub, s->nonowner_risks[u]->definition, false);
        definition->indexed_string_nonowner = NULL;
    }
    for (size_t u = 0; u < definitions; u++) {
        PublishedNode *definition = publication_node(pub, s->string_definitions[u]->declaration, false);
        definition->indexed_string_definition = NULL;
    }
    for (size_t u = 0; u < permanents; u++) permanent_owners[u]->indexed_string_permanent = NULL;
    for (size_t u = 0; u < fields; u++) {
        IndexedStringFieldRecipe *r = s->source_uses[u];
        PublishedNode *value = publication_node(pub, r->value, false);
        value->indexed_string_field = NULL;
        value->projection = (PublishedProjection){0};
    }
    for (size_t m = 0; m < s->nominal_count; m++) nominal_owners[m]->nominal_result = NULL;
    free(nominal_owners);
    free(permanent_owners);
    s->owns_source_payloads = true;
    *out = s;
    return true;
fail:
    /* Payloads/claims still belong to publication/indexes until successful take. */
    s->caller_count = 0;
    free(nominal_owners);
    free(permanent_owners);
    semantic_collection_effect_snapshot_destroy(s);
    return false;
}
