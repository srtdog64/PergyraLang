#include "indexed_string_field_lineage_internal.h"

#include <stdlib.h>
#include <string.h>

#include "function_param_publication_internal.h"
#include "loan_copy_landing_owner.h"
#include "type_checker_internal.h"
#include "diag_codes.h"

/* A recipe carries the original owners' receipts, not a new String source.
 * Literal leaves are the only independence admitted by this bounded chain. */
static bool field_id(const ASTNode *node, uint32_t id)
{ return node != NULL && id != 0 && ast_node_stable_id(node) == id; }

static bool field_projection_same(const PublishedProjection *left,
    const PublishedProjection *right)
{
    /* Borrowed strings cannot be read until their issuing row still owns them. */
    return left->present && right->present && left->member == right->member &&
        left->name == right->name &&
        left->field_type_name == right->field_type_name &&
        left->declaration_name == right->declaration_name &&
        left->root_name == right->root_name;
}


static bool field_result_append(IndexedStringFieldRecipe *recipe,
    IndexedStringFieldReturn result)
{
    if (recipe->return_count == SIZE_MAX ||
        recipe->return_count + 1 > SIZE_MAX / sizeof(*recipe->returns)) return false;
    size_t count = recipe->return_count + 1;
    IndexedStringFieldReturn *rows = realloc(recipe->returns,
        count * sizeof(*rows));
    if (rows == NULL) return false;
    recipe->returns = rows;
    rows[recipe->return_count] = result;
    recipe->return_count = count;
    recipe->return_capacity = count;
    return true;
}

static bool field_literal_current(SemanticContext *ctx, const ASTNode *literal)
{
    if (literal == NULL || literal->type != AST_STRING ||
        ast_node_stable_id(literal) == 0 || ast_string_value(literal) == NULL) return false;
    SemanticPublishedValue value = semantic_param_publication_value(ctx, literal);
    return value.ok && !value.meta && value.type == TYPE_STRING;
}


static bool field_direct_definition(const FunctionParamFlowProgramPointIndex *index,
    const IndexedStringFieldRecipe *recipe)
{
    size_t found = 0;
    for (size_t i = 0; i < index->local_count; i++) {
        ASTNode *node = index->local_definitions[i];
        if (node == NULL || node->type != AST_LET_DECL ||
            index->local_ids[i] != recipe->binding_id ||
            ast_node_stable_id(node) != recipe->binding_id) continue;
        if (++found != 1 || node != recipe->definition ||
            ast_let_is_alias(node) || ast_let_is_mutable(node) ||
            ast_let_initializer(node) != recipe->producer ||
            ast_let_name(node) == NULL ||
            strcmp(ast_let_name(node), ast_identifier_name(recipe->root)) != 0)
            return false;
    }
    return found == 1;
}

static bool field_source_for_return(SemanticContext *ctx,
    IndexedStringFieldRecipe *recipe, ASTNode *statement, ASTNode *constructor,
    ASTNode *source, size_t actual, SemanticNominalResultSnapshot *mapping,
    bool prepare, size_t return_at)
{
    ASTNode *input = source;
    size_t formal = SIZE_MAX;
    uint32_t formal_id = 0;
    if (source->type == AST_IDENTIFIER) {
        uint32_t binding = ast_identifier_binding_syntax_id(source);
        size_t matches = 0;
        for (size_t i = 0; i < ast_func_param_count(recipe->producer_decl); i++) {
            FuncParam *p = ast_func_param(recipe->producer_decl, i);
            if (p == NULL || ast_func_param_stable_id(p) != binding) continue;
            if (++matches != 1 || (p->mode != PARAM_MODE_DEFAULT &&
                p->mode != PARAM_MODE_REF)) return false;
            formal = i; formal_id = binding;
        }
        if (matches != 1) return false;
        input = NULL;
        for (size_t i = 0; i < ast_call_arg_count(recipe->producer); i++) {
            size_t mapped = SIZE_MAX;
            if (!semantic_param_publication_formal_for_actual(ctx,
                    recipe->producer, i, &mapped)) return false;
            if (mapped != formal) continue;
            if (input != NULL || i != formal) return false;
            input = ast_call_argument(recipe->producer, i);
        }
    }
    if (!field_literal_current(ctx, input) ||
        (source->type != AST_IDENTIFIER && source->type != AST_STRING)) return false;
    IndexedStringFieldReturn row = {statement, constructor, source, input,
        ast_node_stable_id(statement), ast_node_stable_id(constructor),
        ast_node_stable_id(source), ast_node_stable_id(input), actual,
        formal, formal_id, mapping};
    if (row.statement_id == 0 || row.constructor_id == 0 ||
        row.source_id == 0 || row.input_id == 0 || mapping == NULL) return false;
    if (prepare) return field_result_append(recipe, row);
    if (return_at >= recipe->return_count) return false;
    const IndexedStringFieldReturn *saved = &recipe->returns[return_at];
    return saved->statement == row.statement && saved->constructor == row.constructor &&
        saved->source == row.source && saved->input == row.input &&
        saved->statement_id == row.statement_id &&
        saved->constructor_id == row.constructor_id &&
        saved->source_id == row.source_id && saved->input_id == row.input_id &&
        saved->constructor_actual == row.constructor_actual &&
        saved->formal_ordinal == row.formal_ordinal &&
        saved->formal_id == row.formal_id && saved->mapping == row.mapping;
}

