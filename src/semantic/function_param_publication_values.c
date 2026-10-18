/* Checked typed values and their independent String-result source. */
#include "function_param_publication_internal.h"
#include "indexed_string_borrow_owner.h"
#include "array_storage_release_owner.h"
#include "type_checker_internal.h"
#include <string.h>

typedef struct PublicationConcatVisit {
    const ASTNode *node;
    const struct PublicationConcatVisit *parent;
} PublicationConcatVisit;
static PublishedNode *publication_current_value_at(const SemanticContext *ctx,
    const ASTNode *value, const PublicationConcatVisit *parent)
{
    if (!semantic_param_publication_inputs_ready(ctx)) return NULL;
    PublishedNode *row = publication_node(ctx->function_param_publications, value, false);
    if (row == NULL || !row->value_present || row->value_shape != value->type ||
        (value->type == AST_CALL && !semantic_param_publication_checked_call(ctx, value)) || (value->type == AST_IDENTIFIER &&
        row->binding_id != ast_identifier_binding_syntax_id(value))) return NULL;
    if (row->kind_present && row->builtin == BUILTIN_TEXT_BUILDER_FINISH &&
        (row->origin_ready || row->copy_escape) &&
        !semantic_param_publication_checked_finish_independent(ctx, value)) return NULL;
    if (row->kind_present && row->builtin == BUILTIN_READ_FILE &&
        (row->origin_ready || row->copy_escape) &&
        !semantic_param_publication_checked_readfile_independent(ctx, value)) return NULL;
    if (row->empty_map_declaration != NULL && (row->empty_map_declaration_id == 0 ||
        row->empty_map_declaration_id != ast_node_stable_id(row->empty_map_declaration) ||
        !semantic_param_publication_empty_map_binding(ctx, row->empty_map_declaration))) return NULL;
    if (row->owned_slot_declaration != NULL) {
        SemanticPublishedSlotBirth birth;
        if (row->owned_slot_declaration_id == 0 || row->owned_slot_declaration_id != ast_node_stable_id(row->owned_slot_declaration) ||
            !semantic_param_publication_owned_slot_binding(ctx, row->owned_slot_declaration, &birth)) return NULL;
    }
    if (value->type == AST_BINARY && row->string_value && row->origin_ready &&
        !row->builtin_string_concat) return NULL;
    if (row->builtin_string_concat) {
        for (const PublicationConcatVisit *visit = parent; visit != NULL; visit = visit->parent)
            if (visit->node == value) return NULL;
        if (value->type != AST_BINARY || ast_binary_operator(value).type != TOKEN_PLUS ||
            !row->string_value || row->value_type != TYPE_STRING || row->value_type->kind != TYPE_KIND_PRIMITIVE ||
            !row->origin_ready || row->argument_count != 2 || row->arguments == NULL) return NULL;
        PublicationConcatVisit visit = {value, parent};
        const ASTNode *nodes[2] = {ast_binary_left(value), ast_binary_right(value)};
        for (size_t i = 0; i < 2; i++) {
            const PublishedCallArgument *snapshot = &row->arguments[i];
            if (nodes[i] == NULL || snapshot->node != nodes[i] || snapshot->id == 0 ||
                snapshot->id != ast_node_stable_id(nodes[i]) || snapshot->shape != nodes[i]->type ||
                snapshot->binding != ast_identifier_binding_syntax_id(nodes[i])) return NULL;
            PublishedNode *operand = publication_current_value_at(ctx, nodes[i], &visit);
            const Type *type = operand == NULL ? NULL : operand->value_type;
            const Type *effective = type_is_slot_handle(type) ? type_slot_inner_type(type) : type;
            if (operand == NULL || snapshot->type != type || effective != TYPE_STRING ||
                effective->kind != TYPE_KIND_PRIMITIVE) return NULL;
        }
    }
    return row;
}
PublishedNode *publication_current_value(const SemanticContext *ctx, const ASTNode *value)
{ return publication_current_value_at(ctx, value, NULL); }
bool semantic_param_publication_checked_local_owner_constructor(
    const SemanticContext *ctx, const ASTNode *value)
{
    if (value != NULL && value->type == AST_BINARY) {
        PublishedNode *concat = publication_current_value(ctx, value);
        return concat != NULL && !concat->meta && concat->value_type == TYPE_STRING &&
            concat->builtin_string_concat && concat->origin_ready &&
            concat->argument_count == 2 && ast_binary_operator(value).type == TOKEN_PLUS;
    }
    if (semantic_param_publication_inputs_ready(ctx) &&
        semantic_param_publication_checked_readfile_independent(ctx, value)) return true;
    PublishedNode *row = publication_result_current(ctx, value);
    if (row == NULL || row->meta || row->declared_callable || row->declaration != 0 || row->value_binding != 0 ||
        !row->kind_present || row->nominal_result != NULL || row->result_relation ||
        row->target == NULL || row->target->type != AST_IDENTIFIER || row->target_binding != 0 ||
        row->target_id == 0 || row->target_name == NULL || row->id == 0) return false;
    if (semantic_array_storage_independent_physical_hof_result(value, (SemanticContext *)ctx))
        return true;
    /* These constructor contracts prove no input retention and independent
     * result backing. They confer no copy, cleanup or Finish lifetime right. */
    if (row->empty_map_declaration != NULL) {
        const ASTNode *declaration = row->empty_map_declaration;
        return row->stdlib && row->builtin == BUILTIN_NOT_BUILTIN &&
            declaration->type == AST_LET_DECL &&
            row->empty_map_declaration_id != 0 &&
            row->empty_map_declaration_id == ast_node_stable_id(declaration) &&
            ast_let_initializer(declaration) == value &&
            semantic_param_publication_empty_map_binding(ctx, declaration);
    }
    if (row->builtin == BUILTIN_CONCAT)
        return row->stdlib && row->value_type == TYPE_STRING && row->argument_count == 2 &&
            strcmp(row->target_name, "Concat") == 0 && row->arguments[0].type == TYPE_STRING &&
            row->arguments[1].type == TYPE_STRING &&
            semantic_param_publication_argument_borrowed_for_call(ctx, value, 0) &&
            semantic_param_publication_argument_borrowed_for_call(ctx, value, 1);
    /* The current typed publication owns the result type; a present AST type
       projection must agree, but stdlib calls need not carry that projection.
       ToString(String) is identity; Bool may be static text. This proves input-
       backing independence, never fresh allocation or cleanup authority. */
    if (row->stdlib && row->builtin == BUILTIN_NOT_BUILTIN && row->value_type == TYPE_STRING &&
        (ast_call_semantic_value_type_name(value) == NULL ||
         strcmp(ast_call_semantic_value_type_name(value), "String") == 0)) {
        bool substring = strcmp(row->target_name, "Substring") == 0;
        bool scalar = strcmp(row->target_name, "ToString") == 0;
        if ((substring || scalar || strcmp(row->target_name, "StringReplace") == 0) &&
            row->argument_count == (scalar ? 1u : 3u)) {
            for (size_t p = 0; p < row->argument_count; p++) {
                const PublishedCallArgument *arg = &row->arguments[p];
                if (scalar ? !publication_physical_scalar(arg->type) :
                    arg->type != (substring && p != 0 ? TYPE_INT : TYPE_STRING) ||
                    ((p == 0 || !substring) &&
                     !semantic_param_publication_argument_borrowed_for_call(ctx, value, p))) return false;
                if (arg->shape == AST_CALL && ast_call_semantic_value_type_name(arg->node) != NULL && (
                    strcmp(ast_call_semantic_value_type_name(arg->node), arg->type->name) != 0)) return false;
            }
            return true;
        }
    }
    if (row->stdlib) return false;
    if (row->builtin == BUILTIN_ALLOCATOR_RESULT)
        return row->value_type == TYPE_ALLOCATOR && row->argument_count == 0;
    if (row->builtin != BUILTIN_TEXT_BUILDER_NEW || row->value_type != TYPE_TEXT_BUILDER ||
        row->argument_count != 1 || row->arguments[0].id == 0 ||
        !semantic_param_publication_checked_argument(ctx, value, 0)) return false;
    PublishedNode *capacity = publication_current_value(ctx, row->arguments[0].node);
    return capacity != NULL && !capacity->meta &&
        (capacity->value_type == TYPE_INT || capacity->value_type == TYPE_LONG) &&
        row->arguments[0].type == capacity->value_type;
}

