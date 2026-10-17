/* An exact local String's independent definitions, carried by the existing
 * indexed-source owner. The recipe never changes its raw source or cleanup. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "indexed_string_definition_recipe.h"
#include "indexed_string_borrow_owner.h"
#include "function_param_publication_internal.h"
#include "diag_codes.h"
#include "type_checker_internal.h"
#include "../common/string_compat.h"

static bool definition_id(const ASTNode *node, uint32_t id)
{
    return node != NULL && id != 0 && ast_node_stable_id(node) == id;
}

static IndexedStringDefinitionRecipe *definition_for_binding(
    SemanticContext *ctx, uint32_t binding)
{
    FunctionParamPublicationStore *store = ctx == NULL ? NULL : ctx->function_param_publications;
    PublishedNode *row = store == NULL ? NULL : publication_node_by_id(store, binding);
    return row == NULL || row->id != binding ? NULL : row->indexed_string_definition;
}

static bool definition_birth_syntax(const IndexedStringDefinitionRecipe *recipe)
{
    return recipe != NULL && definition_id(recipe->declaration, recipe->declaration_id) &&
        recipe->declaration->type == AST_LET_DECL && !ast_let_is_alias(recipe->declaration) &&
        recipe->binding_id == recipe->declaration_id && recipe->binding_name != NULL &&
        ast_let_name(recipe->declaration) != NULL &&
        strcmp(recipe->binding_name, ast_let_name(recipe->declaration)) == 0 &&
        definition_id(recipe->caller, recipe->caller_id) && recipe->caller->type == AST_FUNC_DECL &&
        definition_id(recipe->body, recipe->body_id) && ast_func_body(recipe->caller) == recipe->body &&
        recipe->birth.value == ast_let_initializer(recipe->declaration) &&
        definition_id(recipe->birth.value, recipe->birth.value_id) &&
        recipe->birth.shape == recipe->birth.value->type;
}

static bool definition_birth_current(SemanticContext *ctx,
    const IndexedStringDefinitionRecipe *recipe)
{
    if (ctx == NULL || ctx->has_error || !definition_birth_syntax(recipe)) return false;
    PublishedNode *birth = publication_node_by_id(ctx->function_param_publications,
        recipe->binding_id);
    PublishedNode *value = publication_node(ctx->function_param_publications,
        recipe->birth.value, false);
    return birth != NULL && birth->node == recipe->declaration &&
        birth->id == recipe->declaration_id && birth->indexed_string_definition == recipe &&
        value != NULL && value->node == recipe->birth.value &&
        value->id == recipe->birth.value_id && value->caller == recipe->caller &&
        value->caller_id == recipe->caller_id && value->value_present &&
        value->value_shape == recipe->birth.shape && value->string_value &&
        type_equals(value->value_type, TYPE_STRING);
}

static bool definition_select(SemanticContext *ctx,
    IndexedStringDefinitionRecipe *recipe)
{
    if (recipe->selected || recipe->assignment_count == 0 || recipe->use_count == 0) return false;
    bool protected_use = false;
    for (size_t i = 0; i < recipe->use_count; i++)
        if (recipe->uses[i].protected_use) protected_use = true;
    if (!protected_use) return false;
    FunctionParamPublicationStore *store = ctx->function_param_publications;
    if (store->indexed_string_definition_count == SIZE_MAX) return false;
    for (size_t i = 0; i < recipe->use_count; i++) {
        const IndexedStringDefinitionUse *use = &recipe->uses[i];
        PublishedNode *row = publication_node(store, use->value, false);
        if (row == NULL || row->node != use->value || row->id != use->value_id ||
            (row->indexed_string_definition_use != NULL &&
             row->indexed_string_definition_use != recipe)) return false;
    }
    for (size_t i = 0; i < recipe->use_count; i++)
        publication_node(store, recipe->uses[i].value, false)->indexed_string_definition_use = recipe;
    recipe->selected = true;
    store->indexed_string_definition_count++;
    return true;
}

IndexedStringDefinitionRecord semantic_indexed_string_definition_record_birth(
    SemanticContext *ctx, const Symbol *binding, const ASTNode *declaration,
    const ASTNode *initializer)
{
    if (ctx == NULL || binding == NULL || declaration == NULL || initializer == NULL ||
        binding->is_parameter || binding->is_host_field ||
        !type_equals(binding->type, TYPE_STRING) || !semantic_param_publication_pending(ctx) ||
        ctx->has_error) return INDEXED_STRING_DEFINITION_NOT_APPLICABLE;
    if (declaration->type != AST_LET_DECL || ast_let_is_alias(declaration))
        return INDEXED_STRING_DEFINITION_NOT_APPLICABLE;
    if (ast_let_initializer(declaration) != initializer || ast_let_name(declaration) == NULL ||
        ctx->current_function_decl == NULL || ast_func_body(ctx->current_function_decl) == NULL ||
        binding->decl_syntax_id == 0 || binding->decl_syntax_id != ast_node_stable_id(declaration) ||
        ast_node_stable_id(initializer) == 0 ||
        ast_node_stable_id(ctx->current_function_decl) == 0 ||
        ast_node_stable_id(ast_func_body(ctx->current_function_decl)) == 0)
        return INDEXED_STRING_DEFINITION_FAILED;
    PublishedNode *row = publication_node(ctx->function_param_publications,
        declaration, true);
    if (row == NULL) return INDEXED_STRING_DEFINITION_FAILED;
    IndexedStringDefinitionRecipe *prior = row->indexed_string_definition;
    if (prior != NULL) return definition_birth_syntax(prior) &&
        prior->declaration == declaration && prior->birth.value == initializer &&
        prior->caller == ctx->current_function_decl ?
        INDEXED_STRING_DEFINITION_RECORDED : INDEXED_STRING_DEFINITION_FAILED;
    IndexedStringDefinitionRecipe *recipe = calloc(1, sizeof(*recipe));
    if (recipe == NULL) return INDEXED_STRING_DEFINITION_FAILED;
    recipe->declaration = declaration;
    recipe->declaration_id = binding->decl_syntax_id;
    recipe->binding_id = binding->decl_syntax_id;
    recipe->caller = ctx->current_function_decl;
    recipe->caller_id = ast_node_stable_id(ctx->current_function_decl);
    recipe->body = ast_func_body(ctx->current_function_decl);
    recipe->body_id = ast_node_stable_id(recipe->body);
    recipe->binding_name = pergyra_strdup(ast_let_name(declaration));
    recipe->birth.value = initializer;
    recipe->birth.value_id = ast_node_stable_id(initializer);
    recipe->birth.binding_id = ast_identifier_binding_syntax_id(initializer);
    recipe->birth.shape = initializer->type;
    if (recipe->binding_name == NULL || !definition_birth_syntax(recipe)) {
        semantic_indexed_string_definition_destroy(recipe);
        return INDEXED_STRING_DEFINITION_FAILED;
    }
    row->indexed_string_definition = recipe;
    return INDEXED_STRING_DEFINITION_RECORDED;
}

IndexedStringDefinitionRecord semantic_indexed_string_definition_record_assignment(
    SemanticContext *ctx, const Symbol *binding, const ASTNode *assignment,
    const ASTNode *rhs)
{
    if (ctx == NULL || binding == NULL || !semantic_param_publication_pending(ctx) ||
        ctx->has_error || !type_equals(binding->type, TYPE_STRING))
        return INDEXED_STRING_DEFINITION_NOT_APPLICABLE;
    IndexedStringDefinitionRecipe *recipe = definition_for_binding(ctx, binding->decl_syntax_id);
    if (recipe == NULL) return INDEXED_STRING_DEFINITION_NOT_APPLICABLE;
    const ASTNode *target = assignment == NULL ? NULL : ast_assignment_target(assignment);
    if (assignment == NULL || assignment->type != AST_ASSIGNMENT ||
        ast_assignment_value(assignment) != rhs || target == NULL ||
        target->type != AST_IDENTIFIER || ast_identifier_binding_syntax_id(target) != recipe->binding_id ||
        ast_node_stable_id(assignment) == 0 ||
        ast_node_stable_id(target) == 0 || ast_node_stable_id(rhs) == 0)
        return INDEXED_STRING_DEFINITION_FAILED;
    for (size_t i = 0; i < recipe->assignment_count; i++) {
        IndexedStringDefinitionAssignment *prior = &recipe->assignments[i];
        if (prior->assignment == assignment || prior->assignment_id == ast_node_stable_id(assignment))
            return prior->assignment == assignment && prior->target == target &&
                prior->rhs.value == rhs && prior->rhs.value_id == ast_node_stable_id(rhs) ?
                INDEXED_STRING_DEFINITION_RECORDED : INDEXED_STRING_DEFINITION_FAILED;
    }
    if (recipe->assignment_count == SIZE_MAX ||
        recipe->assignment_count + 1 > SIZE_MAX / sizeof(*recipe->assignments) ||
        recipe->prepared)
        return INDEXED_STRING_DEFINITION_FAILED;
    size_t count = recipe->assignment_count + 1;
    IndexedStringDefinitionAssignment *rows = realloc(recipe->assignments,
        count * sizeof(*rows));
    if (rows == NULL) return INDEXED_STRING_DEFINITION_FAILED;
    recipe->assignments = rows;
    rows[recipe->assignment_count++] = (IndexedStringDefinitionAssignment){
        .assignment = assignment, .target = target,
        .assignment_id = ast_node_stable_id(assignment), .target_id = ast_node_stable_id(target),
        .binding_id = recipe->binding_id,
        .rhs = {.value = rhs, .value_id = ast_node_stable_id(rhs),
            .binding_id = ast_identifier_binding_syntax_id(rhs), .shape = rhs->type}};
    recipe->assignment_capacity = count;
    return INDEXED_STRING_DEFINITION_RECORDED;
}

IndexedStringDefinitionRecord semantic_indexed_string_definition_record_use(
    SemanticContext *ctx, const ASTNode *value, const ASTNode *sink,
    size_t actual_ordinal)
{
    if (ctx == NULL || ctx->has_error || !semantic_param_publication_pending(ctx) ||
        value == NULL || value->type != AST_IDENTIFIER || sink == NULL || sink->type != AST_CALL)
        return INDEXED_STRING_DEFINITION_NOT_APPLICABLE;
    uint32_t binding = ast_identifier_binding_syntax_id(value);
    IndexedStringDefinitionRecipe *recipe = definition_for_binding(ctx, binding);
    if (recipe == NULL) return INDEXED_STRING_DEFINITION_NOT_APPLICABLE;
    PublishedNode *value_row = publication_node(ctx->function_param_publications,
        value, false);
    if (!definition_birth_syntax(recipe) || actual_ordinal >= ast_call_arg_count(sink) ||
        ast_call_argument(sink, actual_ordinal) != value ||
        ast_node_stable_id(value) == 0 || ast_node_stable_id(sink) == 0 ||
        value_row == NULL || value_row->node != value ||
        value_row->id != ast_node_stable_id(value) ||
        !value_row->indexed_string_source_present)
        return INDEXED_STRING_DEFINITION_FAILED;
    for (size_t i = 0; i < recipe->use_count; i++) {
        IndexedStringDefinitionUse *prior = &recipe->uses[i];
        if (prior->sink != sink || prior->actual_ordinal != actual_ordinal) continue;
        return prior->value == value && prior->value_id == ast_node_stable_id(value) &&
            prior->sink_id == ast_node_stable_id(sink) && prior->binding_id == binding &&
            prior->issued_source == value_row->indexed_string_source_id ?
            INDEXED_STRING_DEFINITION_RECORDED : INDEXED_STRING_DEFINITION_FAILED;
    }
    if (recipe->use_count == SIZE_MAX || recipe->use_count + 1 > SIZE_MAX / sizeof(*recipe->uses) ||
        recipe->prepared)
        return INDEXED_STRING_DEFINITION_FAILED;
    size_t count = recipe->use_count + 1;
    IndexedStringDefinitionUse *rows = realloc(recipe->uses, count * sizeof(*rows));
    if (rows == NULL) return INDEXED_STRING_DEFINITION_FAILED;
    recipe->uses = rows;
    rows[recipe->use_count++] = (IndexedStringDefinitionUse){value, sink,
        ast_node_stable_id(value), ast_node_stable_id(sink), binding,
        value_row->indexed_string_source_id, value->type, actual_ordinal, false, false};
    recipe->use_capacity = count;
    return INDEXED_STRING_DEFINITION_RECORDED;
}

bool semantic_indexed_string_definition_protect_use(SemanticContext *ctx,
    const ASTNode *value, const ASTNode *sink, size_t actual_ordinal,
    bool require_assignment)
{
    if (ctx == NULL || !semantic_param_publication_pending(ctx) ||
        value == NULL || sink == NULL || sink->type != AST_CALL ||
        actual_ordinal >= ast_call_arg_count(sink) ||
        ast_call_argument(sink, actual_ordinal) != value) return false;
    IndexedStringDefinitionRecipe *recipe = definition_for_binding(ctx,
        ast_identifier_binding_syntax_id(value));
    if (recipe == NULL || recipe->prepared ||
        (require_assignment && recipe->assignment_count == 0)) return false;
    for (size_t i = 0; i < recipe->use_count; i++) {
        IndexedStringDefinitionUse *use = &recipe->uses[i];
        if (use->value != value || use->sink != sink ||
            use->actual_ordinal != actual_ordinal) continue;
        if (use->value_id != ast_node_stable_id(value) ||
            use->sink_id != ast_node_stable_id(sink) ||
            use->binding_id != recipe->binding_id) return false;
        use->protected_candidate = true;
        return true;
    }
    return false;
}

bool semantic_indexed_string_definition_selected(SemanticContext *ctx,
    const ASTNode *value)
{
    if (value == NULL || value->type != AST_IDENTIFIER) return false;
    PublishedNode *row = ctx == NULL || ctx->function_param_publications == NULL ? NULL :
        publication_node(ctx->function_param_publications, value, false);
    IndexedStringDefinitionRecipe *recipe = definition_for_binding(ctx,
        ast_identifier_binding_syntax_id(value));
    return (row != NULL && row->indexed_string_definition_use != NULL) ||
        (recipe != NULL && recipe->selected);
}

static bool definition_source_syntax(const IndexedStringDefinitionSource *source)
{
    if (!definition_id(source->value, source->value_id) ||
        source->shape != source->value->type ||
        ast_identifier_binding_syntax_id(source->value) != source->binding_id) return false;
    if (source->alias_declaration != NULL) {
        if (!definition_id(source->alias_declaration, source->alias_declaration_id) ||
            source->alias_declaration->type != AST_LET_DECL ||
            ast_let_is_alias(source->alias_declaration) ||
            !definition_id(source->alias_initializer, source->alias_initializer_id) ||
            ast_let_initializer(source->alias_declaration) != source->alias_initializer ||
            source->alias_declaration_id != source->binding_id) return false;
    } else if (source->alias_initializer != NULL || source->alias_declaration_id != 0 ||
        source->alias_initializer_id != 0) return false;
    if (source->named_call != NULL) {
        if (!definition_id(source->named_call, source->named_call_id) ||
            source->named_call->type != AST_CALL ||
            (source->shape == AST_IDENTIFIER &&
             source->alias_initializer != source->named_call) ||
            (source->shape == AST_CALL && source->value != source->named_call) ||
            !semantic_indexed_string_result_inputs_syntax_current(&source->named_inputs)) return false;
    } else if (source->named_call_id != 0 || source->named_inputs.prepared) return false;
    if (source->independent_call_present &&
        (!indexed_string_field_call_syntax(&source->independent_call) ||
         source->independent_call.call != (source->shape == AST_IDENTIFIER ?
             source->alias_initializer : source->value))) return false;
    return true;
}

static bool definition_source_prepare(SemanticContext *ctx,
    IndexedStringDefinitionRecipe *recipe, IndexedStringDefinitionSource *source)
{
    if (source->value == NULL || source->value_id == 0 ||
        source->value->type != source->shape ||
        ast_node_stable_id(source->value) != source->value_id ||
        source->binding_id != ast_identifier_binding_syntax_id(source->value) ||
        source->binding_id == recipe->binding_id) return false;
    SemanticPublishedValue value = semantic_param_publication_value(ctx, source->value);
    if (!value.ok || value.meta || !type_equals(value.type, TYPE_STRING)) return false;
    const ASTNode *named = NULL;
    if (source->shape == AST_IDENTIFIER) {
        if (!semantic_indexed_string_borrow_result_dependency_current(ctx,
                (ASTNode *)source->value, &named)) return false;
        PublishedNode *alias = publication_node_by_id(ctx->function_param_publications,
            source->binding_id);
        if (alias == NULL || alias->node == NULL || alias->node->type != AST_LET_DECL ||
            alias->id != source->binding_id || ast_let_is_alias(alias->node) ||
            ast_let_initializer(alias->node) == NULL) return false;
        source->alias_declaration = alias->node;
        source->alias_declaration_id = alias->id;
        source->alias_initializer = ast_let_initializer(alias->node);
        source->alias_initializer_id = ast_node_stable_id(source->alias_initializer);
        /* A raw zero alias without an exact named or independent initializer
         * is not a transport proof for this new recipe. */
        if (named == NULL && !semantic_indexed_string_independent_result(
                source->alias_initializer, ctx)) return false;
    } else if (source->shape == AST_CALL) {
        if (!semantic_indexed_string_independent_result(source->value, ctx))
            named = source->value;
    } else if (source->shape != AST_STRING && source->shape != AST_BINARY) return false;
    if (named != NULL) {
        uint32_t result = UINT32_MAX;
        source->named_call = named;
        source->named_call_id = ast_node_stable_id(named);
        if (!semantic_indexed_string_result_source_prepare(ctx, named,
                &result, &source->named_inputs) || result != 0) return false;
    } else {
        const ASTNode *independent = source->shape == AST_IDENTIFIER ?
            source->alias_initializer : source->value;
        if (independent != NULL && independent->type == AST_CALL) {
            if (!indexed_string_field_call_capture(ctx, (ASTNode *)independent,
                    &source->independent_call)) return false;
            source->independent_call_present = true;
        }
    }
    uint32_t current = UINT32_MAX;
    return definition_source_syntax(source) &&
        semantic_indexed_string_borrow_source_current((ASTNode *)source->value,
            ctx, &current) && current == 0;
}

