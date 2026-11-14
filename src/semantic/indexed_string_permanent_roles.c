#include "indexed_string_permanent_internal.h"
#include "indexed_string_option_payload.h"
#include "function_param_flow/function_param_publication_internal.h"
#include <string.h>

static bool permanent_id(const ASTNode *node, uint32_t id)
{ return node != NULL && id != 0 && ast_node_stable_id(node) == id; }
static bool permanent_same(const ASTNode *value, const ASTNode *original, uint32_t id)
{ return value != NULL && (value == original || (id != 0 && ast_node_stable_id(value) == id)); }

static const FunctionParamFlowIndexClaim *permanent_member(const FunctionParamFlowIndexClaims *claims,
    const ASTNode *value)
{
    if (claims == NULL || claims->rows == NULL || value == NULL) return NULL;
    const FunctionParamFlowIndexClaim *found = NULL;
    for (size_t i = 0; i < claims->count; i++) if (claims->rows[i].node == value) {
        if (found != NULL || claims->rows[i].id == 0 || claims->rows[i].id != ast_node_stable_id(value) ||
            claims->rows[i].shape != value->type) return NULL;
        found = &claims->rows[i];
    }
    return found;
}

bool indexed_string_permanent_history(SemanticContext *ctx,
    IndexedStringPermanentElementRecipe *recipe, const IndexedStringPermanentBirth *birth, bool prepare)
{
    const FunctionParamFlowIndexClaims *claims = birth->claims;
    const FunctionParamFlowIndexClaim *definition = permanent_member(claims, birth->declaration);
    if (definition == NULL || definition->shape != AST_LET_DECL || definition->id != birth->binding_id ||
        definition->local_alias || definition->child_count != 2 || definition->children == NULL ||
        definition->children[0] != birth->initializer) return false;
    for (size_t i = 0; i < claims->count; i++) {
        const FunctionParamFlowIndexClaim *row = &claims->rows[i];
        switch (row->shape) {
        case AST_LAMBDA_EXPR: case AST_ASYNC_BLOCK: case AST_SPAWN_EXPR: case AST_PARALLEL_BLOCK:
        case AST_DEFER_STMT: case AST_CHANNEL_SEND: case AST_TRANSACTION_BLOCK: case AST_UNSAFE_BLOCK:
        case AST_GIVE_STMT: case AST_EVENT_INVOKE: case AST_EVENT_SUBSCRIBE: case AST_EVENT_UNSUBSCRIBE:
            return false;
        default: break;
        }
        if (row->shape != AST_IDENTIFIER || row->binding != birth->binding_id) continue;
        if (!row->semantic_promoted || !row->snapshot_semantic_complete || !row->value_ready || row->value_meta ||
            ast_identifier_binding_syntax_id(row->node) != birth->binding_id) return false;
        size_t parents = 0;
        for (size_t p = 0; p < claims->count; p++) {
            const FunctionParamFlowIndexClaim *parent = &claims->rows[p];
            for (size_t c = 0; c < parent->child_count; c++) if (parent->children[c] == row->node) {
                if (parent->child_claim_ordinals == NULL || parent->child_claim_ordinals[c] != i) return false;
                if (parent->shape == AST_ARRAY_ACCESS && c == 0) {
                    const ASTNode *index = ast_array_access_index(parent->node);
                    const FunctionParamFlowIndexClaim *offset = permanent_member(claims, index);
                    if (!parent->semantic_promoted || !parent->snapshot_semantic_complete || !parent->value_ready ||
                        parent->value_meta || offset == NULL || !offset->value_ready || offset->value_meta || !offset->physical_value)
                        return false;
                    for (size_t a = 0; a < claims->count; a++) if (claims->rows[a].shape == AST_ASSIGNMENT &&
                        ast_assignment_target(claims->rows[a].node) == parent->node) return false;
                } else if (parent->shape == AST_CALL && c == 1) {
                    const ASTNode *target = ast_call_callee(parent->node);
                    uint32_t kind = 0;
                    if (target == NULL || target->type != AST_IDENTIFIER || ast_identifier_name(target) == NULL ||
                        strcmp(ast_identifier_name(target), "ArrayLength") != 0 || ast_call_arg_count(parent->node) != 1 ||
                        !ast_call_semantic_callee_is_stdlib(parent->node) || ast_call_semantic_callee_decl_id(parent->node) != 0 ||
                        ast_call_semantic_callee_declared_callable(parent->node) || ast_call_semantic_callee_value_binding_id(parent->node) != 0 ||
                        !ast_call_semantic_callee_builtin_kind(parent->node, &kind) || kind != BUILTIN_NOT_BUILTIN ||
                        !parent->semantic_promoted || !parent->snapshot_semantic_complete || !parent->physical_value ||
                        (ctx != NULL && (!semantic_param_publication_checked_argument(ctx, parent->node, 0) ||
                            !indexed_string_result_capture_call(ctx, parent->node, &recipe->inputs, prepare)))) return false;
                } else if (parent->shape == AST_RETURN && c == 0 && birth->declaration != recipe->declaration) {
                    if (ast_return_value(parent->node) != row->node) return false;
                } else return false;
                parents++;
            }
        }
        if (parents == 0) return false;
    }
    return true;
}