typedef struct IndexedConcatVisit {
    const ASTNode *node;
    const struct IndexedConcatVisit *parent;
} IndexedConcatVisit;

static bool
pending_string_concat_current(const ASTNode *value, const SemanticContext *ctx,
    const IndexedConcatVisit *parent)
{
    if (value == NULL || value->type != AST_BINARY || !publication_generation_ready(ctx)
        || ast_binary_operator(value).type != TOKEN_PLUS) return false;
    for (const IndexedConcatVisit *p = parent; p != NULL; p = p->parent)
        if (p->node == value) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, value, false);
    if (row == NULL || row->node != value || row->id == 0 || row->id != ast_node_stable_id(value)
        || !row->value_present || row->value_shape != AST_BINARY || row->meta
        || row->value_type != TYPE_STRING || row->value_type->kind != TYPE_KIND_PRIMITIVE
        || !row->string_value || !row->origin_ready || !row->builtin_string_concat
        || row->argument_count != 2 || row->arguments == NULL) return false;
    IndexedConcatVisit visit = {value, parent};
    const ASTNode *nodes[2] = {ast_binary_left(value), ast_binary_right(value)};
    for (size_t i = 0; i < 2; i++) {
        const PublishedCallArgument *snapshot = &row->arguments[i];
        if (nodes[i] == NULL || snapshot->node != nodes[i] || snapshot->id == 0
            || snapshot->id != ast_node_stable_id(nodes[i]) || snapshot->shape != nodes[i]->type
            || snapshot->binding != ast_identifier_binding_syntax_id(nodes[i])) return false;
        PublishedNode *operand = publication_node(ctx->function_param_publications, nodes[i], false);
        const Type *type = operand == NULL ? NULL : operand->value_type;
        const Type *effective = type_is_slot_handle(type) ? type_slot_inner_type(type) : type;
        if (operand == NULL || operand->node != nodes[i] || operand->id != snapshot->id
            || !operand->value_present || operand->value_shape != snapshot->shape || type != snapshot->type
            || effective != TYPE_STRING || effective->kind != TYPE_KIND_PRIMITIVE
            || (nodes[i]->type == AST_IDENTIFIER && operand->binding_id != snapshot->binding)
            || (nodes[i]->type == AST_CALL && !publication_call_ready(operand))) return false;
        if (nodes[i]->type == AST_BINARY && !pending_string_concat_current(nodes[i], ctx, &visit)) return false;
    }
    return true;
}

