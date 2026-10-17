#include "indexed_string_option_payload.h"
#include "function_param_publication_internal.h"
#include "../common/match_variant_policy.h"
#include <stdlib.h>
#include <string.h>

static bool option_id(const ASTNode *node, uint32_t id)
{ return node != NULL && id != 0 && ast_node_stable_id(node) == id; }

static bool option_same(const ASTNode *actual, const ASTNode *saved, uint32_t id)
{ return actual != NULL && (actual == saved || (id != 0 && ast_node_stable_id(actual) == id)); }

static bool option_input_seal_current(const IndexedStringOptionInputs *option)
{
    return option != NULL && option->sealed && option->birth_count != 0 && option->return_count != 0 &&
        option->relation_count != 0 && option->birth_count == option->sealed_births &&
        option->return_count == option->sealed_returns && option->relation_count == option->sealed_relations &&
        option->use_count == option->sealed_uses && option->birth_count <= option->birth_capacity &&
        option->return_count <= option->return_capacity && option->relation_count <= option->relation_capacity &&
        option->use_count <= option->use_capacity && option->births != NULL && option->returns != NULL &&
        option->relations != NULL && (option->use_count == 0 || option->uses != NULL);
}

bool indexed_string_option_use_record(IndexedStringOptionInputs *option,
    const IndexedStringOptionUse *row, bool prepare)
{
    if (!option_id(row->caller, row->caller_id) || !option_id(row->sink, row->sink_id) ||
        !option_id(row->value, row->value_id) || row->binding == 0 || row->sink->type != AST_CALL ||
        row->ordinal >= ast_call_arg_count(row->sink) || ast_call_argument(row->sink, row->ordinal) != row->value)
        return false;
    for (size_t i = 0; i < option->use_count; i++) {
        const IndexedStringOptionUse *old = &option->uses[i];
        if (old->sink != row->sink || old->ordinal != row->ordinal) continue;
        return old->caller == row->caller && old->caller_id == row->caller_id && old->value == row->value &&
            old->value_id == row->value_id && old->sink_id == row->sink_id && old->binding == row->binding;
    }
    if (!prepare || option->use_count >= SIZE_MAX / sizeof(*option->uses)) return false;
    IndexedStringOptionUse *grown = realloc(option->uses, (option->use_count + 1) * sizeof(*grown));
    if (grown == NULL) return false;
    option->uses = grown; grown[option->use_count++] = *row;
    option->use_capacity = option->use_count;
    return true;
}

