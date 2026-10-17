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
    return false;
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
        (s->caller_count != 0 && (s->callers == NULL || s->claims == NULL)) ||
        (s->effect_count != 0 && s->effects == NULL) ||
        (s->source_use_count != 0 && s->source_uses == NULL) ||
        (s->string_definition_count != 0 && s->string_definitions == NULL) ||
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
            if (!semantic_indexed_string_result_inputs_source_write_count(inputs, &writes) ||
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
    /* Identity is global; only the selected routine replays its full claims. */
    for (size_t i = 0; i < s->caller_count; i++) {
        const PgyCollectionEffectCaller *c = &s->callers[i];
        for (size_t j = 0; j < i; j++)
            if (s->callers[j].decl == c->decl || s->callers[j].decl_id == c->decl_id ||
                s->claims[j] == s->claims[i]) return false;
        if (c->decl == NULL || c->decl->type != AST_FUNC_DECL || c->decl_id == 0 ||
            c->decl_id != ast_node_stable_id(c->decl) || c->body == NULL ||
            c->body != ast_func_body(c->decl) || c->body_id == 0 ||
            c->body_id != ast_node_stable_id(c->body) || s->claims[i] == NULL) return false;
        if (!collection_snapshot_claim_required(s, i, routine_id)) continue;
        if (!function_param_flow_syntax_claims_current(NULL, s->claims[i])) return false;
        bool root = false;
        for (size_t r = 0; r < s->claims[i]->count; r++)
            if (s->claims[i]->rows[r].node == c->body &&
                s->claims[i]->rows[r].id == c->body_id) root = true;
        if (!root) return false;
    }
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
    if (out != NULL) *out = (PgyCollectionEffectView){s->callers, s->caller_count,
        s->effects, s->effect_count, s->source_use_count, s->nonowner_risk_count,
        s->string_definition_count, string_write_count};
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
    if (present != matched->collection_effect_present || kind != matched->collection_effect_kind ||
        receiver != matched->collection_effect_receiver || source != matched->collection_effect_source) return false;
    for (size_t i = 0; i < s->effect_count; i++) {
        const PgyCollectionExpectedEffect *e = &s->effects[i];
        if (e->call != call) continue;
        if (*effect != SIZE_MAX || e->caller != caller || e->id != matched->id ||
            !present || e->kind != kind || e->receiver != receiver || e->source != source) return false;
        *effect = i;
    }
    return true;
}

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
    s->callers[c] = (PgyCollectionEffectCaller){decl, index->dependency_body,
        index->function_id, index->dependency_body_id};
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
    if (risks > SIZE_MAX - count) return false;
    size_t caller_capacity = count + risks, nominal_capacity = 0, field_seen = 0, risk_seen = 0;
    if (definitions > SIZE_MAX - caller_capacity) return false;
    caller_capacity += definitions;
    for (size_t n = 0; n < pub->node_capacity; n++) {
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
        count > SIZE_MAX / sizeof(PgyCollectionExpectedEffect) ||
        caller_capacity > SIZE_MAX / sizeof(PgyCollectionEffectCaller) ||
        caller_capacity > SIZE_MAX / sizeof(FunctionParamFlowIndexClaims *) ||
        fields > SIZE_MAX / sizeof(IndexedStringFieldRecipe *) ||
        nominal_capacity > SIZE_MAX / sizeof(SemanticNominalResultSnapshot *) ||
        nominal_capacity > SIZE_MAX / sizeof(PublishedNode *) ||
        risks > SIZE_MAX / sizeof(IndexedStringNonownerRisk *) ||
        definitions > SIZE_MAX / sizeof(IndexedStringDefinitionRecipe *)) return false;
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
    if ((count != 0 && s->effects == NULL) ||
        (caller_capacity != 0 && (s->callers == NULL || s->claims == NULL)) ||
        (fields != 0 && s->source_uses == NULL) ||
        (nominal_capacity != 0 && (s->nominals == NULL || nominal_owners == NULL)) ||
        (risks != 0 && s->nonowner_risks == NULL) ||
        (definitions != 0 && s->string_definitions == NULL)) goto fail;
    for (size_t n = 0; n < pub->node_capacity; n++) {
        PublishedNode *row = &pub->nodes[n];
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
        s->string_definition_count != definitions) goto fail;
    for (size_t i = 0; i < count; i++)
        if (!semantic_indexed_string_private_push_current(ctx, s->effects[i].call)) goto fail;
    if (pub->private_string_push_count != count || pub->indexed_string_field_count != fields ||
        pub->indexed_string_nonowner_count != risks || pub->indexed_string_definition_count != definitions) goto fail;
    FunctionParamFlowSummaryStore *flow = ctx->function_param_flow_summaries;
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
    for (size_t u = 0; u < fields; u++) {
        IndexedStringFieldRecipe *r = s->source_uses[u];
        PublishedNode *value = publication_node(pub, r->value, false);
        value->indexed_string_field = NULL;
        value->projection = (PublishedProjection){0};
    }
    for (size_t m = 0; m < s->nominal_count; m++) nominal_owners[m]->nominal_result = NULL;
    free(nominal_owners);
    s->owns_source_payloads = true;
    *out = s;
    return true;
fail:
    /* Payloads/claims still belong to publication/indexes until successful take. */
    s->caller_count = 0;
    free(nominal_owners);
    semantic_collection_effect_snapshot_destroy(s);
    return false;
}