bool
semantic_indexed_string_independent_result(const ASTNode *expression,
                                          const SemanticContext *ctx)
{
    const ASTNode *callee;
    const char *name;
    uint32_t builtin_kind = 0;

    if (expression == NULL)
        return false;
    if (expression->type == AST_STRING)
        return true;
    if (expression->type == AST_BINARY) {
        if (!publication_generation_ready(ctx)) return false;
        PublishedNode *concat = publication_node(ctx->function_param_publications, expression, false);
        if (concat == NULL || concat->node != expression || concat->id == 0
            || concat->id != ast_node_stable_id(expression)) return false;
        return semantic_param_publication_inputs_ready(ctx)
            ? semantic_param_publication_checked_local_owner_constructor(ctx, expression)
            : pending_string_concat_current(expression, ctx, NULL);
    }
    if (expression->type != AST_CALL)
        return false;
    if (ast_call_semantic_callee_decl_id(expression) != 0 ||
        ast_call_semantic_callee_value_binding_id(expression) != 0 ||
        !ast_call_semantic_callee_builtin_kind(expression, &builtin_kind))
        return false;
    if (builtin_kind == BUILTIN_TEXT_BUILDER_FINISH)
        return semantic_param_publication_checked_finish_independent(ctx, expression);
    if (builtin_kind == BUILTIN_READ_FILE)
        return semantic_param_publication_checked_readfile_independent(ctx, expression);
    if (!ast_call_semantic_callee_is_stdlib(expression))
        return false;
    callee = ast_call_callee(expression);
    if (callee == NULL || callee->type != AST_IDENTIFIER)
        return false;
    name = ast_identifier_name(callee);
    /* A scalar ToString reads no String storage, so its result is no element
     * of any Array<String>. ToString(String) returns its input and stays out. */
    if (name != NULL && strcmp(name, "ToString") == 0) {
        if (builtin_kind != BUILTIN_NOT_BUILTIN || ast_call_arg_count(expression) != 1 ||
            !publication_generation_ready(ctx)) return false;
        PublishedNode *row = publication_node(ctx->function_param_publications, expression, false);
        return row != NULL && row->node == expression && row->id != 0 &&
            row->id == ast_node_stable_id(expression) && row->call_present &&
            row->value_present && row->value_type == TYPE_STRING &&
            row->argument_count == 1 && row->arguments != NULL &&
            publication_physical_scalar(row->arguments[0].type);
    }
    /* Consumed only after successful String typing (publication or binding).
     * These checked allocating readers do not inherit freshness from their
     * input-retention classification. Empty/NULL failure results are not an
     * exact-copy or successful-allocation certificate. */
    if (name != NULL && builtin_kind == BUILTIN_NOT_BUILTIN &&
        ast_node_stable_id(expression) != 0 && ast_node_stable_id(callee) != 0 &&
        ((strcmp(name, "StringTrim") == 0 && ast_call_arg_count(expression) == 1) ||
         (strcmp(name, "CharAtN") == 0 && ast_call_arg_count(expression) == 3) ||
         (strcmp(name, "Substring") == 0 && ast_call_arg_count(expression) == 3) ||
         (strcmp(name, "SubstringWithLen") == 0 && ast_call_arg_count(expression) == 4)))
        return true;
    return name != NULL && (strcmp(name, "Concat") == 0
        || strcmp(name, "StringConcat") == 0
        || strcmp(name, "StringJoin") == 0);
}
