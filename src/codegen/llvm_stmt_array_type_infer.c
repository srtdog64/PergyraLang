#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"
#include "llvm_stmt_type_infer_helpers.h"
#include "../parser/ast_api.h"

#include <string.h>

static LLVMTypeRef
llvm_stmt_expected_array_elem_type(LLVMGenCtx *ctx)
{
    char inner_buf[256];

    if (ctx == NULL || ctx->expected_type_name == NULL)
        return NULL;
    switch (pgy_classify_type(ctx->expected_type_name)) {
    case PGY_TK_ARRAY:
    case PGY_TK_SLICE:
        break;
    default:
        return NULL;
    }
    if (!llvm_constructed_arg_name_copy(ctx->expected_type_name, 0,
            inner_buf, sizeof(inner_buf))) {
        return NULL;
    }
    if (strcmp(inner_buf, "Unknown") == 0)
        return NULL;
    return pergyra_type_to_llvm(ctx, inner_buf);
}

static LLVMTypeRef
llvm_stmt_array_elem_type_from_declared_return(LLVMGenCtx *ctx, ASTNode *call)
{
    ASTNode *decl;
    ASTNode *ret;
    GenericParams *generic_args;
    GenericParam *gp;
    const char *ret_name;

    if (ctx == NULL || call == NULL || call->type != AST_CALL
        || ast_call_callee(call) == NULL
        || ast_call_callee(call)->type != AST_IDENTIFIER
        || ast_identifier_name(ast_call_callee(call)) == NULL) {
        return NULL;
    }

    decl = llvm_stmt_find_function_decl_by_name(
        ctx, ast_identifier_name(ast_call_callee(call)));
    ret = ast_func_return_type(decl);
    if (ret == NULL || ret->type != AST_TYPE)
        return NULL;

    ret_name = ast_type_name(ret);
    generic_args = ast_type_generic_args(ret);
    gp = ast_generic_param_at(generic_args, 0);
    if (ret_name == NULL
        || (strcmp(ret_name, "Array") != 0 && strcmp(ret_name, "Slice") != 0)
        || gp == NULL) {
        return NULL;
    }

    if (ast_generic_param_name(gp) != NULL) {
        LLVMTypeRef declared =
            pergyra_type_to_llvm(ctx, ast_generic_param_name(gp));
        if (declared != NULL)
            return declared;
    }
    if (ast_generic_param_constraint(gp) != NULL)
        return ast_type_to_llvm(ctx, ast_generic_param_constraint(gp));
    return NULL;
}

static LLVMTypeRef
llvm_stmt_array_elem_type_from_slice_receiver(LLVMGenCtx *ctx,
                                              ASTNode *expr)
{
    ASTNode *receiver;
    LLVMTypeRef declared;

    if (ctx == NULL || expr == NULL || expr->type != AST_CALL
        || ast_call_callee(expr) == NULL
        || ast_call_callee(expr)->type != AST_MEMBER_ACCESS
        || ast_member_name(ast_call_callee(expr)) == NULL
        || strcmp(ast_member_name(ast_call_callee(expr)), "Slice") != 0
        || ast_member_object(ast_call_callee(expr)) == NULL) {
        return NULL;
    }

    receiver = ast_member_object(ast_call_callee(expr));
    if (receiver->type == AST_IDENTIFIER && ast_identifier_name(receiver) != NULL) {
        LLVMArrayVarEntry *entry = llvm_lookup_array_var(
            ctx, ast_identifier_name(receiver));
        if (entry != NULL && entry->elem_type != NULL)
            return entry->elem_type;
    }

    if (receiver->type == AST_CALL
        && ast_call_callee(receiver) != NULL
        && ast_call_callee(receiver)->type == AST_IDENTIFIER
        && ast_identifier_name(ast_call_callee(receiver)) != NULL) {
        ASTNode *decl = llvm_stmt_find_function_decl_by_name(
            ctx, ast_identifier_name(ast_call_callee(receiver)));
        ASTNode *return_type = ast_func_return_type(decl);
        const char *return_type_name = ast_type_name(return_type);
        GenericParams *return_generic_args =
            ast_type_generic_args(return_type);
        if (return_type != NULL
            && return_type->type == AST_TYPE
            && return_type_name != NULL
            && (strcmp(return_type_name, "Array") == 0
                || strcmp(return_type_name, "Slice") == 0)
            && ast_generic_param_at(return_generic_args, 0) != NULL) {
            char *elem_name = llvm_stmt_render_type_arg_scratch(
                ast_generic_param_at(return_generic_args, 0),
                &ctx->scratch);
            if (elem_name == NULL)
                return llvm_stmt_unknown_expr_type(ctx, expr,
                    "Slice() receiver return type is missing its element type");
            return pergyra_type_to_llvm(ctx, elem_name);
        }
    }

    declared = llvm_stmt_array_elem_type_from_declared_return(ctx, receiver);
    if (declared != NULL)
        return declared;
    return llvm_stmt_resolve_array_elem_type(ctx, receiver, NULL);
}