static bool definition_source_current(SemanticContext *ctx,
    const IndexedStringDefinitionSource *source)
{
    if (!definition_source_syntax(source)) return false;
    if (source->shape == AST_IDENTIFIER) {
        PublishedNode *alias = publication_node_by_id(ctx->function_param_publications,
            source->binding_id);
        if (alias == NULL || alias->node != source->alias_declaration ||
            alias->id != source->alias_declaration_id ||
            ast_let_initializer(alias->node) != source->alias_initializer) return false;
    }
    if (source->named_call != NULL) {
        uint32_t result = UINT32_MAX;
        if (!semantic_indexed_string_result_source_current(ctx, source->named_call,
                &result, (IndexedStringResultInputs *)&source->named_inputs) || result != 0)
            return false;
    }
    if (source->independent_call_present &&
        (!indexed_string_field_call_current(ctx, &source->independent_call) ||
         !semantic_indexed_string_independent_result(source->shape == AST_IDENTIFIER ?
            source->alias_initializer : source->value, ctx))) return false;
    uint32_t current = UINT32_MAX;
    return semantic_indexed_string_borrow_source_current((ASTNode *)source->value,
        ctx, &current) && current == 0;
}

static bool definition_prepare(SemanticContext *ctx,
    IndexedStringDefinitionRecipe *recipe)
{
    if (recipe == NULL || !recipe->selected || recipe->prepared || recipe->claims != NULL ||
        recipe->assignment_count == 0 || recipe->use_count == 0 ||
        !semantic_param_publication_inputs_ready(ctx) ||
        !definition_birth_current(ctx, recipe) ||
        !semantic_param_publication_routine_inputs_current(ctx, recipe->caller, true))
        return false;
    SlotFunctionLookup lookup = {ctx, ctx->program_root};
    FunctionParamFlowSummaryStore *store = function_param_flow_store_get(&lookup);
    FunctionParamFlowProgramPointIndex *index = NULL;
    if (store == NULL || store->failed || store->solving || store->validating_current ||
        !function_param_flow_program_point_index_build(store,
            (ASTNode *)recipe->caller, &index) || index == NULL ||
        index->function_decl != recipe->caller || index->function_id != recipe->caller_id ||
        !function_param_flow_index_roots_current(index) ||
        !function_param_flow_index_claims_current(ctx, index)) return false;
    recipe->claims = index->claims;
    if (!indexed_string_definition_claims_roles(ctx, recipe, true) ||
        !definition_source_prepare(ctx, recipe, &recipe->birth)) return false;
    for (size_t i = 0; i < recipe->assignment_count; i++)
        if (!definition_source_prepare(ctx, recipe,
            &recipe->assignments[i].rhs)) return false;
    for (size_t i = 0; i < recipe->use_count; i++) {
        PublishedNode *value = publication_node(ctx->function_param_publications,
            recipe->uses[i].value, false);
        if (value == NULL || value->node != recipe->uses[i].value ||
            value->id != recipe->uses[i].value_id ||
            (value->indexed_string_definition_use != NULL &&
             value->indexed_string_definition_use != recipe)) return false;
        value->indexed_string_definition_use = recipe;
    }
    recipe->sealed_assignments = recipe->assignment_count;
    recipe->sealed_uses = recipe->use_count;
    recipe->sealed_use_calls = recipe->use_call_count;
    recipe->prepared = true;
    return semantic_indexed_string_definition_syntax_current(recipe);
}