bool semantic_indexed_string_permanent_syntax_current(const IndexedStringPermanentElementRecipe *recipe)
{
    if (recipe == NULL || !recipe->prepared || recipe->birth_count == 0 || recipe->return_count == 0 ||
        recipe->birth_count != recipe->sealed_births || recipe->return_count != recipe->sealed_returns ||
        recipe->birth_count > recipe->birth_capacity || recipe->return_count > recipe->return_capacity ||
        recipe->births == NULL || recipe->returns == NULL ||
        !permanent_id(recipe->caller, recipe->caller_id) || !permanent_id(recipe->some, recipe->some_id) ||
        !permanent_id(recipe->value, recipe->value_id) || !permanent_id(recipe->receiver, recipe->receiver_id) ||
        !permanent_id(recipe->index, recipe->index_id) || !permanent_id(recipe->declaration, recipe->binding_id) ||
        !permanent_id(recipe->producer, recipe->producer_id) || recipe->source_id == 0 || recipe->source_id == UINT32_MAX ||
        !semantic_indexed_string_permanent_some_boundary(recipe->some) || ast_call_argument(recipe->some, 0) != recipe->value ||
        recipe->value->type != AST_ARRAY_ACCESS || ast_array_access_array(recipe->value) != recipe->receiver ||
        ast_array_access_index(recipe->value) != recipe->index || recipe->receiver->type != AST_IDENTIFIER ||
        ast_identifier_binding_syntax_id(recipe->receiver) != recipe->binding_id ||
        recipe->declaration->type != AST_LET_DECL || ast_let_initializer(recipe->declaration) != recipe->producer ||
        !semantic_indexed_string_result_inputs_syntax_current(&recipe->inputs)) return false;
    bool source_use = false;
    for (size_t d = 0; d < recipe->inputs.dependency_count; d++) {
        const IndexedStringFieldDependency *dependency = &recipe->inputs.dependencies[d];
        if (dependency->decl != recipe->caller || dependency->decl_id != recipe->caller_id) continue;
        const FunctionParamFlowIndexClaim *some = permanent_member(dependency->claims, recipe->some);
        const FunctionParamFlowIndexClaim *value = permanent_member(dependency->claims, recipe->value);
        if (some == NULL || value == NULL || !some->semantic_promoted || !some->snapshot_semantic_complete ||
            !some->result_relation || !some->result_depends || some->external_retains ||
            !some->backing_relation || !some->backing_depends || some->backing_external_retains ||
            !value->semantic_promoted || !value->snapshot_semantic_complete || !value->value_ready || value->value_meta ||
            some->child_count != 2 || some->children == NULL || some->child_ids == NULL ||
            some->child_claim_ordinals == NULL || some->children[1] != recipe->value ||
            some->child_ids[1] != recipe->value_id || some->child_claim_ordinals[1] >= dependency->claims->count ||
            &dependency->claims->rows[some->child_claim_ordinals[1]] != value || some->result_type_name == NULL ||
            ast_call_semantic_value_type_name(recipe->some) == NULL ||
            strcmp(some->result_type_name, ast_call_semantic_value_type_name(recipe->some)) != 0) return false;
        source_use = true;
    }
    if (!source_use) return false;
    for (size_t b = 0; b < recipe->birth_count; b++) {
        const IndexedStringPermanentBirth *birth = &recipe->births[b];
        if (!permanent_id(birth->caller, birth->caller_id) || !permanent_id(birth->declaration, birth->binding_id) ||
            !permanent_id(birth->initializer, birth->initializer_id) || birth->declaration->type != AST_LET_DECL ||
            ast_let_is_mutable(birth->declaration) || ast_let_is_alias(birth->declaration) ||
            ast_let_initializer(birth->declaration) != birth->initializer ||
            !indexed_string_permanent_history(NULL, (IndexedStringPermanentElementRecipe *)recipe, birth, false)) return false;
        bool dependency = false;
        for (size_t d = 0; d < recipe->inputs.dependency_count; d++) if (recipe->inputs.dependencies[d].decl == birth->caller &&
            recipe->inputs.dependencies[d].claims == birth->claims) dependency = true;
        if (!dependency) return false;
        for (size_t p = 0; p < b; p++) if (recipe->births[p].binding_id == birth->binding_id) return false;
    }
    for (size_t r = 0; r < recipe->return_count; r++) {
        const IndexedStringPermanentReturn *row = &recipe->returns[r];
        if (!permanent_id(row->caller, row->caller_id) || !permanent_id(row->statement, row->statement_id) ||
            !permanent_id(row->value, row->value_id) || !permanent_id(row->literal, row->literal_id) ||
            row->statement->type != AST_RETURN || ast_return_value(row->statement) != row->value ||
            row->literal->type != AST_ARRAY_LITERAL) return false;
        if (row->source == ARRAY_RESULT_LITERAL) {
            if (row->value != row->literal || row->binding_id != 0 || row->callee_id != 0) return false;
        } else if (row->source == ARRAY_RESULT_OWNED_BINDING) {
            if (row->value->type != AST_IDENTIFIER || row->binding_id == 0 || row->callee_id != 0 ||
                ast_identifier_binding_syntax_id(row->value) != row->binding_id) return false;
            bool origin = false;
            for (size_t b = 0; b < recipe->birth_count; b++) if (recipe->births[b].caller == row->caller &&
                recipe->births[b].binding_id == row->binding_id && recipe->births[b].initializer == row->literal) origin = true;
            if (!origin) return false;
        } else return false;
        const FunctionParamFlowIndexClaims *claims = NULL;
        for (size_t d = 0; d < recipe->inputs.dependency_count; d++)
            if (recipe->inputs.dependencies[d].decl == row->caller) claims = recipe->inputs.dependencies[d].claims;
        const FunctionParamFlowIndexClaim *literal = permanent_member(claims, row->literal);
        if (permanent_member(claims, row->statement) == NULL || literal == NULL ||
            !literal->semantic_promoted || !literal->snapshot_semantic_complete || !literal->value_ready || literal->value_meta ||
            literal->child_count != ast_array_literal_count(row->literal)) return false;
        for (size_t e = 0; e < literal->child_count; e++) {
            if (literal->children == NULL || literal->child_ids == NULL || literal->child_claim_ordinals == NULL ||
                literal->children[e] == NULL || literal->children[e]->type != AST_STRING ||
                literal->child_claim_ordinals[e] >= claims->count) return false;
            const FunctionParamFlowIndexClaim *child = &claims->rows[literal->child_claim_ordinals[e]];
            if (child->node != literal->children[e] || child->id != literal->child_ids[e] || child->shape != AST_STRING ||
                !child->semantic_promoted || !child->snapshot_semantic_complete || !child->value_ready || child->value_meta)
                return false;
        }
    }
    return true;
}

