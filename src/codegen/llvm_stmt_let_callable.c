#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"
#include "llvm_mir_signature.h"
#include "parser/ast_api.h"

static bool
llvm_stmt_register_callable_from_function_decl(LLVMGenCtx *ctx,
                                               const char *name,
                                               ASTNode *decl)
{
    ASTNode **param_types = NULL;
    ASTNode *return_type = NULL;
    size_t param_count = 0;
    bool extern_func;
    const MIRRoutine *routine = NULL;
    bool generic_func = false;

    if (ctx == NULL || name == NULL || decl == NULL
        || decl->type != AST_FUNC_DECL) {
        return true;
    }

    extern_func = llvm_decl_is_extern_function(ctx, decl);
    if (llvm_active_has_mir(ctx) && !extern_func)
        routine = llvm_active_function_routine_for_source_ast(ctx, decl);
    generic_func = llvm_mir_or_ast_function_is_generic(routine, decl);
    if (llvm_active_has_mir(ctx) && !generic_func && !extern_func) {
        if (routine == NULL) {
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path missing callable let routine for '%s'",
                ast_declaration_name(decl) != NULL
                    ? ast_declaration_name(decl)
                    : "(anonymous-callable)");
            return false;
        }
        if (!llvm_mir_routine_signature_metadata_complete(ctx,
                routine, decl,
                "MIR-only LLVM path missing callable let signature metadata for '%s'",
                "MIR-only LLVM path missing callable let return type-name metadata for '%s'",
                "MIR-only LLVM path missing callable let parameter type-name metadata for '%s'")) {
            return false;
        }
        param_count = llvm_mir_routine_param_count(routine);
        if (param_count > 0) {
            param_types = pgy_arena_calloc(&ctx->scratch,
                param_count * sizeof(ASTNode *));
            if (param_types == NULL) {
                llvm_set_error(ctx,
                    "out of memory registering function callable");
                return false;
            }
            for (size_t i = 0; i < param_count; i++) {
                FuncParam *p = llvm_mir_routine_param(routine, i);
                param_types[i] = p != NULL ? p->type : NULL;
            }
        }
        return_type = llvm_mir_routine_return_type(routine);
        llvm_register_callable_signature(ctx, name,
            param_count, param_types, return_type);
        return true;
    }

    param_count = ast_func_param_count(decl);
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
        param_count, param_types, ast_func_return_type(decl));
    return true;
}

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
        if (!llvm_stmt_register_callable_from_function_decl(ctx, name, decl))
            return false;
    } else if (init != NULL && init->type == AST_CALL
               && ast_call_callee(init) != NULL
               && ast_call_callee(init)->type == AST_IDENTIFIER
               && ast_identifier_name(ast_call_callee(init)) != NULL) {
        ASTNode *decl = llvm_stmt_find_function_decl_by_name(ctx,
            ast_identifier_name(ast_call_callee(init)));
        if (decl != NULL && decl->type == AST_FUNC_DECL) {
            ASTNode *return_type = NULL;
            bool extern_func = llvm_decl_is_extern_function(ctx, decl);
            const MIRRoutine *routine = NULL;
            bool generic_func = false;
            if (llvm_active_has_mir(ctx) && !extern_func)
                routine = llvm_active_function_routine_for_source_ast(ctx,
                    decl);
            generic_func = llvm_mir_or_ast_function_is_generic(routine, decl);
            if (llvm_active_has_mir(ctx) && !generic_func && !extern_func) {
                if (routine == NULL) {
                    llvm_set_mir_inventory_missing(ctx,
                        "MIR-only LLVM path missing callable call-return routine for '%s'",
                        ast_declaration_name(decl) != NULL
                            ? ast_declaration_name(decl)
                            : "(anonymous-callable)");
                    return false;
                }
                if (!llvm_mir_routine_signature_metadata_complete(ctx,
                        routine, decl,
                        "MIR-only LLVM path missing callable call-return signature metadata for '%s'",
                        "MIR-only LLVM path missing callable call-return return type-name metadata for '%s'",
                        "MIR-only LLVM path missing callable call-return parameter type-name metadata for '%s'")) {
                    return false;
                }
                return_type = llvm_mir_routine_return_type(routine);
            } else {
                return_type = ast_func_return_type(decl);
            }
            if (return_type != NULL
                && return_type->type == AST_EVENT_HANDLER_TYPE) {
                llvm_register_callable_var(ctx, name, return_type);
            }
        }
    }

    return true;
}

#endif /* PGY_LLVM_ENABLED */