static bool definition_current(SemanticContext *ctx,
    IndexedStringDefinitionRecipe *recipe)
{
    if (recipe == NULL || !recipe->selected || !recipe->prepared ||
        recipe->validating_current || !definition_birth_current(ctx, recipe) ||
        recipe->assignment_count != recipe->sealed_assignments ||
        recipe->use_count != recipe->sealed_uses ||
        recipe->use_call_count != recipe->sealed_use_calls ||
        !semantic_indexed_string_definition_syntax_current(recipe)) return false;
    bool previous;
    if (!function_param_flow_current_begin(ctx, &previous)) return false;
    recipe->validating_current = true;
    bool current = false;
    FunctionParamFlowSummaryStore *store = ctx->function_param_flow_summaries;
    if (semantic_param_publication_routine_inputs_current(ctx, recipe->caller, true))
        for (size_t i = 0; i < store->program_point_index_count; i++) {
            FunctionParamFlowProgramPointIndex *index = &store->program_point_indexes[i];
            if (index->function_id != recipe->caller_id) continue;
            if (index->function_decl != recipe->caller || index->claims != recipe->claims ||
                !function_param_flow_index_roots_current(index) ||
                !function_param_flow_index_claims_current(ctx, index) ||
                !indexed_string_definition_claims_roles(ctx, recipe, false) ||
                !definition_source_current(ctx, &recipe->birth)) break;
            current = true;
            for (size_t a = 0; a < recipe->assignment_count; a++)
                if (!definition_source_current(ctx, &recipe->assignments[a].rhs)) {
                    current = false; break;
                }
            if (current) for (size_t u = 0; u < recipe->use_count; u++) {
                const IndexedStringDefinitionUse *use = &recipe->uses[u];
                PublishedNode *row = publication_node(ctx->function_param_publications,
                    use->value, false);
                if (row == NULL || row->node != use->value || row->id != use->value_id ||
                    row->indexed_string_definition_use != recipe ||
                    !row->indexed_string_source_present ||
                    row->indexed_string_source_id != use->issued_source ||
                    !semantic_param_publication_checked_argument(ctx, use->sink,
                        use->actual_ordinal)) { current = false; break; }
            }
            break;
        }
    recipe->validating_current = false;
    function_param_flow_current_end(ctx, previous);
    return current;
}