static bool field_returns_current(SemanticContext *ctx,
    IndexedStringFieldRecipe *recipe, const FunctionParamFlowIndexClaims *claims,
    bool prepare)
{
    size_t reached = 0;
    for (size_t i = 0; i < claims->count; i++) {
        const FunctionParamFlowIndexClaim *row = &claims->rows[i];
        if (row->shape != AST_RETURN) continue;
        bool reachable = false;
        if (!row->execution_present || !semantic_param_publication_execution(ctx,
                row->node, &reachable) || row->reachable != reachable) return false;
        if (!reachable) continue;
        ASTNode *constructor = ast_return_value(row->node);
        if (constructor == NULL || constructor->type != AST_CALL ||
            !semantic_param_publication_nominal_result_complete(ctx, constructor)) return false;
        ASTNode *source = NULL;
        size_t actual = SIZE_MAX;
        SemanticPublishedSelectedField selected = {.member = recipe->value,
            .member_id = recipe->value_id,
            .declaration = recipe->projection.declaration,
            .declaration_id = recipe->projection.declaration_id,
            .field_id = recipe->projection.field_id,
            .field_type_node = recipe->projection.field_type_node,
            .field_type_id = recipe->projection.field_type_id,
            .field_ordinal = recipe->projection.field_ordinal,
            .field_count = recipe->projection.field_count,
            .nominal_type = recipe->projection.receiver_type,
            .field_type = recipe->projection.field_type};
        if (!semantic_param_publication_constructor_field_actual_current(ctx,
                constructor, &selected, &source, &actual)) return false;
        PublishedNode *published = publication_node(ctx->function_param_publications,
            constructor, false);
        if (published == NULL || published->nominal_result == NULL ||
            !field_source_for_return(ctx, recipe, row->node, constructor,
                source, actual, published->nominal_result, prepare, reached)) return false;
        reached++;
    }
    return reached != 0 && (prepare ? reached == recipe->return_count :
        reached == recipe->sealed_returns);
}