bool indexed_string_option_birth_uses(SemanticContext *ctx, IndexedStringResultInputs *inputs,
    const IndexedStringOptionBirth *birth, bool prepare)
{
    const FunctionParamFlowIndexClaims *claims = birth->claims;
    if (claims == NULL || claims->failed || claims->rows == NULL || claims->count == 0) return false;
    size_t declaration_count = 0;
    for (size_t i = 0; i < claims->count; i++) {
        const FunctionParamFlowIndexClaim *row = &claims->rows[i];
        switch (row->shape) {
        case AST_LAMBDA_EXPR: case AST_ASYNC_BLOCK: case AST_SPAWN_EXPR: case AST_PARALLEL_BLOCK:
        case AST_DEFER_STMT: case AST_CHANNEL_SEND: case AST_TRANSACTION_BLOCK: case AST_UNSAFE_BLOCK:
        case AST_GIVE_STMT: return false;
        default: break;
        }
        if (row->node == birth->declaration) {
            if (++declaration_count != 1 || row->shape != AST_LET_DECL || row->id != birth->binding ||
                row->local_alias || row->child_count != 2 || row->children == NULL ||
                row->children[0] != birth->initializer) return false;
        }
        if (row->shape != AST_IDENTIFIER || row->binding != birth->binding) continue;
        if (!row->semantic_promoted || !row->snapshot_semantic_complete || !row->value_ready || row->value_meta ||
            ast_identifier_binding_syntax_id(row->node) != birth->binding) return false;
        size_t parents = 0;
        for (size_t p = 0; p < claims->count; p++) {
            const FunctionParamFlowIndexClaim *parent = &claims->rows[p];
            for (size_t c = 0; c < parent->child_count; c++) if (parent->children[c] == row->node) {
                if (parent->child_claim_ordinals == NULL || parent->child_claim_ordinals[c] != i ||
                    parent->shape != AST_CALL || c == 0 || c - 1 >= ast_call_arg_count(parent->node) ||
                    ast_call_argument(parent->node, c - 1) != row->node ||
                    !semantic_param_publication_checked_call(ctx, parent->node) ||
                    !semantic_param_publication_checked_argument(ctx, parent->node, c - 1)) return false;
                if (birth->carrier) {
                    const ASTNode *target = ast_call_callee(parent->node);
                    uint32_t kind = 0;
                    const char *name = target == NULL ? NULL : ast_identifier_name(target);
                    if (c != 1 || ast_call_arg_count(parent->node) != 1 || target == NULL ||
                        target->type != AST_IDENTIFIER || name == NULL ||
                        (strcmp(name, "IsSome") != 0 && strcmp(name, "UnwrapOption") != 0) ||
                        !ast_call_semantic_callee_is_stdlib(parent->node) ||
                        ast_call_semantic_callee_decl_id(parent->node) != 0 ||
                        ast_call_semantic_callee_declared_callable(parent->node) ||
                        ast_call_semantic_callee_value_binding_id(parent->node) != 0 ||
                        !ast_call_semantic_callee_builtin_kind(parent->node, &kind) || kind != BUILTIN_NOT_BUILTIN)
                        return false;
                    if (strcmp(name, "UnwrapOption") == 0 && !indexed_string_option_relation_record(
                        ctx, inputs, parent->node, true, prepare)) return false;
                } else {
                    ASTNode *callee = semantic_param_publication_direct_callee(ctx, parent->node);
                    if (callee != NULL) {
                        size_t ordinal = SIZE_MAX;
                        if (!semantic_param_publication_formal_for_actual(ctx, parent->node, c - 1, &ordinal)) return false;
                        FuncParam *formal = ast_func_param(callee, ordinal);
                        if (formal == NULL || (formal->mode != PARAM_MODE_DEFAULT && formal->mode != PARAM_MODE_REF))
                            return false;
                    }
                }
                if (!indexed_string_result_capture_call(ctx, parent->node, inputs, prepare)) return false;
                IndexedStringOptionUse use = {birth->caller, parent->node, row->node, birth->caller_id,
                    parent->id, row->id, birth->binding, c - 1};
                if (!indexed_string_option_use_record(inputs->option, &use, prepare)) return false;
                parents++;
            }
        }
        if (parents == 0) return false;
    }
    return declaration_count == 1;
}