bool semantic_indexed_string_definition_source_current(SemanticContext *ctx,
    const ASTNode *value, uint32_t *source)
{
    if (ctx == NULL || ctx->has_error || value == NULL || source == NULL ||
        value->type != AST_IDENTIFIER || !semantic_param_publication_inputs_ready(ctx))
        return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, value, false);
    IndexedStringDefinitionRecipe *recipe = definition_for_binding(ctx,
        ast_identifier_binding_syntax_id(value));
    if (row == NULL || recipe == NULL || row->indexed_string_definition_use != recipe ||
        !recipe->selected || !definition_current(ctx, recipe)) return false;
    *source = 0;
    return true;
}

bool semantic_indexed_string_definition_syntax_current(
    const IndexedStringDefinitionRecipe *recipe)
{
    if (recipe == NULL || !recipe->selected || !recipe->prepared ||
        !definition_birth_syntax(recipe) || recipe->claims == NULL ||
        recipe->assignment_count == 0 || recipe->use_count == 0 ||
        recipe->assignment_count != recipe->sealed_assignments ||
        recipe->use_count != recipe->sealed_uses ||
        recipe->use_call_count != recipe->sealed_use_calls ||
        recipe->assignment_count > recipe->assignment_capacity ||
        recipe->use_count > recipe->use_capacity ||
        recipe->use_call_count > recipe->use_call_capacity ||
        (recipe->assignment_count != 0 && recipe->assignments == NULL) ||
        (recipe->use_count != 0 && recipe->uses == NULL) ||
        (recipe->use_call_count != 0 && recipe->use_calls == NULL) ||
        !function_param_flow_syntax_claims_current(NULL, recipe->claims) ||
        !definition_source_syntax(&recipe->birth)) return false;
    for (size_t a = 0; a < recipe->assignment_count; a++) {
        const IndexedStringDefinitionAssignment *row = &recipe->assignments[a];
        if (!definition_id(row->assignment, row->assignment_id) ||
            row->assignment->type != AST_ASSIGNMENT ||
            !definition_id(row->target, row->target_id) ||
            row->target->type != AST_IDENTIFIER ||
            row->binding_id != recipe->binding_id ||
            ast_identifier_binding_syntax_id(row->target) != row->binding_id ||
            ast_assignment_target(row->assignment) != row->target ||
            ast_assignment_value(row->assignment) != row->rhs.value ||
            !definition_source_syntax(&row->rhs)) return false;
        for (size_t p = 0; p < a; p++)
            if (recipe->assignments[p].assignment == row->assignment) return false;
    }
    for (size_t u = 0; u < recipe->use_count; u++) {
        const IndexedStringDefinitionUse *row = &recipe->uses[u];
        if (!definition_id(row->value, row->value_id) ||
            !definition_id(row->sink, row->sink_id) ||
            row->value->type != AST_IDENTIFIER || row->value_shape != AST_IDENTIFIER ||
            row->sink->type != AST_CALL || row->binding_id != recipe->binding_id ||
            ast_identifier_binding_syntax_id(row->value) != row->binding_id ||
            row->actual_ordinal >= ast_call_arg_count(row->sink) ||
            ast_call_argument(row->sink, row->actual_ordinal) != row->value) return false;
        for (size_t p = 0; p < u; p++)
            if (recipe->uses[p].sink == row->sink &&
                recipe->uses[p].actual_ordinal == row->actual_ordinal) return false;
    }
    for (size_t c = 0; c < recipe->use_call_count; c++) {
        if (!indexed_string_field_call_syntax(&recipe->use_calls[c])) return false;
        for (size_t p = 0; p < c; p++)
            if (recipe->use_calls[p].call == recipe->use_calls[c].call) return false;
    }
    return indexed_string_definition_claims_roles(NULL,
        (IndexedStringDefinitionRecipe *)recipe, false);
}