bool semantic_indexed_string_field_record(SemanticContext *ctx, ASTNode *value,
    ASTNode *sink, size_t ordinal)
{
    if (ctx == NULL || ctx->has_error || !semantic_param_publication_pending(ctx) ||
        value == NULL || value->type != AST_MEMBER_ACCESS || sink == NULL ||
        sink->type != AST_CALL || ordinal >= ast_call_arg_count(sink) ||
        ast_call_argument(sink, ordinal) != value ||
        ast_node_stable_id(sink) == 0) return false;
    FunctionParamPublicationStore *store = ctx->function_param_publications;
    PublishedNode *node = publication_node(store, value, false);
    PublishedNode *definition = node == NULL ? NULL :
        publication_node_by_id(store, node->projection.root_binding_id);
    ASTNode *let = definition == NULL ? NULL : (ASTNode *)definition->node;
    ASTNode *producer = let != NULL && let->type == AST_LET_DECL ?
        ast_let_initializer(let) : NULL;
    PublishedNode *call = publication_node(store, producer, false);
    const char *name = node == NULL || node->projection.root == NULL ||
        node->projection.root->type != AST_IDENTIFIER ? NULL :
        ast_identifier_name(node->projection.root);
    Symbol *symbol = name == NULL ? NULL : scope_lookup(ctx->scope, name);
    if (node == NULL ||
        node->caller != ctx->current_function_decl ||
        !semantic_param_publication_member_projection_syntax_current(value,
            node->caller, &node->projection) ||
        node->projection.result_type != TYPE_STRING ||
        node->projection.root_binding_id == 0 ||
        definition == NULL || !definition->execution_present || !definition->reachable ||
        let == NULL || let->type != AST_LET_DECL ||
        ast_node_stable_id(let) != node->projection.root_binding_id ||
        ast_let_is_alias(let) || ast_let_is_mutable(let) ||
        producer == NULL || producer->type != AST_CALL ||
        call == NULL || !call->value_present || call->meta || call->value_type == NULL ||
        !type_equals(call->value_type, node->projection.receiver_type) ||
        call->caller != node->caller || call->caller_id != node->caller_id ||
        call->selected_callable == NULL ||
        call->selected_callable->type != AST_FUNC_DECL ||
        ast_func_body(call->selected_callable) == NULL ||
        !ast_call_semantic_callee_declared_callable(producer) ||
        call->selected_callable_id != ast_call_semantic_callee_decl_id(producer) ||
        symbol == NULL || symbol->decl_syntax_id != node->projection.root_binding_id ||
        symbol->is_parameter || symbol->is_host_field ||
        ast_let_name(let) == NULL || symbol->name == NULL ||
        strcmp(ast_let_name(let), symbol->name) != 0 ||
        !type_equals(symbol->type, node->projection.receiver_type)) return false;
    if (node->indexed_string_field != NULL) {
        IndexedStringFieldRecipe *prior = node->indexed_string_field;
        return prior->issued && prior->value == value && prior->sink == sink &&
            prior->sink_ordinal == ordinal && prior->caller == node->caller &&
            prior->root == node->projection.root &&
            prior->binding_id == node->projection.root_binding_id &&
            field_projection_same(&prior->projection, &node->projection);
    }
    if (store->indexed_string_field_count == SIZE_MAX) return false;
    IndexedStringFieldRecipe *recipe = calloc(1, sizeof(*recipe));
    if (recipe == NULL) return false;
    recipe->value = value; recipe->value_id = ast_node_stable_id(value);
    recipe->sink = sink; recipe->sink_id = ast_node_stable_id(sink);
    recipe->sink_ordinal = ordinal;
    recipe->caller = node->caller; recipe->caller_id = node->caller_id;
    recipe->root = node->projection.root;
    recipe->root_id = node->projection.root_id;
    recipe->binding_id = node->projection.root_binding_id;
    recipe->projection = node->projection; /* borrowed until the snapshot take */
    recipe->issued = true;
    node->indexed_string_field = recipe;
    store->indexed_string_field_count++;
    return true;
}

static bool field_live_metadata(SemanticContext *ctx,
    const IndexedStringFieldRecipe *recipe)
{
    if (recipe->return_count > recipe->return_capacity ||
        (recipe->return_count != 0 && recipe->returns == NULL)) return false;
    FunctionParamPublicationStore *store = ctx->function_param_publications;
    PublishedNode *node = publication_node(store, recipe->value, false);
    if (node == NULL || node->indexed_string_field != recipe ||
        !field_projection_same(&node->projection, &recipe->projection) ||
        node->caller != recipe->caller || node->caller_id != recipe->caller_id)
        return false;
    for (size_t i = 0; i < recipe->return_count; i++) {
        const IndexedStringFieldReturn *row = &recipe->returns[i];
        PublishedNode *call = publication_node(store, row->constructor, false);
        if (call == NULL || call->nominal_result != row->mapping) return false;
    }
    return true;
}