bool indexed_string_option_syntax(const IndexedStringResultInputs *inputs)
{
    const IndexedStringOptionInputs *option = inputs->option;
    if (!option_input_seal_current(option) || !option_id(option->unwrap, option->unwrap_id)) return false;
    for (size_t b = 0; b < option->birth_count; b++) {
        const IndexedStringOptionBirth *birth = &option->births[b];
        if (!option_id(birth->declaration, birth->binding) || !option_id(birth->caller, birth->caller_id) ||
            !option_id(birth->initializer, birth->initializer_id) || birth->declaration->type != AST_LET_DECL ||
            ast_let_is_mutable(birth->declaration) || ast_let_is_alias(birth->declaration) ||
            ast_let_initializer(birth->declaration) != birth->initializer) return false;
        bool dependency = false, declaration = false;
        for (size_t d = 0; d < inputs->dependency_count; d++)
            if (inputs->dependencies[d].decl == birth->caller && inputs->dependencies[d].claims == birth->claims &&
                inputs->dependencies[d].decl_id == birth->caller_id) dependency = true;
        if (!dependency || birth->claims == NULL) return false;
        for (size_t r = 0; r < birth->claims->count; r++)
            if (birth->claims->rows[r].node == birth->declaration && birth->claims->rows[r].id == birth->binding)
                declaration = true;
        if (!declaration) return false;
        for (size_t p = 0; p < b; p++) if (option->births[p].binding == birth->binding) return false;
    }
    for (size_t i = 0; i < option->relation_count; i++) {
        const IndexedStringOptionRelation *row = &option->relations[i];
        if (!option_id(row->call, row->call_id) || !option_id(row->argument, row->argument_id) ||
            row->call->type != AST_CALL || ast_call_arg_count(row->call) != 1 ||
            ast_call_argument(row->call, 0) != row->argument || row->input_name == NULL || row->output_name == NULL ||
            row->argument_shape != row->argument->type || row->argument_binding !=
                (row->argument->type == AST_IDENTIFIER ? ast_identifier_binding_syntax_id(row->argument) : 0) ||
            ast_call_semantic_value_type_name(row->call) == NULL ||
            strcmp(row->output_name, ast_call_semantic_value_type_name(row->call)) != 0 ||
            (row->argument->type == AST_CALL && (ast_call_semantic_value_type_name(row->argument) == NULL ||
                strcmp(row->input_name, ast_call_semantic_value_type_name(row->argument)) != 0))) return false;
        bool header = false, typed = false;
        for (size_t c = 0; c < inputs->call_count; c++) if (inputs->calls[c].call == row->call) header = true;
        for (size_t d = 0; d < inputs->dependency_count; d++)
            for (size_t r = 0; r < inputs->dependencies[d].claims->count; r++) {
                const FunctionParamFlowIndexClaim *claim = &inputs->dependencies[d].claims->rows[r];
                if (claim->node == row->call && claim->id == row->call_id && claim->shape == AST_CALL &&
                    claim->semantic_promoted && claim->snapshot_semantic_complete && claim->value_ready &&
                    claim->result_relation && claim->result_depends && !claim->external_retains &&
                    claim->backing_relation && claim->backing_depends && !claim->backing_external_retains &&
                    claim->result_type_name != NULL && strcmp(claim->result_type_name, row->output_name) == 0) {
                    if (claim->child_count != 2 || claim->children == NULL || claim->child_ids == NULL ||
                        claim->child_claim_ordinals == NULL || claim->children[1] != row->argument ||
                        claim->child_ids[1] != row->argument_id ||
                        claim->child_claim_ordinals[1] >= inputs->dependencies[d].claims->count) return false;
                    const FunctionParamFlowIndexClaim *actual =
                        &inputs->dependencies[d].claims->rows[claim->child_claim_ordinals[1]];
                    if (actual->node != row->argument || actual->id != row->argument_id ||
                        actual->shape != row->argument_shape || actual->binding != row->argument_binding ||
                        !actual->semantic_promoted || !actual->snapshot_semantic_complete ||
                        !actual->value_ready || actual->value_meta) return false;
                    typed = true;
                }
            }
        const ASTNode *target = ast_call_callee(row->call);
        if (!header || !typed || target == NULL || target->type != AST_IDENTIFIER ||
            ast_identifier_name(target) == NULL || strcmp(ast_identifier_name(target), row->unwrap ? "UnwrapOption" : "Some") != 0)
            return false;
    }
    for (size_t i = 0; i < option->return_count; i++) {
        const IndexedStringOptionReturn *row = &option->returns[i];
        if (!option_id(row->caller, row->caller_id) || !option_id(row->statement, row->statement_id) ||
            !option_id(row->value, row->value_id) || row->statement->type != AST_RETURN ||
            ast_return_value(row->statement) != row->value) return false;
        if (row->none) {
            if (row->payload != NULL || row->payload_id != 0 || row->value->type != AST_IDENTIFIER ||
                ast_identifier_binding_syntax_id(row->value) != 0 ||
                pgy_match_variant_lookup(ast_identifier_name(row->value)) != PGY_MATCH_VARIANT_NONE_CTOR) return false;
        } else if (row->value->type != AST_CALL || !option_id(row->payload, row->payload_id) ||
            ast_call_argument(row->value, 0) != row->payload) return false;
        bool member = false, constant = !row->none;
        for (size_t d = 0; d < inputs->dependency_count; d++) if (inputs->dependencies[d].decl == row->caller) {
            for (size_t r = 0; r < inputs->dependencies[d].claims->count; r++) {
                if (inputs->dependencies[d].claims->rows[r].node == row->statement) {
                    member = true;
                }
                else if (row->none && inputs->dependencies[d].claims->rows[r].node == row->value) {
                    const FunctionParamFlowIndexClaim *claim = &inputs->dependencies[d].claims->rows[r];
                    constant = claim->id == row->value_id && claim->shape == AST_IDENTIFIER && claim->binding == 0 &&
                        claim->semantic_promoted && claim->snapshot_semantic_complete && claim->value_ready &&
                        claim->value_meta;
                }
            }
        }
        if (!member || !constant) return false;
    }
    for (size_t i = 0; i < option->use_count; i++) {
        const IndexedStringOptionUse *row = &option->uses[i];
        if (!option_id(row->caller, row->caller_id) || !option_id(row->sink, row->sink_id) ||
            !option_id(row->value, row->value_id) || row->sink->type != AST_CALL ||
            row->ordinal >= ast_call_arg_count(row->sink) || ast_call_argument(row->sink, row->ordinal) != row->value ||
            row->value->type != AST_IDENTIFIER || ast_identifier_binding_syntax_id(row->value) != row->binding)
            return false;
    }
    return true;
}