void semantic_indexed_string_definition_destroy(IndexedStringDefinitionRecipe *recipe)
{
    if (recipe == NULL) return;
    semantic_indexed_string_result_inputs_destroy(&recipe->birth.named_inputs);
    if (recipe->birth.independent_call_present)
        indexed_string_field_call_destroy(&recipe->birth.independent_call);
    for (size_t a = 0; a < recipe->assignment_count; a++) {
        semantic_indexed_string_result_inputs_destroy(&recipe->assignments[a].rhs.named_inputs);
        if (recipe->assignments[a].rhs.independent_call_present)
            indexed_string_field_call_destroy(&recipe->assignments[a].rhs.independent_call);
    }
    for (size_t c = 0; c < recipe->use_call_count; c++)
        indexed_string_field_call_destroy(&recipe->use_calls[c]);
    free(recipe->use_calls);
    free(recipe->assignments);
    free(recipe->uses);
    free(recipe->binding_name);
    free(recipe);
}

static ASTNode *definition_protected_use_site(const IndexedStringDefinitionRecipe *recipe)
{
    const ASTNode *candidate = NULL;
    for (size_t i = 0; i < recipe->use_count; i++) {
        const IndexedStringDefinitionUse *use = &recipe->uses[i];
        if (use->protected_use) return (ASTNode *)use->value;
        if (candidate == NULL && use->protected_candidate) candidate = use->value;
    }
    return (ASTNode *)(candidate != NULL ? candidate : recipe->declaration);
}