bool semantic_indexed_string_field_syntax_current(const IndexedStringFieldRecipe *recipe)
{
    if (recipe == NULL || !recipe->issued ||
        !field_id(recipe->value, recipe->value_id) ||
        !field_id(recipe->sink, recipe->sink_id) ||
        !field_id(recipe->caller, recipe->caller_id) ||
        !field_id(recipe->root, recipe->root_id) ||
        !field_id(recipe->definition, recipe->definition_id) ||
        !field_id(recipe->producer, recipe->producer_id) ||
        !field_id(recipe->producer_decl, recipe->producer_decl_id) ||
        recipe->sink->type != AST_CALL ||
        recipe->sink_ordinal >= ast_call_arg_count(recipe->sink) ||
        ast_call_argument(recipe->sink, recipe->sink_ordinal) != recipe->value ||
        recipe->binding_id == 0 || recipe->definition_id != recipe->binding_id ||
        recipe->definition->type != AST_LET_DECL ||
        ast_let_is_alias(recipe->definition) || ast_let_is_mutable(recipe->definition) ||
        ast_let_name(recipe->definition) == NULL || recipe->projection.root_name == NULL ||
        strcmp(ast_let_name(recipe->definition), recipe->projection.root_name) != 0 ||
        ast_let_initializer(recipe->definition) != recipe->producer ||
        recipe->producer->type != AST_CALL ||
        recipe->producer_decl->type != AST_FUNC_DECL ||
        ast_func_body(recipe->producer_decl) == NULL ||
        recipe->projection.root_binding_id != recipe->binding_id ||
        !semantic_param_publication_member_projection_syntax_current(
            recipe->value, recipe->caller, &recipe->projection) ||
        recipe->dependency_count == 0 ||
        recipe->dependency_count > recipe->dependency_capacity ||
        recipe->sealed_dependencies != recipe->dependency_count ||
        recipe->dependencies == NULL || recipe->return_count == 0 ||
        recipe->return_count > recipe->return_capacity ||
        recipe->sealed_returns != recipe->return_count || recipe->returns == NULL ||
        !indexed_string_field_flow_syntax_current(recipe))
        return false;
    for (size_t i = 0; i < recipe->dependency_count; i++) {
        const IndexedStringFieldDependency *row = &recipe->dependencies[i];
        if (!field_id(row->decl, row->decl_id) ||
            !field_id(row->body, row->body_id) ||
            ast_func_body(row->decl) != row->body || row->claims == NULL ||
            row->decl->type != AST_FUNC_DECL) return false;
        for (size_t j = 0; j < i; j++)
            if (recipe->dependencies[j].decl_id == row->decl_id) return false;
    }
    for (size_t i = 0; i < recipe->return_count; i++) {
        const IndexedStringFieldReturn *row = &recipe->returns[i];
        if (!field_id(row->statement, row->statement_id) ||
            !field_id(row->constructor, row->constructor_id) ||
            !field_id(row->source, row->source_id) ||
            !field_id(row->input, row->input_id) ||
            row->statement->type != AST_RETURN ||
            row->constructor->type != AST_CALL ||
            ast_return_value(row->statement) != row->constructor ||
            row->constructor_actual >= ast_call_arg_count(row->constructor) ||
            ast_call_argument(row->constructor, row->constructor_actual) != row->source ||
            row->input->type != AST_STRING || ast_string_value(row->input) == NULL ||
            row->mapping == NULL || row->mapping->decl != recipe->projection.declaration ||
            row->mapping->decl_id != recipe->projection.declaration_id ||
            row->mapping->field_count != recipe->projection.field_count ||
            row->constructor_actual >= row->mapping->field_count ||
            row->mapping->fields == NULL ||
            row->mapping->fields[row->constructor_actual].field_index !=
                recipe->projection.field_ordinal ||
            row->mapping->fields[row->constructor_actual].field_id !=
                recipe->projection.field_id ||
            row->mapping->fields[row->constructor_actual].type_node !=
                recipe->projection.field_type_node ||
            row->mapping->fields[row->constructor_actual].type_id !=
                recipe->projection.field_type_id ||
            !semantic_nominal_result_snapshot_syntax_current(row->constructor,
                row->mapping)) return false;
        if (row->source->type == AST_IDENTIFIER) {
            if (row->formal_ordinal >= ast_func_param_count(recipe->producer_decl) ||
                row->formal_id == 0 ||
                ast_identifier_binding_syntax_id(row->source) != row->formal_id ||
                ast_func_param_stable_id(ast_func_param(recipe->producer_decl,
                    row->formal_ordinal)) != row->formal_id ||
                row->formal_ordinal >= ast_call_arg_count(recipe->producer) ||
                ast_call_argument(recipe->producer, row->formal_ordinal) != row->input)
                return false;
        } else if (row->source->type != AST_STRING ||
            row->source != row->input || row->formal_id != 0 ||
            row->formal_ordinal != SIZE_MAX) return false;
    }
    return true;
}