bool semantic_indexed_string_result_inputs_has_option(const IndexedStringResultInputs *inputs)
{ return inputs != NULL && inputs->option != NULL && semantic_indexed_string_result_inputs_syntax_current(inputs); }

bool semantic_indexed_string_result_inputs_source_write_count(const IndexedStringResultInputs *inputs, size_t *count)
{
    if (count == NULL) return false;
    *count = 0;
    if (inputs == NULL || inputs->option == NULL) return true;
    if (!semantic_indexed_string_result_inputs_syntax_current(inputs)) return false;
    *count = inputs->option->birth_count;
    return true;
}

bool semantic_indexed_string_result_inputs_source_value(const IndexedStringResultInputs *inputs,
    uint32_t caller, const ASTNode *value)
{
    if (inputs == NULL || inputs->option == NULL) return true;
    const IndexedStringOptionInputs *option = inputs->option;
    if (!option_input_seal_current(option)) return false;
    if (value == NULL) return true;
    for (size_t b = 0; b < option->birth_count; b++) {
        const IndexedStringOptionBirth *birth = &option->births[b];
        if (value->type == AST_IDENTIFIER && ast_identifier_binding_syntax_id(value) == birth->binding) {
            bool found = false;
            for (size_t u = 0; u < option->use_count; u++) if (option->uses[u].binding == birth->binding &&
                option->uses[u].value == value && option->uses[u].value_id == ast_node_stable_id(value) &&
                option->uses[u].caller_id == caller) found = true;
            if (!found || caller != birth->caller_id) return false;
        }
        if (option_same(value, birth->initializer, birth->initializer_id) &&
            (value != birth->initializer || !option_id(value, birth->initializer_id) || caller != birth->caller_id)) return false;
    }
    for (size_t i = 0; i < option->use_count; i++) if (option_same(value, option->uses[i].value, option->uses[i].value_id) &&
        (value != option->uses[i].value || !option_id(value, option->uses[i].value_id) ||
         caller != option->uses[i].caller_id || ast_identifier_binding_syntax_id(value) != option->uses[i].binding)) return false;
    for (size_t i = 0; i < option->return_count; i++) if (option_same(value, option->returns[i].value, option->returns[i].value_id) &&
        (value != option->returns[i].value || !option_id(value, option->returns[i].value_id) ||
         caller != option->returns[i].caller_id)) return false;
    return true;
}

