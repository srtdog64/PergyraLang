#include "collection_ownership_effect_snapshot_internal.h"
#include "../parser/ast_api.h"
#include <stdint.h>

/* Pure intrinsic results are reusable. Their original role edges, however,
 * cannot be replaced by another original node or a foreign caller. */
static bool collection_independent_role(const PgyCollectionEffectSnapshot *s,
    uint32_t caller, const ASTNode *parent, const ASTNode *value, size_t ordinal,
    ASTNodeType expected_parent, bool *matched)
{
    if (matched != NULL) *matched = false;
    if (s == NULL || s->independent_string_result_count != s->sealed_independent_string_results) return false;
    if (s->independent_string_result_count == 0) return true;
    bool sensitive_value = false, sensitive_parent = false;
    const FunctionParamFlowIndexClaims *owner = NULL;
    for (size_t c = 0; c < s->caller_count; c++) {
        if (s->callers[c].independent_string_result_count == 0) continue;
        const FunctionParamFlowIndexClaims *claims = s->claims[c];
        if (claims == NULL || claims->failed || claims->rows == NULL) return false;
        size_t next = claims->readfile_first_plus_one, visited = 0;
        while (next != 0) {
            if (next > claims->count || visited++ >= s->callers[c].independent_string_result_count) return false;
            const FunctionParamFlowIndexClaim *row = &claims->rows[next - 1];
            next = row->readfile_next_plus_one;
            if (!row->readfile_result) return false;
            bool source = value != NULL && (row->node == value || row->id == ast_node_stable_id(value));
            bool sink = parent != NULL && (row->node == parent || row->id == ast_node_stable_id(parent));
            if (!source && !sink) continue;
            if (s->callers[c].decl_id != caller ||
                !function_param_flow_readfile_claim_current(NULL, claims, row) ||
                (source && row->node != value) || (sink && row->node != parent) ||
                (owner != NULL && owner != claims)) return false;
            owner = claims;
            sensitive_value |= source;
            sensitive_parent |= sink;
        }
        if (visited != s->callers[c].independent_string_result_count) return false;
    }
    /* Also protect the reverse edge: an original role containing the issued
     * result cannot silently receive another actual value. */
    for (size_t c = 0; parent != NULL && c < s->caller_count; c++) {
        if (s->callers[c].decl_id != caller || s->callers[c].independent_string_result_count == 0) continue;
        const FunctionParamFlowIndexClaims *claims = s->claims[c];
        for (size_t r = 0; r < claims->count; r++) {
            const FunctionParamFlowIndexClaim *row = &claims->rows[r];
            if (row->node != parent && row->id != ast_node_stable_id(parent)) continue;
            if (ordinal >= row->child_count || row->child_claim_ordinals == NULL) continue;
            size_t child = row->child_claim_ordinals[ordinal];
            if (child >= claims->count || !claims->rows[child].readfile_result) continue;
            if (row->node != parent || s->callers[c].decl_id != caller ||
                (owner != NULL && owner != claims)) return false;
            owner = claims;
            sensitive_parent = true;
        }
    }
    if (!sensitive_value && !sensitive_parent) return true;
    if (matched != NULL) *matched = true;
    if (parent == NULL) return true; /* Value identity only; role checked by its caller. */
    if (owner == NULL || value == NULL || (expected_parent != 0 && parent->type != expected_parent)) return false;
    for (size_t r = 0; r < owner->count; r++) {
        const FunctionParamFlowIndexClaim *row = &owner->rows[r];
        if (row->node != parent || row->id != ast_node_stable_id(parent) || row->shape != parent->type ||
            ordinal >= row->child_count || row->children == NULL || row->child_ids == NULL ||
            row->child_claim_ordinals == NULL) continue;
        size_t child = row->child_claim_ordinals[ordinal];
        return child < owner->count && row->children[ordinal] == value &&
            row->child_ids[ordinal] == ast_node_stable_id(value) &&
            owner->rows[child].node == value && owner->rows[child].id == ast_node_stable_id(value);
    }
    return false;
}

/* A selected result's value may be reused, but an executable return still
 * belongs to its original statement even when the value wraps that result. */
