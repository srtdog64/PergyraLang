#ifdef PGY_LLVM_ENABLED
#include "llvm_stmt_type_infer_helpers.h"

#include <stdio.h>
#include <string.h>

static LLVMTypeRef
llvm_stmt_await_unknown_type(LLVMGenCtx *ctx, ASTNode *expr,
                             const char *reason)
{
    if (ctx == NULL)
        return NULL;
    if (!ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, expr,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "LLVM await expression type inference requires Future<T> metadata: %s",
            reason != NULL ? reason : "unknown await expression");
    }
    return ctx->type_i32;
}

static LLVMTypeRef
llvm_stmt_remote_await_result_type(LLVMGenCtx *ctx, LLVMTypeRef inner_ty)
{
    LLVMTypeRef fields[] = { ctx->type_i32, inner_ty, ctx->type_i8ptr };
    return LLVMStructTypeInContext(ctx->context, fields, 3, 0);
}

LLVMTypeRef
llvm_stmt_infer_await_expr_type(LLVMGenCtx *ctx, ASTNode *expr)
{
    ASTNode *operand;
    const char *future_name;
    const char *inner;
    LLVMTypeRef inner_ty;

    if (ctx == NULL)
        return NULL;
    if (expr == NULL || expr->type != AST_AWAIT_EXPR)
        return llvm_stmt_await_unknown_type(ctx, expr,
            "expected an await expression");

    operand = ast_await_expression(expr);
    if (operand == NULL)
        return llvm_stmt_await_unknown_type(ctx, expr,
            "missing Future<T> operand");
    if (operand->type != AST_IDENTIFIER)
        return llvm_stmt_await_unknown_type(ctx, expr,
            "operand must be a named Future<T> binding");

    future_name = ast_identifier_name(operand);
    if (future_name == NULL || future_name[0] == '\0')
        return llvm_stmt_await_unknown_type(ctx, expr,
            "operand identifier is missing a Future<T> binding name");

    inner = llvm_lookup_future_inner(ctx, future_name);
    if (inner == NULL || inner[0] == '\0') {
        char reason[256];
        int written = snprintf(reason, sizeof(reason),
            "operand '%s' has no registered Future<T> metadata",
            future_name != NULL ? future_name : "<future>");
        return llvm_stmt_await_unknown_type(ctx, expr,
            written >= 0 && (size_t)written < sizeof(reason)
                ? reason : "operand has no registered Future<T> metadata");
    }

    if (strcmp(inner, "Void") == 0) {
        if (!llvm_lookup_future_is_remote(ctx, future_name))
            return ctx->type_i32;
        return llvm_stmt_remote_await_result_type(ctx, ctx->type_i32);
    }

    inner_ty = pergyra_type_to_llvm(ctx, inner);
    if (ctx->has_error || inner_ty == NULL)
        return NULL;
    if (!llvm_lookup_future_is_remote(ctx, future_name))
        return inner_ty;

    return llvm_stmt_remote_await_result_type(ctx, inner_ty);
}

#endif
