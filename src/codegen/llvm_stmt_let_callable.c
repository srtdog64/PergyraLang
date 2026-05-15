#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"
#include "parser/ast_api.h"

bool
llvm_stmt_register_callable_let_binding(ASTNode *node, LLVMGenCtx *ctx)
{
    const char *name;
    ASTNode *type_ann;
    ASTNode *init;

    if (node == NULL || node->type != AST_LET_DECL || ctx == NULL)
        return true;

    name = ast_let_name(node);
    type_ann = ast_let_type(node);
    init = ast_let_initializer(node);

    if (type_ann != NULL && type_ann->type == AST_EVENT_HANDLER_TYPE) {
        llvm_register_callable_var(ctx, name, type_ann);
    } else if (init != NULL && init->type == AST_LAMBDA_EXPR) {
        ASTNode **param_types = NULL;
        size_t param_count = ast_lambda_param_count(init);
        if (param_count > 0) {
            param_types = pgy_arena_calloc(&ctx->scratch,
                param_count * sizeof(ASTNode *));
            if (param_types == NULL) {
                llvm_set_error(ctx, "out of memory registering lambda callable");
                return false;
            }
            for (size_t i = 0; i < param_count; i++) {
                ASTNode *p = ast_lambda_param(init, i);
                param_types[i] = (p != NULL && p->type == AST_LET_DECL)
                    ? ast_let_type(p) : NULL;
            }
        }
        llvm_register_callable_signature(ctx, name,
            param_count,
            param_types,
            ast_lambda_return_type(init));
    } else if (init != NULL && init->type == AST_IDENTIFIER
               && ast_identifier_name(init) != NULL) {
        ASTNode *decl = llvm_stmt_find_function_decl_by_name(ctx,
            ast_identifier_name(init));
        if (decl != NULL && decl->type == AST_FUNC_DECL) {
            ASTNode **param_types = NULL;
            size_t param_count = ast_func_param_count(decl);
            if (param_count > 0) {
                param_types = pgy_arena_calloc(&ctx->scratch,
                    param_count * sizeof(ASTNode *));
                if (param_types == NULL) {
                    llvm_set_error(ctx,
                        "out of memory registering function callable");
                    return false;
                }
                for (size_t i = 0; i < param_count; i++) {
                    FuncParam *p = ast_func_param(decl, i);
                    param_types[i] = p != NULL ? p->type : NULL;
                }
            }
            llvm_register_callable_signature(ctx, name,
                param_count,
                param_types,
                ast_func_return_type(decl));
        }
    } else if (init != NULL && init->type == AST_CALL
               && ast_call_callee(init) != NULL
               && ast_call_callee(init)->type == AST_IDENTIFIER
               && ast_identifier_name(ast_call_callee(init)) != NULL) {
        ASTNode *decl = llvm_stmt_find_function_decl_by_name(ctx,
            ast_identifier_name(ast_call_callee(init)));
        if (decl != NULL && decl->type == AST_FUNC_DECL
            && ast_func_return_type(decl) != NULL
            && ast_func_return_type(decl)->type == AST_EVENT_HANDLER_TYPE) {
            llvm_register_callable_var(ctx, name, ast_func_return_type(decl));
        }
    }

    return true;
}

#endif /* PGY_LLVM_ENABLED */