static bool collection_independent_return_current(const PgyCollectionEffectSnapshot *s,
    uint32_t caller_id, const ASTNode *statement, const ASTNode *value)
{
    if (value == NULL) return true;
    bool selected_caller = false;
    for (size_t c = 0; c < s->caller_count; c++) {
        if (s->callers[c].decl_id != caller_id ||
            s->callers[c].independent_string_result_count == 0) continue;
        if (selected_caller || statement == NULL || statement->type != AST_RETURN ||
            ast_node_stable_id(statement) == 0 || ast_node_stable_id(value) == 0 ||
            ast_return_value(statement) != value) return false;
        selected_caller = true;
        const FunctionParamFlowIndexClaims *claims = s->claims[c];
        if (claims == NULL || claims->failed || claims->rows == NULL) return false;
        bool found = false;
        for (size_t r = 0; r < claims->count; r++) {
            const FunctionParamFlowIndexClaim *row = &claims->rows[r];
            if (row->node != statement && row->id != ast_node_stable_id(statement)) continue;
            if (found || row->node != statement || row->id != ast_node_stable_id(statement) ||
                row->shape != AST_RETURN || !row->snapshot_complete || row->snapshot_active ||
                row->child_count != 1 || row->children == NULL || row->child_ids == NULL ||
                row->child_claim_ordinals == NULL || row->children[0] != value ||
                row->child_ids[0] != ast_node_stable_id(value) ||
                row->child_claim_ordinals[0] >= claims->count) return false;
            const FunctionParamFlowIndexClaim *child = &claims->rows[row->child_claim_ordinals[0]];
            if (child->node != value || child->id != row->child_ids[0] ||
                child->shape != value->type || !child->snapshot_complete || child->snapshot_active)
                return false;
            found = true;
        }
        if (!found) return false;
    }
    return true;
}

bool semantic_collection_effect_snapshot_source_occurrence(const PgyCollectionEffectSnapshot *s,
    uint32_t caller_id, const ASTNode *value, const ASTNode *execution_root, size_t *ordinal)
{
    if (ordinal == NULL) return false;
    *ordinal = SIZE_MAX;
    if (s == NULL || s->refs == 0 ||
        s->independent_string_result_count != s->sealed_independent_string_results ||
        s->caller_count != s->sealed_callers ||
        (s->caller_count != 0 && (s->callers == NULL || s->claims == NULL))) return false;
    size_t prefix = 0, selected = SIZE_MAX;
    for (size_t c = 0; c < s->caller_count; c++) {
        size_t count = 0, local = SIZE_MAX;
        if (!function_param_flow_readfile_claim_count(s->claims[c], &count) ||
            count != s->callers[c].independent_string_result_count ||
            count > SIZE_MAX - prefix ||
            !function_param_flow_readfile_occurrence_ordinal(s->claims[c], value, execution_root, &local)) return false;
        if (local != SIZE_MAX) {
            if (caller_id == 0 || s->callers[c].decl_id != caller_id ||
                selected != SIZE_MAX || local >= count) return false;
            selected = prefix + local;
        }
        prefix += count;
    }
    if (prefix != s->independent_string_result_count) return false;
    *ordinal = selected;
    return true;
}

/* The boundary already replayed shared claims. Per-use queries only consume
 * their membership and immutable admission edges, not another whole body. */
bool semantic_collection_effect_snapshot_source_root(const PgyCollectionEffectSnapshot *s,
    uint32_t caller_id, const ASTNode *root)
{
    if (s == NULL || s->refs == 0 || s->source_use_count != s->sealed_source_uses ||
        s->string_definition_count != s->sealed_string_definitions ||
        s->nonowner_risk_count != s->sealed_nonowner_risks ||
        s->permanent_element_count != s->sealed_permanent_elements ||
        s->independent_string_result_count != s->sealed_independent_string_results) return false;
    if ((s->source_use_count == 0 && s->nonowner_risk_count == 0 &&
            s->string_definition_count == 0 && s->permanent_element_count == 0 &&
            s->independent_string_result_count == 0) || root == NULL) return true;
    if (!collection_independent_role(s, caller_id, NULL, root, 0, 0, NULL)) return false;
    if (caller_id == 0 || ast_node_stable_id(root) == 0) return false;
    for (size_t p = 0; p < s->permanent_element_count; p++)
        if (!semantic_indexed_string_permanent_source_root(s->permanent_elements[p], caller_id, root)) return false;
    for (size_t c = 0; c < s->caller_count; c++) {
        if (s->callers[c].decl_id != caller_id) continue;
        const FunctionParamFlowIndexClaims *claims = s->claims[c];
        if (claims == NULL || claims->failed || claims->rows == NULL) return false;
        for (size_t r = 0; r < claims->count; r++) {
            const FunctionParamFlowIndexClaim *row = &claims->rows[r];
            if (row->node == root) return row->id == ast_node_stable_id(root) &&
                row->shape == root->type && row->snapshot_complete && !row->snapshot_active;
        }
        return false;
    }
    return true; /* No field permission: the visitor still checks global edges. */
}

