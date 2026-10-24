/* Loan copy landing: which getter call a next analysis clones, and which
 * call's result a Clone copies into storage of its own. */
#include "loan_copy_landing_owner.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "builtin_kind.h"
#include "function_param_flow_summary_internal.h"
#include "function_param_publication_owner.h"
#include "slot_summary.h"
#include "type_checker_internal.h"
#include "type_checker_ownership_internal.h"
#include "../parser/ast_api.h"

/* The call changes no storage it shares with its caller through any
 * argument: a copy of its result then leaves the caller as it was. A call
 * that grows or changes such storage in place is a move, whatever becomes
 * of its result. */
static bool loan_call_preserves_storage(SemanticContext *ctx, const ASTNode *call)
{
    for (size_t p = 0; p < ast_call_arg_count(call); p++) {
        SemanticPublishedValue argument = semantic_param_publication_value(ctx, ast_call_argument(call, p));
        if (!argument.ok) return false;
        if (argument.meta || type_equals(argument.type, TYPE_STRING) ||
            semantic_classify_ownership_type(argument.type, ctx) == OWNERSHIP_TYPE_COPY_ONLY) continue;
        ASTNode *callee = semantic_param_publication_callee(ctx, call, p);
        size_t formal = 0;
        if (callee == NULL || !semantic_param_publication_formal_for_actual(ctx, call, p, &formal) ||
            !function_param_flow_preserves_array_storage(ctx, callee, formal)) return false;
    }
    return true;
}

static bool loan_copy_landing_record(SemanticContext *ctx,
    const ASTNode *node)
{
    uint32_t id = ast_node_stable_id(node);
    for (size_t i = 0; i < ctx->loan_copy_landing_selected_count; i++)
        if (ctx->loan_copy_landing_selected[i] == id) return true;
    if (ctx->loan_copy_landing_selected_count == ctx->loan_copy_landing_selected_capacity) {
        size_t capacity = ctx->loan_copy_landing_selected_capacity == 0 ? 16
            : ctx->loan_copy_landing_selected_capacity * 2;
        uint32_t *grown = realloc(ctx->loan_copy_landing_selected, capacity * sizeof(*grown));
        if (grown == NULL) return false;
        ctx->loan_copy_landing_selected = grown;
        ctx->loan_copy_landing_selected_capacity = capacity;
    }
    ctx->loan_copy_landing_selected[ctx->loan_copy_landing_selected_count++] = id;
    if (getenv("PGY_DEBUG_PIPELINE_STAGE") != NULL) {
        const char *routine = ctx->current_function_decl != NULL
            ? ast_declaration_name(ctx->current_function_decl) : NULL;
        fprintf(stderr,
            "[semantic landing select] node=%u kind=%d line=%d col=%d routine=%s routine_id=%u\n",
            id, node != NULL ? (int)node->type : -1,
            node != NULL ? node->line : 0, node != NULL ? node->column : 0,
            routine != NULL ? routine : "<none>",
            ctx->current_function_decl != NULL
                ? ast_node_stable_id(ctx->current_function_decl) : 0);
    }
    return true;
}

bool semantic_loan_copy_landing_refusal_tail_current(const SemanticContext *ctx)
{
    if (ctx == NULL) return false;
    if (!ctx->has_error) return true;
    return ctx->loan_copy_landing_selected_count != 0 &&
        ctx->loan_copy_landing_tail_diagnostic_revision != 0 &&
        ctx->diagnostic_emission_revision != SIZE_MAX &&
        ctx->loan_copy_landing_tail_diagnostic_revision ==
            ctx->diagnostic_emission_revision &&
        ctx->current_function_decl != NULL &&
        ctx->loan_copy_landing_tail_function_id != 0 &&
        ctx->loan_copy_landing_tail_function_id ==
            ast_node_stable_id(ctx->current_function_decl);
}

void semantic_loan_copy_landing_refusal_tail_seal(SemanticContext *ctx)
{
    if (ctx != NULL && ctx->loan_copy_landing_selected_count != 0 &&
        ctx->diagnostic_emission_revision != 0 &&
        ctx->diagnostic_emission_revision != SIZE_MAX &&
        ctx->current_function_decl != NULL) {
        ctx->loan_copy_landing_tail_diagnostic_revision =
            ctx->diagnostic_emission_revision;
        ctx->loan_copy_landing_tail_function_id =
            ast_node_stable_id(ctx->current_function_decl);
    }
}

bool semantic_loan_copy_landing_call_copies(SemanticContext *ctx, const ASTNode *call)
{
    return semantic_loan_copy_landing_carrier_copies(ctx, call) && loan_call_preserves_storage(ctx, call);
}

bool semantic_loan_copy_landing_select(SemanticContext *ctx, const ASTNode *call)
{
    return !semantic_loan_copy_landing_call_copies(ctx, call) || loan_copy_landing_record(ctx, call);
}

