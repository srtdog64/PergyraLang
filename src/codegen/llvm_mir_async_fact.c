/*
 * LLVM MIR async value facts for channel recv and await locals.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_mir_async_fact.h"

#include <string.h>

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

static LLVMTypeRef
llvm_mir_async_fact_type_from_local_container(const MIRRoutine *routine,
                                              LLVMGenCtx *ctx,
                                              const char *local_name,
                                              const char *prefix)
{
    const char *type_name;
    char inner[256];

    if (routine == NULL || ctx == NULL || local_name == NULL
        || prefix == NULL)
        return NULL;
    type_name = mir_routine_source_local_type_name(routine, local_name);
    if (type_name == NULL || strncmp(type_name, prefix, strlen(prefix)) != 0)
        return NULL;
    if (!llvm_constructed_arg_name_copy(type_name, 0, inner, sizeof(inner))
        || inner[0] == '\0')
        return NULL;
    return pergyra_type_to_llvm(ctx, inner);
}

bool
llvm_mir_async_fact_future_inner_from_source_local(
    const MIRRoutine *routine,
    const char *future_name,
    char *inner_out,
    size_t inner_out_size,
    bool *is_remote_out)
{
    const char *type_name;

    if (inner_out == NULL || inner_out_size == 0)
        return false;
    inner_out[0] = '\0';
    if (is_remote_out != NULL)
        *is_remote_out = false;
    if (routine == NULL || future_name == NULL)
        return false;

    type_name = mir_routine_source_local_type_name(routine, future_name);
    if (type_name == NULL || type_name[0] == '\0')
        return false;
    if (strncmp(type_name, "RemoteFuture<", 13) == 0) {
        if (is_remote_out != NULL)
            *is_remote_out = true;
        return llvm_constructed_arg_name_copy(type_name, 0, inner_out,
            inner_out_size);
    }
    if (strncmp(type_name, "Future<", 7) == 0)
        return llvm_constructed_arg_name_copy(type_name, 0, inner_out,
            inner_out_size);
    return false;
}

LLVMTypeRef
llvm_mir_async_fact_type_from_channel_recv(const MIRRoutine *routine,
                                           LLVMGenCtx *ctx,
                                           const MIRInstruction *inst)
{
    ASTNode *recv;
    ASTNode *channel;

    if (routine == NULL || ctx == NULL || inst == NULL)
        return NULL;
    recv = llvm_mir_async_fact_recv_expr(inst->expr0);
    if (recv == NULL)
        return NULL;
    channel = ast_channel_recv_channel(recv);
    if (channel == NULL || channel->type != AST_IDENTIFIER
        || ast_identifier_name(channel) == NULL)
        return NULL;

    return llvm_mir_async_fact_type_from_local_container(routine, ctx,
        ast_identifier_name(channel), "Channel<");
}

LLVMTypeRef
llvm_mir_async_fact_type_from_await(const MIRRoutine *routine,
                                    LLVMGenCtx *ctx,
                                    const MIRInstruction *inst)
{
    ASTNode *await_expr;
    ASTNode *operand;
    LLVMTypeRef type;

    if (routine == NULL || ctx == NULL || inst == NULL)
        return NULL;
    await_expr = llvm_mir_async_fact_await_expr(inst->expr0);
    if (await_expr == NULL)
        return NULL;
    operand = ast_await_expression(await_expr);
    if (operand == NULL || operand->type != AST_IDENTIFIER
        || ast_identifier_name(operand) == NULL)
        return NULL;

    type = llvm_mir_async_fact_type_from_local_container(routine, ctx,
        ast_identifier_name(operand), "Future<");
    if (ctx->has_error || type != NULL)
        return type;
    return llvm_mir_async_fact_type_from_local_container(routine, ctx,
        ast_identifier_name(operand), "RemoteFuture<");
}

#endif /* PGY_LLVM_ENABLED */