static bool collection_source_place_binding(const ASTNode *place, uint32_t *binding);

bool semantic_collection_effect_snapshot_source_use(const PgyCollectionEffectSnapshot *s,
    uint32_t caller_id, const ASTNode *sink, size_t ordinal, const ASTNode *value)
{
    if (ordinal == SIZE_MAX || !collection_independent_role(s, caller_id, sink, value, ordinal + 1, AST_CALL, NULL)) return false;
    if (s == NULL || s->refs == 0 || s->source_use_count != s->sealed_source_uses ||
        s->string_definition_count != s->sealed_string_definitions ||
        s->nonowner_risk_count != s->sealed_nonowner_risks ||
        s->permanent_element_count != s->sealed_permanent_elements ||
        s->independent_string_result_count != s->sealed_independent_string_results ||
        sink == NULL || sink->type != AST_CALL || value == NULL) return false;
    for (size_t p = 0; p < s->permanent_element_count; p++)
        if (!semantic_indexed_string_permanent_source_use(s->permanent_elements[p],
                caller_id, sink, ordinal, value)) return false;
    uint32_t binding = 0;
    if (!collection_source_place_binding(value, &binding)) return false;
    for (size_t n = 0; n < s->string_definition_count; n++) {
        const IndexedStringDefinitionRecipe *r = s->string_definitions[n];
        if (r == NULL || r->use_count != r->sealed_uses || r->use_count > r->use_capacity ||
            (r->use_count != 0 && r->uses == NULL) || r->assignment_count == SIZE_MAX ||
            r->assignment_count != r->sealed_assignments ||
            r->assignment_count > r->assignment_capacity ||
            (r->assignment_count != 0 && r->assignments == NULL)) return false;
        for (size_t input = 0; input <= r->assignment_count; input++) {
            const IndexedStringResultInputs *inputs = input == 0 ? &r->birth.named_inputs :
                &r->assignments[input - 1].rhs.named_inputs;
            if (!semantic_indexed_string_result_inputs_source_use(inputs,
                    caller_id, sink, ordinal, value)) return false;
        }
        size_t matched = 0;
        bool slot_seen = false, value_seen = false;
        for (size_t u = 0; u < r->use_count; u++) {
            const IndexedStringDefinitionUse *use = &r->uses[u];
            bool slot = (use->sink == sink || use->sink_id == ast_node_stable_id(sink)) &&
                use->actual_ordinal == ordinal;
            bool source = use->value == value || use->value_id == ast_node_stable_id(value);
            slot_seen |= slot;
            value_seen |= source;
            if (!slot || !source) continue;
            if (++matched != 1 || caller_id != r->caller_id || use->sink != sink ||
                use->sink_id != ast_node_stable_id(sink) || use->value != value ||
                use->value_id != ast_node_stable_id(value) || use->value_shape != value->type ||
                use->binding_id != binding || binding != r->binding_id ||
                ordinal >= ast_call_arg_count(sink) || ast_call_argument(sink, ordinal) != value) return false;
        }
        if ((slot_seen || value_seen || binding == r->binding_id) &&
            (!slot_seen || !value_seen || matched != 1)) return false;
    }
    for (size_t n = 0; n < s->nonowner_risk_count; n++) {
        const IndexedStringNonownerRisk *risk = s->nonowner_risks[n];
        if (risk == NULL || risk->use_count != risk->sealed_uses || risk->use_count > risk->use_capacity ||
            (risk->use_count != 0 && risk->uses == NULL)) return false;
        if (binding == risk->binding_id) {
            size_t caller = 0, ignored = SIZE_MAX;
            for (; caller < s->caller_count; caller++) if (s->callers[caller].decl_id == caller_id) break;
            if (caller_id != risk->caller_id || caller == s->caller_count ||
                !semantic_collection_effect_snapshot_call(s, caller, sink, &ignored) ||
                ordinal >= ast_call_arg_count(sink) || ast_call_argument(sink, ordinal) != value) return false;
        }
        size_t matched = 0;
        bool slot_seen = false, value_seen = false;
        for (size_t u = 0; u < risk->use_count; u++) {
            const IndexedStringNonownerUse *use = &risk->uses[u];
            bool slot = (use->sink == sink || use->sink_id == ast_node_stable_id(sink)) && use->actual_ordinal == ordinal;
            bool source = use->value == value || use->value_id == ast_node_stable_id(value);
            slot_seen |= slot;
            value_seen |= source;
            if (!slot || !source) continue;
            if (++matched != 1 || caller_id != risk->caller_id || use->sink != sink ||
                use->sink_id != ast_node_stable_id(sink) || use->value != value ||
                use->value_id != ast_node_stable_id(value) || use->shape != value->type ||
                use->binding_id != ast_identifier_binding_syntax_id(value) ||
                ordinal >= ast_call_arg_count(sink) || ast_call_argument(sink, ordinal) != value) return false;
        }
        if ((slot_seen || value_seen) && (!slot_seen || !value_seen || matched != 1)) return false;
    }
    size_t matches = 0;
    for (size_t u = 0; u < s->source_use_count; u++) {
        const IndexedStringFieldRecipe *r = s->source_uses[u];
        if (r == NULL) return false;
        bool slot = (r->sink == sink || r->sink_id == ast_node_stable_id(sink)) && r->sink_ordinal == ordinal;
        bool source = r->value == value || r->value_id == ast_node_stable_id(value);
        if (!slot && !source) continue;
        if (!slot || !source || ++matches != 1) return false;
        if (!r->issued || r->caller_id != caller_id || r->sink != sink ||
            r->sink_id != ast_node_stable_id(sink) || r->value != value ||
            r->value_id != ast_node_stable_id(value) || ordinal >= ast_call_arg_count(sink) ||
            ast_call_argument(sink, ordinal) != value) return false;
    }
    return true;
}

