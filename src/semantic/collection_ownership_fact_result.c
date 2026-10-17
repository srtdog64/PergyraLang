#include "collection_ownership_fact_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "diag_codes.h"
#include "symbol_table.h"
#include "type_checker.h"
#include "type_checker_internal.h"
#include "function_param_publication_owner.h"
#include "function_param_publication_internal.h"
#include "function_param_flow_summary_internal.h"
#include "../parser/ast_api.h"

bool
collection_call_is_unshadowed_builtin(const ASTNode *expr,
                                      const char *expected_name)
{
    ASTNode *callee;
    const char *name;

    if (expr == NULL || expr->type != AST_CALL || expected_name == NULL)
        return false;
    callee = ast_call_callee(expr);
    if (callee == NULL || callee->type != AST_IDENTIFIER)
        return false;
    name = ast_identifier_name(callee);
    return name != NULL && strcmp(name, expected_name) == 0
        && ast_identifier_binding_syntax_id(callee) == 0;
}

static ASTNode *
collection_direct_let_for_binding(ASTNode *body, uint32_t binding_syntax_id)
{
    if (body == NULL || body->type != AST_BLOCK || binding_syntax_id == 0)
        return NULL;
    for (size_t i = 0; i < ast_block_statement_count(body); i++) {
        ASTNode *stmt = ast_block_statement(body, i);
        if (stmt != NULL && stmt->type == AST_LET_DECL
            && ast_node_stable_id(stmt) == binding_syntax_id)
            return stmt;
    }
    return NULL;
}

typedef struct OwnedStringResultVisit {
    const ASTNode *decl;
    const struct OwnedStringResultVisit *parent;
} OwnedStringResultVisit;

static bool collection_callable_type_returns_owned_string_at(const ASTNode *call,
    SemanticContext *ctx, uint32_t *producer_id_out, const OwnedStringResultVisit *parent);

static bool collection_owned_string_source_current(const ASTNode *call,
    SemanticContext *ctx, const OwnedStringResultVisit *parent)
{
    if (ctx == NULL || !publication_generation_ready(ctx) ||
        (!ctx->function_param_publications->pending && !ctx->function_param_publications->inputs_ready)) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, call, false);
    if (row == NULL || call == NULL || row->node != call || row->id != ast_node_stable_id(call) ||
        call->type != AST_CALL || !publication_call_ready(row) ||
        !row->value_present || row->meta || row->value_shape != AST_CALL ||
        row->value_type != TYPE_STRING || !row->string_value) return false;
    for (size_t p = 0; p < row->argument_count; p++) {
        const PublishedCallArgument *arg = &row->arguments[p];
        PublishedNode *value = publication_node(ctx->function_param_publications, arg->node, false);
        if (value == NULL || value->node != arg->node || value->id != arg->id ||
            !value->value_present || value->meta || value->value_type == NULL ||
            value->value_type == TYPE_UNKNOWN || value->value_type != arg->type ||
            value->value_shape != arg->shape || (arg->shape == AST_IDENTIFIER && value->binding_id != arg->binding) ||
            (arg->shape == AST_CALL && !publication_call_ready(value))) return false;
    }
    if (row->kind_present && row->builtin == BUILTIN_TEXT_BUILDER_FINISH)
        return semantic_param_publication_checked_finish_independent(ctx, call);
    if (row->kind_present && row->builtin == BUILTIN_NOT_BUILTIN && row->stdlib &&
        row->declaration == 0 && row->value_binding == 0 && row->target_binding == 0 &&
        row->target_shape == AST_IDENTIFIER && row->target_name != NULL &&
        strcmp(row->target_name, "Concat") == 0 && row->argument_count == 2 &&
        row->arguments[0].type == TYPE_STRING && row->arguments[1].type == TYPE_STRING) return true;
    return collection_callable_type_returns_owned_string_at(call, ctx, NULL, parent);
}

static bool collection_owned_string_receipt_current(ASTNode *decl, SemanticContext *ctx,
    const OwnedStringResultVisit *parent)
{
    for (const OwnedStringResultVisit *visit = parent; visit != NULL; visit = visit->parent)
        if (visit->decl == decl) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, decl, false);
    PublishedOwnedStringResult *receipt = row == NULL ? NULL : row->owned_string_result;
    if (row == NULL || row->node != decl || row->id != ast_node_stable_id(decl) ||
        receipt == NULL || receipt->body == NULL || receipt->body != ast_func_body(decl) ||
        receipt->body_id == 0 || receipt->body_id != ast_node_stable_id(receipt->body) ||
        receipt->source_count == 0 || receipt->source_count != receipt->sealed_source_count ||
        receipt->sources == NULL || receipt->syntax == NULL ||
        !semantic_param_publication_routine_inputs_current(ctx, decl, true) ||
        !function_param_flow_syntax_claims_current(ctx, receipt->syntax)) return false;
    OwnedStringResultVisit visit = {decl, parent};
    for (size_t p = 0; p < receipt->source_count; p++) {
        const PublishedCallArgument *source = &receipt->sources[p];
        if (source->node == NULL || source->shape != AST_CALL || source->node->type != AST_CALL ||
            source->id == 0 || source->id != ast_node_stable_id(source->node) || source->type != TYPE_STRING ||
            source->binding != ast_identifier_binding_syntax_id(source->node) ||
            !collection_owned_string_source_current(source->node, ctx, &visit)) return false;
    }
    return true;
}