bool semantic_indexed_string_definition_receipts(SemanticContext *ctx, bool prepare)
{
    if (ctx == NULL || ctx->has_error || !semantic_param_publication_ready(ctx)) return false;
    FunctionParamPublicationStore *store = ctx->function_param_publications;
    size_t issued = 0;
    for (size_t i = 0; i < store->node_capacity; i++) {
        IndexedStringDefinitionRecipe *recipe = store->nodes[i].indexed_string_definition;
        if (recipe == NULL) continue;
        if (prepare && !recipe->selected && recipe->assignment_count != 0 &&
            recipe->use_count != 0) {
            bool protected_use = false;
            if (!indexed_string_definition_classify_uses(ctx, recipe, &protected_use) ||
                (protected_use && !definition_select(ctx, recipe))) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                    PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                    definition_protected_use_site(recipe),
                    "A String use has no current retention classification");
                return false;
            }
        }
        if (!recipe->selected) continue;
        if (issued == SIZE_MAX || (prepare && !definition_prepare(ctx, recipe)) ||
            !definition_current(ctx, recipe)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                definition_protected_use_site(recipe),
                "A String definition has no current independent source proof");
            return false;
        }
        issued++;
    }
    if (issued == store->indexed_string_definition_count) return true;
    semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
        PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
        ctx->program_root, "A String definition receipt was withdrawn");
    return false;
}