bool semantic_collection_effect_snapshot_source_value(const PgyCollectionEffectSnapshot *s,
    uint32_t caller_id, const ASTNode *value)
{
    if (!collection_independent_role(s, caller_id, NULL, value, 0, 0, NULL)) return false;
    if (s == NULL || s->refs == 0 || s->source_use_count != s->sealed_source_uses ||
        s->string_definition_count != s->sealed_string_definitions ||
        s->nonowner_risk_count != s->sealed_nonowner_risks ||
        s->permanent_element_count != s->sealed_permanent_elements ||
        s->independent_string_result_count != s->sealed_independent_string_results || value == NULL) return false;
    for (size_t p = 0; p < s->permanent_element_count; p++)
        if (!semantic_indexed_string_permanent_source_value(s->permanent_elements[p], caller_id, value)) return false;
    for (size_t u = 0; u < s->string_definition_count; u++) {
        const IndexedStringDefinitionRecipe *r = s->string_definitions[u];
        if (r == NULL || r->claims == NULL || r->assignment_count == SIZE_MAX ||
            r->assignment_count != r->sealed_assignments ||
            r->assignment_count > r->assignment_capacity ||
            (r->assignment_count != 0 && r->assignments == NULL)) return false;
        for (size_t input = 0; input <= r->assignment_count; input++) {
            const IndexedStringResultInputs *inputs = input == 0 ? &r->birth.named_inputs :
                &r->assignments[input - 1].rhs.named_inputs;
            if (!semantic_indexed_string_result_inputs_source_value(inputs,
                    caller_id, value)) return false;
        }
        bool sensitive = value == r->declaration || value == r->birth.value ||
            ast_node_stable_id(value) == r->binding_id ||
            (value->type == AST_IDENTIFIER && ast_identifier_binding_syntax_id(value) == r->binding_id);
        if (!sensitive) continue;
        if (caller_id != r->caller_id) return false;
        bool matched = false;
        for (size_t c = 0; c < r->claims->count; c++) {
            const FunctionParamFlowIndexClaim *row = &r->claims->rows[c];
            if (row->node == value && row->id == ast_node_stable_id(value) && row->shape == value->type)
                matched = true;
        }
        if (!matched) return false;
    }
    for (size_t u = 0; u < s->nonowner_risk_count; u++) {
        const IndexedStringNonownerRisk *risk = s->nonowner_risks[u];
        if (risk == NULL || risk->claims == NULL || risk->use_count != risk->sealed_uses ||
            risk->use_count > risk->use_capacity || (risk->use_count != 0 && risk->uses == NULL)) return false;
        for (size_t i = 0; i < risk->use_count; i++) {
            const IndexedStringNonownerUse *use = &risk->uses[i];
            if (use->value != value && use->value_id != ast_node_stable_id(value)) continue;
            if (caller_id != risk->caller_id || use->value != value || use->value_id != ast_node_stable_id(value) ||
                use->shape != value->type || use->binding_id != ast_identifier_binding_syntax_id(value)) return false;
        }
        bool sensitive = value == risk->definition || value == risk->initializer ||
            ast_node_stable_id(value) == risk->binding_id || ast_node_stable_id(value) == risk->initializer_id ||
            (value->type == AST_IDENTIFIER && ast_identifier_binding_syntax_id(value) == risk->binding_id);
        if (!sensitive) continue;
        if (caller_id != risk->caller_id) return false;
        bool matched = false;
        for (size_t r = 0; r < risk->claims->count; r++) {
            const FunctionParamFlowIndexClaim *row = &risk->claims->rows[r];
            if (row->node == value && row->id == ast_node_stable_id(value) && row->shape == value->type)
                matched = true;
        }
        if (!matched) return false;
    }
    for (size_t u = 0; u < s->source_use_count; u++) {
        const IndexedStringFieldRecipe *r = s->source_uses[u];
        if (r == NULL) return false;
        if (r->value != value && r->value_id != ast_node_stable_id(value)) continue;
        if (!r->issued || r->caller_id != caller_id || r->value != value ||
            r->value_id != ast_node_stable_id(value) || value->type != AST_MEMBER_ACCESS) return false;
    }
    return true;
}

