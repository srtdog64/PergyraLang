#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"
#include "llvm_internal_api.h"
#include "parser/ast_api.h"

static ASTNode *
llvm_stmt_lambda_expected_return_type(LLVMGenCtx *ctx, ASTNode *lambda)
{
    ASTNode *expected;

    if (ctx == NULL || lambda == NULL)
        return NULL;
    expected = ctx->expected_callable_type;
    if (expected == NULL || expected->type != AST_EVENT_HANDLER_TYPE)
        return NULL;
    if (ast_event_handler_param_count(expected)
        != ast_lambda_param_count(lambda)) {
        return NULL;
    }
    return ast_event_handler_return_type(expected);
}

ASTNode *
llvm_stmt_current_return_callable_type(LLVMGenCtx *ctx)
{
    return ctx != NULL ? ctx->current_return_callable_type : NULL;
}

static ASTNode *
llvm_stmt_lambda_expected_param_type_at(LLVMGenCtx *ctx, ASTNode *lambda,
                                        size_t param_index)
{
    ASTNode *expected;
    size_t param_count;

    if (ctx == NULL || lambda == NULL)
        return NULL;
    expected = ctx->expected_callable_type;
    if (expected == NULL || expected->type != AST_EVENT_HANDLER_TYPE)
        return NULL;
    param_count = ast_lambda_param_count(lambda);
    if (ast_event_handler_param_count(expected) != param_count)
        return NULL;
    if (param_index >= param_count)
        return NULL;
    return ast_event_handler_param_type(expected, param_index);
}

LLVMTypeRef
llvm_stmt_lambda_return_type(LLVMGenCtx *ctx, ASTNode *expr)
{
    ASTNode *return_type;
    ASTNode *body;
    int pc;
    LLVMTypeRef param_types[8];
    const char *param_names[8];

    if (ctx == NULL || expr == NULL || expr->type != AST_LAMBDA_EXPR)
        return NULL;

    return_type = ast_lambda_return_type(expr);
    if (return_type != NULL) {
        LLVMTypeRef ret_type = ast_type_to_llvm(ctx, return_type);
        if (ctx->has_error || ret_type == NULL)
            return NULL;
        return ret_type;
    }
    return_type = llvm_stmt_lambda_expected_return_type(ctx, expr);
    if (return_type != NULL) {
        LLVMTypeRef ret_type = ast_type_to_llvm(ctx, return_type);
        if (ctx->has_error || ret_type == NULL)
            return NULL;
        return ret_type;
    }

    body = ast_lambda_body(expr);
    if (body != NULL && body->type == AST_BLOCK)
        return ctx->type_void;
    if (body != NULL) {
        pc = (int)ast_lambda_param_count(expr);
        if (pc > 8) {
            llvm_set_error_at_with_hints(ctx, expr,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_REFACTOR_OR_RAISE_LIMIT,
                "LLVM lambda return inference supports at most 8 parameters");
            return NULL;
        }
        for (int i = 0; i < pc; i++) {
            ASTNode *param = ast_lambda_param(expr, (size_t)i);
            if (param == NULL) {
                llvm_set_error_at_with_hints(ctx, expr,
                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                    PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                    "LLVM lambda return inference requires named typed parameters");
                return NULL;
            }
            param_names[i] = param->type == AST_LET_DECL
                ? ast_let_name(param)
                : ast_identifier_name(param);
            if (param_names[i] == NULL || param_names[i][0] == '\0') {
                llvm_set_error_at_with_hints(ctx, param,
                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                    PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                    "LLVM lambda return inference requires named parameters");
                return NULL;
            }
            param_types[i] = llvm_stmt_lambda_param_type(ctx, expr, param,
                (size_t)i);
            if (ctx->has_error || param_types[i] == NULL)
                return NULL;
        }
        LLVMLexicalRegistrySnapshot lexical_snapshot =
            llvm_lexical_registry_snapshot(ctx);
        llvm_scope_push(ctx);
        if (ctx->has_error) {
            llvm_lexical_registry_restore(ctx, lexical_snapshot);
            return NULL;
        }
        for (int i = 0; i < pc; i++)
            llvm_scope_declare(ctx, param_names[i], NULL, param_types[i]);
        LLVMTypeRef inferred = llvm_stmt_infer_expr_type(ctx, body);
        llvm_scope_pop(ctx);
        llvm_lexical_registry_restore(ctx, lexical_snapshot);
        if (ctx->has_error || inferred == NULL)
            return NULL;
        return inferred;
    }

    llvm_set_error_at_with_hints(ctx, expr,
        PGY_CODE_LLVM_TYPE_UNSUPPORTED,
        PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
        PGY_FIX_ANNOTATE_CONCRETE_TYPE,
        "LLVM lambda return type requires an explicit annotation or inferable expression body");
    return NULL;
}