bool semantic_indexed_string_permanent_source_write_count(const IndexedStringPermanentElementRecipe *recipe, size_t *count)
{
    if (count == NULL) return false;
    *count = 0;
    if (!semantic_indexed_string_permanent_syntax_current(recipe)) return false;
    *count = recipe->birth_count;
    return true;
}

bool semantic_indexed_string_permanent_source_root(const IndexedStringPermanentElementRecipe *recipe,
    uint32_t caller, const ASTNode *value)
{
    if (recipe == NULL || !recipe->prepared) return false;
    for (size_t d = 0; d < recipe->inputs.dependency_count; d++) if (recipe->inputs.dependencies[d].decl_id == caller)
        return permanent_member(recipe->inputs.dependencies[d].claims, value) != NULL;
    return semantic_indexed_string_permanent_source_value(recipe, caller, value);
}

bool semantic_indexed_string_permanent_source_value(const IndexedStringPermanentElementRecipe *recipe,
    uint32_t caller, const ASTNode *value)
{
    if (recipe == NULL || !recipe->prepared) return false;
    if (value == NULL) return true;
    if (permanent_same(value, recipe->some, recipe->some_id) &&
        (caller != recipe->caller_id || value != recipe->some || !permanent_id(value, recipe->some_id))) return false;
    if (permanent_same(value, recipe->value, recipe->value_id) &&
        (caller != recipe->caller_id || value != recipe->value || !permanent_id(value, recipe->value_id))) return false;
    for (size_t b = 0; b < recipe->birth_count; b++) {
        const IndexedStringPermanentBirth *birth = &recipe->births[b];
        if (permanent_same(value, birth->initializer, birth->initializer_id) &&
            (value != birth->initializer || !permanent_id(value, birth->initializer_id) || caller != birth->caller_id)) return false;
        if (value->type == AST_IDENTIFIER && ast_identifier_binding_syntax_id(value) == birth->binding_id &&
            (caller != birth->caller_id || permanent_member(birth->claims, value) == NULL)) return false;
    }
    for (size_t r = 0; r < recipe->return_count; r++) if (permanent_same(value, recipe->returns[r].value, recipe->returns[r].value_id) &&
        (caller != recipe->returns[r].caller_id || value != recipe->returns[r].value ||
         !permanent_id(value, recipe->returns[r].value_id))) return false;
    return true;
}