static const ASTNode *collection_source_place_parent(const ASTNode *place)
{
    return place != NULL && place->type == AST_MEMBER_ACCESS ? ast_member_object(place) :
        place != NULL && place->type == AST_ARRAY_ACCESS ? ast_array_access_array(place) : NULL;
}

static bool collection_source_place_binding(const ASTNode *place, uint32_t *binding)
{
    const ASTNode *slow = place, *fast = place;
    while (fast != NULL && collection_source_place_parent(fast) != NULL) {
        slow = collection_source_place_parent(slow);
        fast = collection_source_place_parent(collection_source_place_parent(fast));
        if (slow == fast) return false;
    }
    const ASTNode *parent;
    while ((parent = collection_source_place_parent(place)) != NULL) place = parent;
    *binding = place != NULL && place->type == AST_IDENTIFIER ? ast_identifier_binding_syntax_id(place) :
        place != NULL && place->type == AST_LET_DECL ? ast_node_stable_id(place) : 0;
    return true;
}

static bool collection_source_evaluation_current(const PgyCollectionEffectSnapshot *s,
    uint32_t caller_id, const ASTNode *value, bool standalone)
{
    if (s == NULL || s->refs == 0 || s->source_use_count != s->sealed_source_uses ||
        s->string_definition_count != s->sealed_string_definitions ||
        s->nonowner_risk_count != s->sealed_nonowner_risks ||
        s->permanent_element_count != s->sealed_permanent_elements ||
        s->independent_string_result_count != s->sealed_independent_string_results) return false;
    for (size_t p = 0; standalone && p < s->permanent_element_count; p++)
        if (!semantic_indexed_string_permanent_source_evaluation(s->permanent_elements[p], caller_id, value)) return false;
    for (size_t u = 0; u < s->string_definition_count; u++) {
        const IndexedStringDefinitionRecipe *r = s->string_definitions[u];
        if (r == NULL || r->assignment_count == SIZE_MAX ||
            r->assignment_count != r->sealed_assignments ||
            r->assignment_count > r->assignment_capacity ||
            (r->assignment_count != 0 && r->assignments == NULL)) return false;
        /* A definition RHS has its own exact statement/place/value join
         * below. Only a standalone root is checked as a bare evaluation. */
        for (size_t input = 0; standalone && input <= r->assignment_count; input++) {
            const IndexedStringResultInputs *inputs = input == 0 ? &r->birth.named_inputs :
                &r->assignments[input - 1].rhs.named_inputs;
            if (!semantic_indexed_string_result_inputs_source_evaluation(inputs,
                    caller_id, value)) return false;
        }
        /* A call actual or binary child is not a standalone executable root.
         * Assignment targets are checked separately by the definition role. */
        if (value != NULL && value->type == AST_IDENTIFIER &&
            ast_identifier_binding_syntax_id(value) == r->binding_id) return false;
    }
    for (size_t u = 0; u < s->nonowner_risk_count; u++) {
        const IndexedStringNonownerRisk *risk = s->nonowner_risks[u];
        if (risk == NULL || risk->terminal_count != risk->sealed_terminals ||
            risk->terminal_count > risk->terminal_capacity ||
            (risk->terminal_count != 0 && risk->terminals == NULL)) return false;
        if (value != NULL && value->type == AST_IDENTIFIER &&
            ast_identifier_binding_syntax_id(value) == risk->binding_id) return false;
        for (size_t t = 0; value != NULL && t < risk->terminal_count; t++)
            if (risk->terminals[t].call == value ||
                risk->terminals[t].call_id == ast_node_stable_id(value)) return false;
    }
    return true;
}

