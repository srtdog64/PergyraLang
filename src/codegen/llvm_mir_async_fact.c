/*
 * LLVM MIR async value facts for channel recv and await locals.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_mir_async_fact.h"

#include <string.h>

#include "llvm_internal_api.h"
#include "parser/ast_api.h"

static ASTNode *
llvm_mir_async_fact_initializer_expr(ASTNode *expr)
{
    if (expr != NULL && expr->type == AST_LET_DECL)
        return ast_let_initializer(expr);
    return expr;
}

static ASTNode *
llvm_mir_async_fact_recv_expr(ASTNode *expr)
{
    ASTNode *value = llvm_mir_async_fact_initializer_expr(expr);

    if (value == NULL)
        return NULL;
    if (value->type == AST_CHANNEL_RECV)
        return value;
    if (value->type == AST_ASSIGNMENT
        && ast_assignment_value(value) != NULL
        && ast_assignment_value(value)->type == AST_CHANNEL_RECV) {
        return ast_assignment_value(value);
    }
    return NULL;
}

static ASTNode *
llvm_mir_async_fact_await_expr(ASTNode *expr)
{
    ASTNode *value = llvm_mir_async_fact_initializer_expr(expr);

    return value != NULL && value->type == AST_AWAIT_EXPR ? value : NULL;
}

static const MIRInstruction *
llvm_mir_async_fact_find_base_def(const MIRRoutine *routine,
                                  const char *base_name)
{
    char result_base[128];

    if (routine == NULL || base_name == NULL)
        return NULL;
    for (size_t b = 0; b < routine->block_count; b++) {
        const MIRBasicBlock *block = &routine->blocks[b];
        for (size_t i = 0; i < block->instruction_count; i++) {
            const MIRInstruction *candidate = &block->instructions[i];
            if (candidate->kind != MIR_INST_DEF)
                continue;
            if (candidate->arg0 != NULL
                && strcmp(candidate->arg0, base_name) == 0)
                return candidate;
            if (candidate->result_name == NULL)
                continue;
            if (!llvm_mir_base_name_from_versioned(candidate->result_name,
                    result_base, sizeof(result_base)))
                continue;
            if (strcmp(result_base, base_name) == 0)
                return candidate;
        }
    }
    return NULL;
}

static const char *
llvm_mir_async_fact_first_type_arg_scratch(LLVMGenCtx *ctx,
                                           ASTNode *type_node,
                                           const char *container_name)
{
    GenericParams *args;
    GenericParam *arg0;

    if (ctx == NULL || type_node == NULL || type_node->type != AST_TYPE)
        return NULL;
    if (ast_type_name(type_node) == NULL
        || strcmp(ast_type_name(type_node), container_name) != 0)
        return NULL;
    args = ast_type_generic_args(type_node);
    arg0 = ast_generic_param_at(args, 0);
    if (arg0 == NULL)
        return NULL;
    return llvm_stmt_render_type_arg_scratch(arg0, &ctx->scratch);
}

LLVMTypeRef
llvm_mir_async_fact_type_from_channel_recv(const MIRRoutine *routine,
                                           LLVMGenCtx *ctx,
                                           const MIRInstruction *inst)
{
    ASTNode *recv;
    ASTNode *channel;
    const MIRInstruction *channel_def;
    ASTNode *source;
    ASTNode *type_ann;
    const char *inner;

    if (routine == NULL || ctx == NULL || inst == NULL)
        return NULL;
    recv = llvm_mir_async_fact_recv_expr(inst->expr0);
    if (recv == NULL)
        return NULL;
    channel = ast_channel_recv_channel(recv);
    if (channel == NULL || channel->type != AST_IDENTIFIER
        || ast_identifier_name(channel) == NULL)
        return NULL;

    channel_def = llvm_mir_async_fact_find_base_def(routine,
        ast_identifier_name(channel));
    source = mir_instruction_source_payload(channel_def);
    type_ann = source != NULL && source->type == AST_LET_DECL
        ? ast_let_type(source) : NULL;
    inner = llvm_mir_async_fact_first_type_arg_scratch(ctx, type_ann,
        "Channel");
    if (inner == NULL || inner[0] == '\0')
        return NULL;
    return pergyra_type_to_llvm(ctx, inner);
}

LLVMTypeRef
llvm_mir_async_fact_type_from_await(const MIRRoutine *routine,
                                    LLVMGenCtx *ctx,
                                    const MIRInstruction *inst)
{
    ASTNode *await_expr;
    ASTNode *operand;
    const MIRInstruction *future_def;
    ASTNode *source;
    ASTNode *type_ann;
    ASTNode *init;
    const char *inner;

    if (routine == NULL || ctx == NULL || inst == NULL)
        return NULL;
    await_expr = llvm_mir_async_fact_await_expr(inst->expr0);
    if (await_expr == NULL)
        return NULL;
    operand = ast_await_expression(await_expr);
    if (operand == NULL || operand->type != AST_IDENTIFIER
        || ast_identifier_name(operand) == NULL)
        return NULL;

    future_def = llvm_mir_async_fact_find_base_def(routine,
        ast_identifier_name(operand));
    source = mir_instruction_source_payload(future_def);
    type_ann = source != NULL && source->type == AST_LET_DECL
        ? ast_let_type(source) : NULL;
    inner = llvm_mir_async_fact_first_type_arg_scratch(ctx, type_ann,
        "Future");
    if (inner == NULL || inner[0] == '\0') {
        init = source != NULL && source->type == AST_LET_DECL
            ? ast_let_initializer(source) : NULL;
        if (init != NULL && init->type == AST_SPAWN_EXPR)
            inner = llvm_infer_spawn_future_inner(ctx, init);
    }
    if (inner == NULL || inner[0] == '\0')
        return NULL;
    return pergyra_type_to_llvm(ctx, inner);
}

#endif /* PGY_LLVM_ENABLED */