bool semantic_indexed_string_permanent_source_use(const IndexedStringPermanentElementRecipe *recipe,
    uint32_t caller, const ASTNode *sink, size_t ordinal, const ASTNode *value)
{
    if (!semantic_indexed_string_permanent_source_value(recipe, caller, value)) return false;
    bool tracked = permanent_same(value, recipe->value, recipe->value_id) || permanent_same(sink, recipe->some, recipe->some_id);
    for (size_t b = 0; value != NULL && b < recipe->birth_count; b++)
        if ((value->type == AST_IDENTIFIER && ast_identifier_binding_syntax_id(value) == recipe->births[b].binding_id) ||
            permanent_same(value, recipe->births[b].initializer, recipe->births[b].initializer_id)) tracked = true;
    for (size_t r = 0; r < recipe->return_count; r++)
        if (permanent_same(value, recipe->returns[r].value, recipe->returns[r].value_id)) tracked = true;
    if (!tracked) return true;
    for (size_t d = 0; d < recipe->inputs.dependency_count; d++) {
        const IndexedStringFieldDependency *dependency = &recipe->inputs.dependencies[d];
        if (dependency->decl_id != caller) continue;
        const FunctionParamFlowIndexClaim *call = permanent_member(dependency->claims, sink);
        if (call == NULL || call->shape != AST_CALL || ordinal >= ast_call_arg_count(sink) ||
            ordinal + 1 >= call->child_count || call->children == NULL || call->children[ordinal + 1] != value ||
            ast_call_argument(sink, ordinal) != value) return false;
        return permanent_member(dependency->claims, value) != NULL;
    }
    return false;
}