bool semantic_collection_effect_snapshot_source_evaluation(const PgyCollectionEffectSnapshot *s,
    uint32_t caller_id, const ASTNode *value)
{
    return collection_source_evaluation_current(s, caller_id, value, true);
}

bool semantic_collection_effect_snapshot_source_definition(const PgyCollectionEffectSnapshot *s,
    uint32_t caller_id, uint32_t binding_id, bool initialization,
    const ASTNode *statement, const ASTNode *place, const ASTNode *value,
    size_t *write_ordinal)
{
    bool independent = false;
    if (!collection_independent_role(s, caller_id, statement, value, initialization ? 0 : 1,
            initialization ? AST_LET_DECL : AST_ASSIGNMENT, &independent)) return false;
    if (independent && (statement == NULL || (initialization ? place != statement ||
            binding_id != ast_node_stable_id(statement) || ast_let_initializer(statement) != value :
            ast_assignment_target(statement) != place || ast_assignment_value(statement) != value))) return false;
    if (independent && !initialization && place != NULL && place->type == AST_IDENTIFIER &&
        binding_id != ast_identifier_binding_syntax_id(place)) return false;
    if (write_ordinal != NULL) *write_ordinal = SIZE_MAX;
    if (s == NULL || s->refs == 0 || s->source_use_count != s->sealed_source_uses ||
        s->string_definition_count != s->sealed_string_definitions ||
        s->nonowner_risk_count != s->sealed_nonowner_risks ||
        s->permanent_element_count != s->sealed_permanent_elements ||
        s->independent_string_result_count != s->sealed_independent_string_results) return false;
    if (!collection_source_evaluation_current(s, caller_id, value, false)) return false;
    if (s->source_use_count == 0 && s->nonowner_risk_count == 0 &&
        s->string_definition_count == 0 && s->permanent_element_count == 0 &&
        s->independent_string_result_count == 0) return true;
    uint32_t actual_binding = 0;
    if (!collection_source_place_binding(place, &actual_binding)) return false;
    size_t offset = 0, matched_write = SIZE_MAX;
    for (size_t u = 0; u < s->string_definition_count; u++) {
        const IndexedStringDefinitionRecipe *r = s->string_definitions[u];
        if (r == NULL || r->assignment_count != r->sealed_assignments ||
            r->assignment_count > r->assignment_capacity ||
            (r->assignment_count != 0 && r->assignments == NULL) ||
            r->assignment_count == SIZE_MAX ||
            offset > SIZE_MAX - 1 - r->assignment_count) return false;
        uint32_t statement_id = statement == NULL ? 0 : ast_node_stable_id(statement);
        bool protected_statement = statement != NULL &&
            (statement == r->declaration || statement_id == r->declaration_id);
        for (size_t a = 0; a < r->assignment_count; a++)
            if (statement != NULL && (statement == r->assignments[a].assignment ||
                statement_id == r->assignments[a].assignment_id)) protected_statement = true;
        bool protected_rhs = value != NULL && value->type == AST_IDENTIFIER &&
            ast_identifier_binding_syntax_id(value) == r->binding_id;
        if (binding_id == r->binding_id || actual_binding == r->binding_id ||
            protected_rhs || protected_statement) {
            if (caller_id != r->caller_id || binding_id != r->binding_id || actual_binding != r->binding_id)
                return false;
            if (initialization) {
                if (statement != r->declaration || statement_id != r->declaration_id ||
                    place != r->declaration || ast_node_stable_id(place) != r->declaration_id ||
                    value != r->birth.value || ast_node_stable_id(value) != r->birth.value_id ||
                    matched_write != SIZE_MAX) return false;
                matched_write = offset;
            } else {
                size_t matches = 0;
                for (size_t a = 0; a < r->assignment_count; a++) {
                    const IndexedStringDefinitionAssignment *row = &r->assignments[a];
                    if (statement == row->assignment && statement_id == row->assignment_id &&
                        place == row->target && ast_node_stable_id(place) == row->target_id &&
                        value == row->rhs.value && ast_node_stable_id(value) == row->rhs.value_id &&
                        row->binding_id == r->binding_id) {
                        matches++;
                        matched_write = offset + 1 + a;
                    }
                }
                if (matches != 1) return false;
            }
        }
        offset += 1 + r->assignment_count;
        for (size_t input = 0; input <= r->assignment_count; input++) {
            const IndexedStringResultInputs *inputs = input == 0 ? &r->birth.named_inputs :
                &r->assignments[input - 1].rhs.named_inputs;
            size_t count = 0, local = SIZE_MAX;
            if (!semantic_indexed_string_result_inputs_source_write_count(inputs, &count) ||
                count > SIZE_MAX - offset ||
                !semantic_indexed_string_result_inputs_source_definition(inputs,
                    caller_id, binding_id, initialization, statement, place, value, &local)) return false;
            if (local != SIZE_MAX) {
                if (local >= count) return false;
                /* Shared immutable proof inputs may name the same original
                 * birth. Every join must match; one actual write always uses
                 * its first stable ordinal, never a second permission. */
                if (matched_write == SIZE_MAX) matched_write = offset + local;
            }
            offset += count;
        }
    }
    for (size_t p = 0; p < s->permanent_element_count; p++) {
        const IndexedStringPermanentElementRecipe *recipe = s->permanent_elements[p];
        size_t count = 0, local = SIZE_MAX;
        if (!semantic_indexed_string_permanent_source_write_count(recipe, &count) ||
            count > SIZE_MAX - offset ||
            !semantic_indexed_string_permanent_source_definition(recipe,
                caller_id, binding_id, initialization, statement, place, value, &local)) return false;
        if (local != SIZE_MAX) {
            if (local >= count) return false;
            if (matched_write == SIZE_MAX) matched_write = offset + local;
        }
        offset += count;
    }
    for (size_t c = 0; c < s->caller_count; c++) {
        size_t count = s->callers[c].independent_string_write_count, local = SIZE_MAX;
        if (count > SIZE_MAX - offset) return false;
        if (independent && s->callers[c].decl_id == caller_id) {
            if (!function_param_flow_readfile_write_ordinal(s->claims[c], statement, &local) ||
                local == SIZE_MAX || local >= count) return false;
            if (matched_write == SIZE_MAX) matched_write = offset + local;
        }
        offset += count;
    }
    for (size_t u = 0; u < s->nonowner_risk_count; u++) {
        const IndexedStringNonownerRisk *r = s->nonowner_risks[u];
        if (r == NULL) return false;
        bool binding = binding_id == r->binding_id || actual_binding == r->binding_id;
        bool birth = value != NULL && (value == r->initializer || ast_node_stable_id(value) == r->initializer_id);
        if (!binding && !birth) continue;
        if (!initialization || caller_id != r->caller_id || binding_id != r->binding_id ||
            actual_binding != r->binding_id || place != r->definition || value != r->initializer ||
            ast_node_stable_id(place) != r->binding_id || ast_node_stable_id(value) != r->initializer_id) return false;
    }
    for (size_t u = 0; u < s->source_use_count; u++) {
        const IndexedStringFieldRecipe *r = s->source_uses[u];
        if (r == NULL) return false;
        bool root = binding_id == r->binding_id || actual_binding == r->binding_id;
        bool producer = value != NULL && (value == r->producer || ast_node_stable_id(value) == r->producer_id);
        if (root || (initialization && producer)) {
            if (!initialization || caller_id != r->caller_id || binding_id != r->binding_id ||
                actual_binding != r->binding_id || place != r->definition ||
                ast_node_stable_id(place) != r->definition_id || value != r->producer ||
                ast_node_stable_id(value) != r->producer_id) return false;
        }
        if (caller_id != r->producer_decl_id) continue;
        for (size_t t = 0; t < r->return_count; t++) {
            uint32_t formal = r->returns[t].formal_id;
            if (formal != 0 && (binding_id == formal || actual_binding == formal)) return false;
        }
    }
    if (write_ordinal != NULL) *write_ordinal = matched_write;
    return true;
}