static bool field_check(SemanticContext *ctx,
    IndexedStringFieldRecipe *recipe, bool prepare)
{
    ASTNode *caller = NULL, *root = NULL;
    SemanticPublishedSelectedField field = {0};
    if (!semantic_param_publication_checked_argument(ctx, recipe->sink,
            recipe->sink_ordinal) ||
        !indexed_string_field_flow_call_current(ctx, recipe, (ASTNode *)recipe->sink,
            prepare) ||
        !semantic_param_publication_selected_member_field_current(ctx,
            recipe->value, &caller, &root, &field) ||
        caller != recipe->caller || root != recipe->root ||
        field.declaration != recipe->projection.declaration ||
        field.declaration_id != recipe->projection.declaration_id ||
        field.field_id != recipe->projection.field_id ||
        field.field_ordinal != recipe->projection.field_ordinal ||
        field.field_count != recipe->projection.field_count ||
        field.field_type_node != recipe->projection.field_type_node ||
        field.field_type_id != recipe->projection.field_type_id ||
        field.field_type != TYPE_STRING ||
        field.nominal_type != recipe->projection.receiver_type)
        return false;
    FunctionParamFlowProgramPointIndex *caller_index = indexed_string_field_flow_index(ctx, caller);
    if (caller_index == NULL) return false;
    if (prepare) {
        for (size_t i = 0; i < caller_index->local_count; i++) {
            ASTNode *node = caller_index->local_definitions[i];
            if (node != NULL && node->type == AST_LET_DECL &&
                caller_index->local_ids[i] == recipe->binding_id &&
                ast_node_stable_id(node) == recipe->binding_id) {
                recipe->definition = node;
                recipe->definition_id = ast_node_stable_id(node);
                recipe->producer = ast_let_initializer(node);
                recipe->producer_id = ast_node_stable_id(recipe->producer);
                break;
            }
        }
    }
    if (!field_direct_definition(caller_index, recipe) ||
        recipe->producer == NULL || recipe->producer->type != AST_CALL ||
        !semantic_param_publication_checked_call(ctx, recipe->producer) ||
        !indexed_string_field_flow_call_current(ctx, recipe, (ASTNode *)recipe->producer,
            prepare) ||
        (prepare && !indexed_string_field_flow_dependency(recipe, caller_index))) return false;
    const Type *result = NULL;
    ASTNode *producer_decl = semantic_param_publication_declared_result(ctx,
        recipe->producer, &result);
    if (producer_decl == NULL || producer_decl->is_async_decl ||
        !type_equals(result, field.nominal_type) ||
        ast_call_arg_count(recipe->producer) != ast_func_param_count(producer_decl))
        return false;
    if (prepare) {
        recipe->producer_decl = producer_decl;
        recipe->producer_decl_id = ast_node_stable_id(producer_decl);
    } else if (producer_decl != recipe->producer_decl ||
        ast_node_stable_id(producer_decl) != recipe->producer_decl_id) return false;
    FunctionParamFlowProgramPointIndex *producer_index = indexed_string_field_flow_index(ctx,
        producer_decl);
    if (producer_index == NULL ||
        (prepare && !indexed_string_field_flow_dependency(recipe, producer_index)) ||
        !field_returns_current(ctx, recipe, producer_index->claims, prepare))
        return false;
    for (size_t i = 0; i < recipe->return_count; i++) {
        const IndexedStringFieldReturn *row = &recipe->returns[i];
        if (row->source->type == AST_IDENTIFIER &&
            !indexed_string_field_flow_uses_current(ctx, producer_index->claims,
                row->formal_id, false, recipe, prepare)) return false;
    }
    caller_index = indexed_string_field_flow_index(ctx, caller);
    if (caller_index == NULL || !indexed_string_field_flow_uses_current(ctx, caller_index->claims,
            recipe->binding_id, true, recipe, prepare)) return false;
    return true;
}

bool semantic_indexed_string_field_prepare(SemanticContext *ctx,
    IndexedStringFieldRecipe *recipe)
{
    if (ctx == NULL || ctx->has_error || recipe == NULL || !recipe->issued ||
        !semantic_param_publication_inputs_ready(ctx) ||
        function_param_flow_store_get(&(SlotFunctionLookup){ctx, ctx->program_root}) == NULL ||
        ctx->function_param_flow_summaries->solving ||
        ctx->function_param_flow_summaries->failed ||
        recipe->dependency_count != 0 || recipe->return_count != 0 ||
        recipe->call_count != 0 ||
        recipe->sealed_dependencies != 0 || recipe->sealed_returns != 0 ||
        recipe->sealed_calls != 0 ||
        !field_live_metadata(ctx, recipe) || !field_check(ctx, recipe, true))
        return false;
    recipe->sealed_dependencies = recipe->dependency_count;
    recipe->sealed_returns = recipe->return_count;
    recipe->sealed_calls = recipe->call_count;
    return semantic_indexed_string_field_syntax_current(recipe);
}