bool semantic_indexed_string_permanent_source_evaluation(const IndexedStringPermanentElementRecipe *recipe,
    uint32_t caller, const ASTNode *value)
{
    if (!semantic_indexed_string_permanent_source_value(recipe, caller, value)) return false;
    for (size_t b = 0; value != NULL && b < recipe->birth_count; b++) if (
        (value->type == AST_IDENTIFIER && ast_identifier_binding_syntax_id(value) == recipe->births[b].binding_id) ||
        permanent_same(value, recipe->births[b].initializer, recipe->births[b].initializer_id)) return false;
    for (size_t r = 0; r < recipe->return_count; r++)
        if (permanent_same(value, recipe->returns[r].value, recipe->returns[r].value_id)) return false;
    return true;
}

bool semantic_indexed_string_permanent_source_definition(const IndexedStringPermanentElementRecipe *recipe,
    uint32_t caller, uint32_t binding, bool initialization, const ASTNode *statement,
    const ASTNode *place, const ASTNode *value, size_t *write_ordinal)
{
    if (write_ordinal != NULL) *write_ordinal = SIZE_MAX;
    if (recipe == NULL || !recipe->prepared) return false;
    bool matched = false;
    for (size_t b = 0; b < recipe->birth_count; b++) {
        const IndexedStringPermanentBirth *birth = &recipe->births[b];
        bool target = binding == birth->binding_id || permanent_same(statement, birth->declaration, birth->binding_id);
        const ASTNode *root = place;
        if (root != NULL && (root->type == AST_MEMBER_ACCESS || root->type == AST_ARRAY_ACCESS))
            root = root->type == AST_MEMBER_ACCESS ? ast_member_object(root) : ast_array_access_array(root);
        target |= root != NULL && root->type == AST_IDENTIFIER && ast_identifier_binding_syntax_id(root) == birth->binding_id;
        bool rhs = permanent_same(value, birth->initializer, birth->initializer_id);
        if (!target && !rhs) continue;
        if (!target || !rhs || !initialization || caller != birth->caller_id || binding != birth->binding_id ||
            statement != birth->declaration || place != birth->declaration || value != birth->initializer ||
            !permanent_id(statement, birth->binding_id) || !permanent_id(value, birth->initializer_id)) return false;
        matched = true;
        if (write_ordinal != NULL && *write_ordinal == SIZE_MAX) *write_ordinal = b;
    }
    return matched || semantic_indexed_string_permanent_source_evaluation(recipe, caller, value);
}

bool semantic_indexed_string_permanent_source_return(const IndexedStringPermanentElementRecipe *recipe,
    uint32_t caller, const ASTNode *statement, const ASTNode *value)
{
    if (!semantic_indexed_string_permanent_source_value(recipe, caller, value)) return false;
    bool producer = false, protected_value = false, exact = false;
    for (size_t r = 0; r < recipe->return_count; r++) {
        const IndexedStringPermanentReturn *row = &recipe->returns[r];
        if (row->caller_id == caller) producer = true;
        if (permanent_same(value, row->value, row->value_id)) protected_value = true;
        if (row->caller_id == caller && statement == row->statement && value == row->value &&
            permanent_id(statement, row->statement_id) && permanent_id(value, row->value_id)) exact = true;
    }
    return producer || protected_value ? exact : semantic_indexed_string_permanent_source_evaluation(recipe, caller, value);
}