bool semantic_collection_effect_snapshot_source_return(const PgyCollectionEffectSnapshot *s,
    uint32_t caller_id, const ASTNode *statement, const ASTNode *value)
{
    if (!collection_independent_role(s, caller_id, statement, value, 0, AST_RETURN, NULL)) return false;
    if (s == NULL || s->refs == 0 || s->source_use_count != s->sealed_source_uses ||
        s->string_definition_count != s->sealed_string_definitions ||
        s->nonowner_risk_count != s->sealed_nonowner_risks ||
        s->permanent_element_count != s->sealed_permanent_elements ||
        s->independent_string_result_count != s->sealed_independent_string_results ||
        !collection_independent_return_current(s, caller_id, statement, value)) return false;
    for (size_t p = 0; p < s->permanent_element_count; p++)
        if (!semantic_indexed_string_permanent_source_return(s->permanent_elements[p],
                caller_id, statement, value)) return false;
    for (size_t u = 0; u < s->string_definition_count; u++) {
        const IndexedStringDefinitionRecipe *r = s->string_definitions[u];
        if (r == NULL || r->assignment_count == SIZE_MAX ||
            r->assignment_count != r->sealed_assignments ||
            r->assignment_count > r->assignment_capacity ||
            (r->assignment_count != 0 && r->assignments == NULL) ||
            r->use_count != r->sealed_uses ||
            r->use_count > r->use_capacity ||
            (r->use_count != 0 && r->uses == NULL)) return false;
        for (size_t input = 0; input <= r->assignment_count; input++) {
            const IndexedStringResultInputs *inputs = input == 0 ? &r->birth.named_inputs :
                &r->assignments[input - 1].rhs.named_inputs;
            if (!semantic_indexed_string_result_inputs_source_return(inputs,
                    caller_id, statement, value)) return false;
        }
        bool returned_binding = value != NULL && value->type == AST_IDENTIFIER &&
            ast_identifier_binding_syntax_id(value) == r->binding_id;
        size_t matched = 0;
        for (size_t use_index = 0; use_index < r->use_count; use_index++) {
            const IndexedStringDefinitionUse *use = &r->uses[use_index];
            bool return_slot = use->sink == statement ||
                (statement != NULL && use->sink_id == ast_node_stable_id(statement));
            bool return_value = use->value == value ||
                (value != NULL && use->value_id == ast_node_stable_id(value));
            if (!return_slot && !return_value)
                continue;
            if (!returned_binding || caller_id != r->caller_id ||
                use->sink != statement ||
                use->sink_id != ast_node_stable_id(statement) ||
                use->value != value ||
                use->value_id != ast_node_stable_id(value) ||
                use->binding_id != r->binding_id ||
                use->actual_ordinal != SIZE_MAX ||
                statement == NULL || statement->type != AST_RETURN ||
                ast_return_value(statement) != value)
                return false;
            matched++;
        }
        if (returned_binding ? matched != 1 : matched != 0)
            return false;
    }
    for (size_t u = 0; u < s->nonowner_risk_count; u++) {
        const IndexedStringNonownerRisk *risk = s->nonowner_risks[u];
        if (risk == NULL || risk->terminal_count != risk->sealed_terminals ||
            risk->terminal_count > risk->terminal_capacity ||
            (risk->terminal_count != 0 && risk->terminals == NULL)) return false;
        if (value != NULL && value->type == AST_IDENTIFIER &&
            ast_identifier_binding_syntax_id(value) == risk->binding_id) return false;
        for (size_t t = 0; value != NULL && t < risk->terminal_count; t++) {
            const IndexedStringNonownerTerminal *terminal = &risk->terminals[t];
            if (terminal->call != value && terminal->call_id != ast_node_stable_id(value)) continue;
            if (caller_id != risk->caller_id || statement != terminal->statement || statement == NULL ||
                statement->type != AST_RETURN || ast_node_stable_id(statement) != terminal->statement_id ||
                value != terminal->call || ast_node_stable_id(value) != terminal->call_id ||
                ast_return_value(statement) != value) return false;
        }
    }
    for (size_t u = 0; u < s->source_use_count; u++) {
        const IndexedStringFieldRecipe *r = s->source_uses[u];
        if (r == NULL) return false;
        bool matched = false;
        for (size_t t = 0; t < r->return_count; t++) {
            const IndexedStringFieldReturn *ret = &r->returns[t];
            if (value == NULL || (value != ret->constructor && ast_node_stable_id(value) != ret->constructor_id)) continue;
            if (caller_id != r->producer_decl_id || value != ret->constructor ||
                ast_node_stable_id(value) != ret->constructor_id) return false;
            matched = true;
        }
        if (caller_id == r->producer_decl_id && !matched) return false;
    }
    return true;
}
