#include "collection_ownership_effect_snapshot_internal.h"
#include "../parser/ast_api.h"
#include <stdint.h>

/* The boundary already replayed shared claims. Per-use queries only consume
 * their membership and immutable admission edges, not another whole body. */
bool semantic_collection_effect_snapshot_source_root(const PgyCollectionEffectSnapshot *s,
    uint32_t caller_id, const ASTNode *root)
{
    if (s == NULL || s->refs == 0 || s->source_use_count != s->sealed_source_uses ||
        s->nonowner_risk_count != s->sealed_nonowner_risks) return false;
    if ((s->source_use_count == 0 && s->nonowner_risk_count == 0) || root == NULL) return true;
    if (caller_id == 0 || ast_node_stable_id(root) == 0) return false;
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
    if (s == NULL || s->refs == 0 || s->source_use_count != s->sealed_source_uses ||
        s->nonowner_risk_count != s->sealed_nonowner_risks ||
        sink == NULL || sink->type != AST_CALL || value == NULL) return false;
    uint32_t binding = 0;
    if (!collection_source_place_binding(value, &binding)) return false;
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
    if (s == NULL || s->refs == 0 || s->source_use_count != s->sealed_source_uses ||
        s->nonowner_risk_count != s->sealed_nonowner_risks || value == NULL) return false;
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

bool semantic_collection_effect_snapshot_source_evaluation(const PgyCollectionEffectSnapshot *s,
    uint32_t caller_id, const ASTNode *value)
{
    if (s == NULL || s->refs == 0 || s->source_use_count != s->sealed_source_uses ||
        s->nonowner_risk_count != s->sealed_nonowner_risks) return false;
    (void)caller_id; /* Protected descriptors/carriers cannot migrate to another caller either. */
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

bool semantic_collection_effect_snapshot_source_definition(const PgyCollectionEffectSnapshot *s,
    uint32_t caller_id, uint32_t binding_id, bool initialization,
    const ASTNode *place, const ASTNode *value)
{
    if (s == NULL || s->refs == 0 || s->source_use_count != s->sealed_source_uses ||
        s->nonowner_risk_count != s->sealed_nonowner_risks) return false;
    if (!semantic_collection_effect_snapshot_source_evaluation(s, caller_id, value)) return false;
    if (s->source_use_count == 0 && s->nonowner_risk_count == 0) return true;
    uint32_t actual_binding = 0;
    if (!collection_source_place_binding(place, &actual_binding)) return false;
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
    return true;
}

bool semantic_collection_effect_snapshot_source_return(const PgyCollectionEffectSnapshot *s,
    uint32_t caller_id, const ASTNode *statement, const ASTNode *value)
{
    if (s == NULL || s->refs == 0 || s->source_use_count != s->sealed_source_uses ||
        s->nonowner_risk_count != s->sealed_nonowner_risks) return false;
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