bool semantic_loan_copy_landing_carrier_copies(SemanticContext *ctx, const ASTNode *carrier)
{
    SemanticPublishedValue value = semantic_param_publication_value(ctx, carrier);
    return ast_node_stable_id(carrier) != 0 && value.ok && !value.meta &&
        semantic_param_publication_clone_independent(ctx, value.type);
}

bool semantic_loan_copy_landing_satisfied(SemanticContext *ctx,
    const ASTNode *carrier)
{
    uint32_t builtin = 0;
    bool depends = true, retains = true;
    SemanticPublishedValue value = semantic_param_publication_value(ctx, carrier);
    bool relation = carrier != NULL && carrier->type == AST_CALL
        && semantic_param_publication_checked_result_relation(
            ctx, carrier, 0, &depends, &retains);
    return carrier != NULL && carrier->type == AST_CALL
        && ast_call_compiler_builtin(carrier)
        && ast_call_arg_count(carrier) == 1
        && ast_call_semantic_callee_builtin_kind(carrier, &builtin)
        && builtin == BUILTIN_CLONE
        && ast_call_semantic_callee_is_stdlib(carrier)
        && ast_call_semantic_callee_decl_id(carrier) == 0
        && ast_call_semantic_callee_value_binding_id(carrier) == 0
        && value.ok && !value.meta
        && semantic_param_publication_clone_independent(ctx, value.type)
        && relation
        && !depends && !retains;
}

bool semantic_loan_copy_landing_select_string(SemanticContext *ctx, const ASTNode *expression)
{
    return semantic_loan_copy_landing_satisfied(ctx, expression)
        || ast_node_stable_id(expression) == 0
        || loan_copy_landing_record(ctx, expression);
}

bool semantic_loan_copy_landing_select_typed(SemanticContext *ctx, const ASTNode *expression, const Type *type)
{
    return ast_node_stable_id(expression) == 0 || !semantic_param_publication_clone_independent(ctx, type) ||
        loan_copy_landing_record(ctx, expression);
}

bool semantic_loan_copy_landing_selected(const SemanticContext *ctx,
    const ASTNode *carrier)
{
    uint32_t id = ast_node_stable_id(carrier);
    if (ctx == NULL || id == 0) return false;
    for (size_t i = 0; i < ctx->loan_copy_landing_selected_count; i++)
        if (ctx->loan_copy_landing_selected[i] == id) return true;
    return false;
}

bool semantic_loan_copy_landing_select_carrier(SemanticContext *ctx, const ASTNode *carrier)
{
    return semantic_loan_copy_landing_satisfied(ctx, carrier)
        || !semantic_loan_copy_landing_carrier_copies(ctx, carrier) ||
        loan_copy_landing_record(ctx, carrier);
}

bool semantic_loan_copy_landing_select_typed_carrier(SemanticContext *ctx,
    const ASTNode *carrier, const Type *type)
{
    uint32_t id = ast_node_stable_id(carrier);
    return id != 0 && semantic_param_publication_clone_independent(ctx, type)
        && loan_copy_landing_record(ctx, carrier);
}

const ASTNode *semantic_loan_copy_landing_copied_call(SemanticContext *ctx, const ASTNode *call)
{
    uint32_t builtin = 0;
    bool depends = true, retains = true;
    const ASTNode *argument = ast_call_arg_count(call) == 1 ? ast_call_argument(call, 0) : NULL;
    if (argument == NULL || argument->type != AST_CALL ||
        !ast_call_semantic_callee_builtin_kind(call, &builtin) || builtin != BUILTIN_CLONE ||
        !ast_call_semantic_callee_is_stdlib(call) ||
        !semantic_param_publication_checked_result_relation(ctx, call, 0, &depends, &retains) ||
        depends || retains || !loan_call_preserves_storage(ctx, argument)) return NULL;
    return argument;
}

bool semantic_loan_copy_landing_ends_loan(SemanticContext *ctx, ASTNode *caller, const ASTNode *call)
{
    SlotFunctionLookup lookup = {ctx, ctx->function_param_publications->program};
    FunctionParamFlowSummaryStore *store = function_param_flow_store_get(&lookup);
    FunctionParamFlowProgramPointIndex *index = NULL;
    if (store == NULL || store->failed || store->solving || call == NULL ||
        !function_param_flow_program_point_index_build(store, caller, &index)) return false;
    for (size_t c = 0; c < index->claims->count; c++) {
        const ASTNode *node = index->claims->rows[c].node;
        if (node != NULL && node->type == AST_CALL && ast_call_arg_count(node) == 1 &&
            ast_call_argument(node, 0) == call)
            return semantic_loan_copy_landing_copied_call(ctx, node) == call;
    }
    return false;
}