static bool
collection_callable_type_returns_owned_string_at(const ASTNode *call,
    SemanticContext *ctx, uint32_t *producer_id_out, const OwnedStringResultVisit *parent)
{
    if (producer_id_out != NULL) *producer_id_out = 0;
    if (ctx == NULL || !publication_generation_ready(ctx) ||
        (!ctx->function_param_publications->pending && !ctx->function_param_publications->inputs_ready)) return false;
    ASTNode *callee = call == NULL ? NULL : ast_call_callee(call);
    const char *name = callee != NULL && callee->type == AST_IDENTIFIER ? ast_identifier_name(callee) : NULL;
    uint32_t producer = call == NULL ? 0 : ast_call_semantic_callee_decl_id(call);
    ASTNode *decl = name == NULL ? NULL : semantic_find_callable_decl_by_name(ctx, name);
    Symbol *symbol = name == NULL ? NULL : scope_lookup(ctx->scope, name);
    PublishedNode *value = publication_node(ctx->function_param_publications, call, false);
    if (producer == 0 || decl == NULL || decl->type != AST_FUNC_DECL || ast_node_stable_id(decl) != producer ||
        symbol == NULL || symbol->kind != SYMBOL_FUNCTION || symbol->decl_syntax_id != producer ||
        symbol->type == NULL || symbol->type->kind != TYPE_KIND_FUNCTION ||
        !type_function_has_body_summary(symbol->type) ||
        !type_equals(type_function_return_type(symbol->type), TYPE_STRING) ||
        (type_function_body_summary(symbol->type) & BODY_SUMMARY_RETURNS_OWNED_STRING) == 0 ||
        value == NULL || value->node != call || value->id != ast_node_stable_id(call) ||
        !publication_call_ready(value) || !value->value_present || value->meta ||
        value->value_type != TYPE_STRING || !value->string_value || value->value_shape != AST_CALL) return false;
    for (size_t p = 0; p < value->argument_count; p++) {
        const PublishedCallArgument *arg = &value->arguments[p];
        PublishedNode *actual = publication_node(ctx->function_param_publications, arg->node, false);
        if (actual == NULL || actual->node != arg->node || actual->id != arg->id ||
            !actual->value_present || actual->meta || actual->value_type == NULL ||
            actual->value_type == TYPE_UNKNOWN || actual->value_type != arg->type ||
            actual->value_shape != arg->shape || (arg->shape == AST_IDENTIFIER && actual->binding_id != arg->binding) ||
            (arg->shape == AST_CALL && !publication_call_ready(actual))) return false;
    }
    if (!collection_owned_string_receipt_current(decl, ctx, parent)) return false;
    if (producer_id_out != NULL) *producer_id_out = producer;
    return true;
}

static void collection_owned_string_receipt_destroy(PublishedOwnedStringResult *receipt)
{
    if (receipt == NULL) return;
    function_param_flow_claims_destroy(receipt->syntax);
    free(receipt->sources);
    free(receipt);
}