LLVMTypeRef
llvm_stmt_lambda_param_type(LLVMGenCtx *ctx, ASTNode *lambda, ASTNode *param,
                            size_t param_index)
{
    ASTNode *param_type;
    ASTNode *return_type;
    ASTNode *body;
    ASTNode *returned;
    const char *param_name;

    if (ctx == NULL || lambda == NULL || param == NULL)
        return NULL;

    if (param->type == AST_LET_DECL) {
        param_type = ast_let_type(param);
        if (param_type != NULL) {
            LLVMTypeRef lowered = ast_type_to_llvm(ctx, param_type);
            if (ctx->has_error || lowered == NULL)
                return NULL;
            return lowered;
        }
        param_name = ast_let_name(param);
    } else {
        param_name = ast_identifier_name(param);
    }

    param_type = llvm_stmt_lambda_expected_param_type_at(ctx, lambda,
        param_index);
    if (param_type != NULL) {
        LLVMTypeRef lowered = ast_type_to_llvm(ctx, param_type);
        if (ctx->has_error || lowered == NULL)
            return NULL;
        return lowered;
    }

    return_type = ast_lambda_return_type(lambda);
    body = ast_lambda_body(lambda);
    returned = NULL;
    if (body != NULL && body->type == AST_IDENTIFIER) {
        returned = body;
    } else if (body != NULL
               && body->type == AST_BLOCK
               && ast_block_statement_count(body) == 1
               && ast_block_statement(body, 0) != NULL
               && ast_block_statement(body, 0)->type == AST_RETURN) {
        returned = ast_return_value(ast_block_statement(body, 0));
    }
    if (return_type != NULL
        && returned != NULL
        && returned->type == AST_IDENTIFIER
        && param_name != NULL
        && ast_identifier_name(returned) != NULL
        && strcmp(ast_identifier_name(returned), param_name) == 0) {
        LLVMTypeRef lowered = ast_type_to_llvm(ctx, return_type);
        if (ctx->has_error || lowered == NULL)
            return NULL;
        return lowered;
    }

    llvm_set_error_at_with_hints(ctx, param,
        PGY_CODE_LLVM_TYPE_UNSUPPORTED,
        PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
        PGY_FIX_ANNOTATE_CONCRETE_TYPE,
        "LLVM lambda parameter '%s' requires an explicit type annotation",
        param_name != NULL ? param_name : "<param>");
    return NULL;
}

LLVMTypeRef
llvm_stmt_lambda_signature_type(LLVMGenCtx *ctx, ASTNode *expr)
{
    int pc;
    LLVMTypeRef *params = NULL;
    LLVMTypeRef ret_type;

    if (ctx == NULL || expr == NULL || expr->type != AST_LAMBDA_EXPR)
        return NULL;

    pc = (int)ast_lambda_param_count(expr);
    ret_type = llvm_stmt_lambda_return_type(ctx, expr);
    if (ctx->has_error || ret_type == NULL)
        return NULL;

    if (pc > 0) {
        params = pgy_arena_calloc(&ctx->scratch,
            (size_t)pc * sizeof(LLVMTypeRef));
        if (params == NULL) {
            llvm_set_error_at_with_hints(ctx, expr,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_INSPECT_MIR_INVENTORY,
                "LLVM lambda signature parameter allocation failed");
            return NULL;
        }
        for (int i = 0; i < pc; i++) {
            ASTNode *p = ast_lambda_param(expr, (size_t)i);
            params[i] = llvm_stmt_lambda_param_type(ctx, expr, p, (size_t)i);
            if (ctx->has_error || params[i] == NULL)
                return NULL;
        }
    }

    return LLVMPointerType(
        LLVMFunctionType(ret_type, params, (unsigned)pc, 0), 0);
}

#endif