bool semantic_indexed_string_result_inputs_source_use(const IndexedStringResultInputs *inputs,
    uint32_t caller, const ASTNode *sink, size_t ordinal, const ASTNode *value)
{
    if (inputs == NULL || inputs->option == NULL) return true;
    if (!semantic_indexed_string_result_inputs_source_value(inputs, caller, value)) return false;
    const IndexedStringOptionInputs *option = inputs->option;
    for (size_t b = 0; b < option->birth_count; b++)
        if (option_same(value, option->births[b].initializer, option->births[b].initializer_id)) return false;
    for (size_t r = 0; r < option->return_count; r++)
        if (option_same(value, option->returns[r].value, option->returns[r].value_id)) return false;
    bool tracked = false, exact = false;
    for (size_t i = 0; i < option->use_count; i++) {
        const IndexedStringOptionUse *row = &option->uses[i];
        bool slot = option_same(sink, row->sink, row->sink_id) && ordinal == row->ordinal;
        bool source = option_same(value, row->value, row->value_id);
        if (slot || source) tracked = true;
        if (slot && source && sink == row->sink && value == row->value && caller == row->caller_id) exact = true;
    }
    /* A new identifier with an existing binding is also a protected value. */
    for (size_t b = 0; value != NULL && value->type == AST_IDENTIFIER && b < option->birth_count; b++)
        if (ast_identifier_binding_syntax_id(value) == option->births[b].binding) tracked = true;
    return !tracked || exact;
}

bool semantic_indexed_string_result_inputs_source_evaluation(const IndexedStringResultInputs *inputs,
    uint32_t caller, const ASTNode *value)
{
    if (inputs == NULL || inputs->option == NULL) return true;
    if (!semantic_indexed_string_result_inputs_source_value(inputs, caller, value)) return false;
    for (size_t b = 0; value != NULL && b < inputs->option->birth_count; b++) {
        const IndexedStringOptionBirth *birth = &inputs->option->births[b];
        if ((value->type == AST_IDENTIFIER && ast_identifier_binding_syntax_id(value) == birth->binding) ||
            option_same(value, birth->initializer, birth->initializer_id)) return false;
    }
    for (size_t r = 0; r < inputs->option->return_count; r++)
        if (option_same(value, inputs->option->returns[r].value, inputs->option->returns[r].value_id)) return false;
    return true;
}

bool semantic_indexed_string_result_inputs_source_definition(const IndexedStringResultInputs *inputs,
    uint32_t caller, uint32_t binding, bool initialization, const ASTNode *statement,
    const ASTNode *place, const ASTNode *value, size_t *write_ordinal)
{
    if (write_ordinal != NULL) *write_ordinal = SIZE_MAX;
    if (inputs == NULL || inputs->option == NULL) return true;
    if (!option_input_seal_current(inputs->option)) return false;
    bool exact = false;
    for (size_t b = 0; b < inputs->option->birth_count; b++) {
        const IndexedStringOptionBirth *birth = &inputs->option->births[b];
        bool target = binding == birth->binding || option_same(statement, birth->declaration, birth->binding) ||
            (place != NULL && place->type == AST_IDENTIFIER && ast_identifier_binding_syntax_id(place) == birth->binding);
        bool rhs = option_same(value, birth->initializer, birth->initializer_id);
        if (!target && !rhs) continue;
        if (!target || !rhs || !initialization || caller != birth->caller_id || binding != birth->binding ||
            statement != birth->declaration || place != birth->declaration || value != birth->initializer ||
            !option_id(statement, birth->binding) || !option_id(value, birth->initializer_id)) return false;
        exact = true;
        if (write_ordinal != NULL) *write_ordinal = b;
    }
    return exact || semantic_indexed_string_result_inputs_source_evaluation(inputs, caller, value);
}

bool semantic_indexed_string_result_inputs_source_return(const IndexedStringResultInputs *inputs,
    uint32_t caller, const ASTNode *statement, const ASTNode *value)
{
    if (inputs == NULL || inputs->option == NULL) return true;
    if (!semantic_indexed_string_result_inputs_source_value(inputs, caller, value)) return false;
    bool producer = false, exact = false, protected_value = false;
    for (size_t r = 0; r < inputs->option->return_count; r++) {
        const IndexedStringOptionReturn *row = &inputs->option->returns[r];
        if (caller == row->caller_id) producer = true;
        if (option_same(value, row->value, row->value_id)) protected_value = true;
        if (caller == row->caller_id && statement == row->statement && value == row->value &&
            option_id(statement, row->statement_id) && option_id(value, row->value_id)) exact = true;
    }
    if (producer || protected_value) return exact;
    return semantic_indexed_string_result_inputs_source_evaluation(inputs, caller, value);
}