bool semantic_indexed_string_field_current(SemanticContext *ctx,
    const IndexedStringFieldRecipe *recipe)
{
    if (ctx == NULL || ctx->has_error || recipe == NULL ||
        !semantic_param_publication_inputs_ready(ctx) ||
        !field_live_metadata(ctx, recipe) ||
        !semantic_indexed_string_field_syntax_current(recipe)) return false;
    bool previous = false;
    if (!function_param_flow_current_begin(ctx, &previous)) return false;
    bool current = field_check(ctx, (IndexedStringFieldRecipe *)recipe, false);
    FunctionParamFlowSummaryStore *store = ctx->function_param_flow_summaries;
    for (size_t i = 0; current && i < recipe->dependency_count; i++) {
        const IndexedStringFieldDependency *row = &recipe->dependencies[i];
        FunctionParamFlowProgramPointIndex *index = indexed_string_field_flow_index(ctx,
            (ASTNode *)row->decl);
        current = index != NULL && index->function_id == row->decl_id &&
            index->dependency_body == row->body &&
            index->dependency_body_id == row->body_id && index->claims == row->claims &&
            store != NULL && !store->failed;
    }
    function_param_flow_current_end(ctx, previous);
    return current;
}

void semantic_indexed_string_field_destroy(IndexedStringFieldRecipe *recipe)
{
    if (recipe == NULL) return;
    free(recipe->dependencies);
    free(recipe->returns);
    indexed_string_field_flow_destroy(recipe);
    free(recipe);
}

bool semantic_indexed_string_field_source_current(SemanticContext *ctx,
    ASTNode *value, uint32_t *source_id)
{
    if (source_id == NULL) return false;
    *source_id = UINT32_MAX;
    PublishedNode *row = ctx == NULL ? NULL : publication_node(ctx->function_param_publications, value, false);
    if (row == NULL || row->indexed_string_field == NULL ||
        !semantic_indexed_string_field_current(ctx, row->indexed_string_field)) return false;
    *source_id = 0; /* An issued independent literal-field recipe, not UNKNOWN. */
    return true;
}

bool semantic_indexed_string_field_receipts(SemanticContext *ctx, bool prepare)
{
    if (ctx == NULL || ctx->has_error || !semantic_param_publication_ready(ctx)) return false;
    FunctionParamPublicationStore *store = ctx->function_param_publications;
    size_t issued = 0;
    ASTNode *first_copy = NULL;
    for (size_t i = 0; i < store->node_capacity; i++) {
        PublishedNode *row = &store->nodes[i];
        if (row->indexed_string_field == NULL) continue;
        if (issued == SIZE_MAX) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                (ASTNode *)row->node, "String field source recipe count overflowed");
            return false;
        }
        bool prepared = !prepare ||
            semantic_indexed_string_field_prepare(ctx, row->indexed_string_field);
        if (!prepared && prepare && row->node != NULL && row->id != 0 &&
            row->id == ast_node_stable_id(row->node) && row->value_present &&
            !row->meta && row->string_value && type_equals(row->value_type, TYPE_STRING) &&
            semantic_loan_copy_landing_select_string(ctx, row->node)) {
            if (first_copy == NULL) first_copy = (ASTNode *)row->node;
            continue;
        }
        if (!prepared ||
            !semantic_indexed_string_field_current(ctx, row->indexed_string_field)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                (ASTNode *)row->node, "A String field has no current independent-source recipe");
            return false;
        }
        issued++;
    }
    if (first_copy != NULL) {
        /* Every selected row is rechecked after the driver inserts the
         * corresponding Clone.  Collecting the complete batch here avoids
         * one whole-program analysis per independent field without treating
         * an unlanded row as current in this analysis. */
        semantic_loan_copy_landing_error_with_hints(ctx,
            PGY_CODE_SEM_BORROW_ESCAPE, PGY_CAUSE_BORROW_ESCAPE,
            PGY_FIX_USE_MOVE_OR_RETAIN_BINDING, first_copy,
            "A String field without an independent-source proof is copied where it is stored");
        return false;
    }
    if (issued == store->indexed_string_field_count) return true;
    semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
        PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
        ctx->program_root, "A String field source recipe was withdrawn");
    return false;
}