static bool
collection_owned_string_expression_ready(ASTNode *body,
                                         const ASTNode *expression,
                                         SemanticContext *ctx,
                                         unsigned depth, PublishedOwnedStringResult *receipt)
{
    ASTNode *decl;
    uint32_t binding_id;

    if (body == NULL || expression == NULL || depth > 8)
        return false;
    if (expression->type == AST_IDENTIFIER) {
        binding_id = ast_identifier_binding_syntax_id(expression);
        decl = collection_direct_let_for_binding(body, binding_id);
        /* A named initializer is only a straight-line provenance proof.
         * Control-dependent rebinding needs a value fact, not the old let. */
        for (size_t i = 0; i < ast_block_statement_count(body); i++) {
            ASTNode *stmt = ast_block_statement(body, i);
            if (stmt == NULL)
                return false;
            if (stmt->type == AST_ASSIGNMENT) {
                ASTNode *target = ast_assignment_target(stmt);
                if (target != NULL && target->type == AST_IDENTIFIER
                    && ast_identifier_binding_syntax_id(target) == binding_id)
                    return false;
            } else if (stmt->type != AST_RETURN && stmt->type != AST_LET_DECL
                       && stmt->type != AST_CALL
                       && stmt->type != AST_DEFER_STMT) {
                return false;
            }
        }
        return decl != NULL && !ast_let_is_mutable(decl)
            && collection_owned_string_expression_ready(
                body, ast_let_initializer(decl), ctx, depth + 1, receipt);
    }
    if (expression->type != AST_CALL)
        return false;
    if (!collection_owned_string_source_current(expression, ctx, NULL)) return false;
    for (size_t p = 0; p < receipt->source_count; p++)
        if (receipt->sources[p].node == expression) return true;
    if (receipt->source_count == SIZE_MAX / sizeof(*receipt->sources)) return false;
    PublishedCallArgument *sources = realloc(receipt->sources,
        (receipt->source_count + 1) * sizeof(*sources));
    if (sources == NULL) return false;
    receipt->sources = sources;
    sources[receipt->source_count++] = (PublishedCallArgument){expression,
        ast_node_stable_id(expression), expression->type,
        ast_identifier_binding_syntax_id(expression), TYPE_STRING, 0, 0};
    return true;
}

bool
semantic_collection_owned_string_call_result(
    const ASTNode *expression,
    SemanticContext *ctx,
    uint32_t *producer_syntax_id_out)
{
    return collection_callable_type_returns_owned_string_at(
        expression, ctx, producer_syntax_id_out, NULL);
}

static bool
collection_owned_string_returns_ready(ASTNode *node, ASTNode *function_body,
                                      SemanticContext *ctx,
                                      bool *saw_return, unsigned depth, PublishedOwnedStringResult *receipt)
{
    if (node == NULL)
        return true;
    if (depth > 8)
        return false;
    if (node->type == AST_RETURN) {
        *saw_return = true;
        return collection_owned_string_expression_ready(
            function_body, ast_return_value(node), ctx, 0, receipt);
    }
    if (node->type == AST_BLOCK) {
        for (size_t i = 0; i < ast_block_statement_count(node); i++) {
            ASTNode *stmt = ast_block_statement(node, i);
            if (stmt == NULL || !collection_owned_string_returns_ready(
                    stmt, function_body, ctx, saw_return, depth + 1, receipt)) {
                return false;
            }
        }
        return true;
    }
    if (node->type == AST_IF_STMT) {
        return collection_owned_string_returns_ready(
                   ast_if_then_branch(node), function_body, ctx,
                   saw_return, depth + 1, receipt)
            && collection_owned_string_returns_ready(
                   ast_if_else_branch(node), function_body, ctx,
                   saw_return, depth + 1, receipt);
    }
    /* Lambda bodies are not enclosing-function returns. Other control owners
     * remain unadmitted until their return facts are explicitly supported. */
    return node->type == AST_LET_DECL || node->type == AST_CALL
        || node->type == AST_ASSIGNMENT || node->type == AST_DEFER_STMT;
}

void
semantic_collection_record_owned_string_result_summary(
    ASTNode *function_decl,
    SemanticContext *ctx)
{
    bool saw_return = false;
    if (function_decl == NULL || function_decl->type != AST_FUNC_DECL || ctx == NULL ||
        ctx->function_param_publications == NULL) return;
    PublishedNode *row = publication_node(ctx->function_param_publications, function_decl, false);
    if (row != NULL) {
        collection_owned_string_receipt_destroy(row->owned_string_result);
        row->owned_string_result = NULL;
    }
    if (!publication_generation_ready(ctx) || !ctx->tracking_function_effects || !type_equals(ctx->current_return, TYPE_STRING) ||
        !ctx->function_param_publications->pending) return;
    PublishedOwnedStringResult *receipt = calloc(1, sizeof(*receipt));
    if (receipt == NULL) return;
    ASTNode *body = ast_func_body(function_decl);
    if (body != NULL && body->type == AST_BLOCK
        && collection_owned_string_returns_ready(
            body, body, ctx, &saw_return, 0, receipt) && saw_return &&
        receipt->source_count != 0 && ast_node_stable_id(body) != 0 &&
        function_param_flow_syntax_claims_snapshot(ctx, &receipt->syntax, body)) {
        receipt->body = body; receipt->body_id = ast_node_stable_id(body);
        receipt->sealed_source_count = receipt->source_count;
        row = publication_node(ctx->function_param_publications, function_decl, true);
        if (row == NULL) { collection_owned_string_receipt_destroy(receipt); return; }
        row->owned_string_result = receipt;
        semantic_record_body_summary(
            ctx, BODY_SUMMARY_RETURNS_OWNED_STRING);
    } else collection_owned_string_receipt_destroy(receipt);
}