static LLVMTypeRef
llvm_stmt_array_elem_type_from_scope_entry(LLVMGenCtx *ctx, const char *name)
{
    LLVMVarEntry *var;
    const char *struct_name;
    const char *suffix = NULL;

    if (ctx == NULL || name == NULL)
        return NULL;
    var = llvm_scope_lookup(ctx, name);
    if (var == NULL || var->type == NULL)
        return NULL;
    if (LLVMGetTypeKind(var->type) != LLVMStructTypeKind)
        return NULL;

    struct_name = LLVMGetStructName(var->type);
    if (struct_name == NULL)
        return NULL;
    if (strncmp(struct_name, "PgyArray_", 9) == 0) {
        suffix = struct_name + 9;
    } else if (strncmp(struct_name, "PgySlice_", 9) == 0) {
        suffix = struct_name + 9;
    }
    if (suffix == NULL || suffix[0] == '\0'
        || strcmp(suffix, "Unknown") == 0) {
        return NULL;
    }
    return pergyra_type_to_llvm(ctx, suffix);
}

LLVMTypeRef
llvm_stmt_resolve_array_elem_type(LLVMGenCtx *ctx, ASTNode *expr,
                                  LLVMValueRef data_ptr)
{
    LLVMTypeRef elem_type = llvm_stmt_expected_array_elem_type(ctx);
    LLVMTypeRef inferred;
    (void)data_ptr;

    if (expr == NULL)
        return elem_type != NULL ? elem_type
            : llvm_stmt_unknown_expr_type(ctx, expr,
                "array element type requires expected Array<T> metadata");

    if (expr->type == AST_IDENTIFIER && ast_identifier_name(expr) != NULL) {
        LLVMArrayVarEntry *entry = llvm_lookup_array_var(
            ctx, ast_identifier_name(expr));
        if (entry != NULL && entry->elem_type != NULL)
            return entry->elem_type;
        inferred = llvm_stmt_array_elem_type_from_scope_entry(
            ctx, ast_identifier_name(expr));
        if (inferred != NULL)
            return inferred;
    }

    if (expr->type == AST_ARRAY_LITERAL
        && ast_array_literal_count(expr) > 0
        && ast_array_literal_element(expr, 0) != NULL) {
        inferred = llvm_stmt_infer_expr_type(ctx,
            ast_array_literal_element(expr, 0));
        if (inferred != NULL)
            return inferred;
    }

    inferred = llvm_stmt_array_elem_type_from_slice_receiver(ctx, expr);
    if (inferred != NULL)
        return inferred;

    inferred = llvm_stmt_array_elem_type_from_declared_return(ctx, expr);
    if (inferred != NULL)
        return inferred;

    if (elem_type != NULL)
        return elem_type;
    return llvm_stmt_unknown_expr_type(ctx, expr,
        "array or slice element type requires registered Array<T>/Slice<T> metadata");
}

#endif /* PGY_LLVM_ENABLED */
