/*
 * LLVM MIR Slice receiver and result facts.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_mir_slice_fact.h"

#include <string.h>

#include "llvm_internal_api.h"
#include "parser/ast_api.h"

static ASTNode *
llvm_mir_slice_fact_initializer_expr(ASTNode *expr)
{
    if (expr != NULL && expr->type == AST_LET_DECL)
        return ast_let_initializer(expr);
    return expr;
}

static LLVMMirVar *
llvm_mir_slice_fact_var_entry(LLVMMirVar *vars, size_t var_count,
                              const char *name)
{
    char base_name[128];

    if (vars == NULL || name == NULL)
        return NULL;

    LLVMMirVar *entry = llvm_mir_get_var_entry(vars, var_count, name);
    if (entry != NULL)
        return entry;

    for (size_t i = var_count; i > 0; i--) {
        const char *mir_name = vars[i - 1].mir_name;
        if (mir_name == NULL)
            continue;
        if (!llvm_mir_base_name_from_versioned(mir_name, base_name,
                sizeof(base_name)))
            continue;
        if (strcmp(base_name, name) == 0)
            return &vars[i - 1];
    }

    return NULL;
}

static LLVMTypeRef
llvm_mir_slice_fact_elem_type_from_slice_type(LLVMGenCtx *ctx,
                                              LLVMTypeRef slice_type)
{
    if (ctx == NULL || slice_type == NULL)
        return NULL;
    if (slice_type == ctx->slice_type_Int)
        return ctx->type_i32;
    if (slice_type == ctx->slice_type_Long)
        return ctx->type_i64;
    if (slice_type == ctx->slice_type_Float)
        return ctx->type_f32;
    if (slice_type == ctx->slice_type_Double)
        return ctx->type_f64;
    if (slice_type == ctx->slice_type_Bool)
        return ctx->type_i1;
    if (slice_type == ctx->slice_type_String)
        return ctx->type_i8ptr;
    return NULL;
}

LLVMTypeRef
llvm_mir_slice_fact_array_type_from_slice_type(LLVMGenCtx *ctx,
                                               LLVMTypeRef slice_type)
{
    if (ctx == NULL || slice_type == NULL)
        return NULL;
    if (slice_type == ctx->slice_type_Int)
        return ctx->array_type_Int;
    if (slice_type == ctx->slice_type_Long)
        return ctx->array_type_Long;
    if (slice_type == ctx->slice_type_Float)
        return ctx->array_type_Float;
    if (slice_type == ctx->slice_type_Double)
        return ctx->array_type_Double;
    if (slice_type == ctx->slice_type_Bool)
        return ctx->array_type_Bool;
    if (slice_type == ctx->slice_type_String)
        return ctx->array_type_String;
    return NULL;
}

LLVMTypeRef
llvm_mir_slice_fact_elem_type_from_receiver(LLVMGenCtx *ctx,
                                            ASTNode *receiver,
                                            LLVMMirVar *vars,
                                            size_t var_count)
{
    if (ctx == NULL || receiver == NULL)
        return NULL;

    if (receiver->type == AST_IDENTIFIER
        && ast_identifier_name(receiver) != NULL) {
        const char *receiver_name = ast_identifier_name(receiver);
        LLVMMirVar *receiver_var = llvm_mir_slice_fact_var_entry(
            vars, var_count, receiver_name);
        LLVMArrayVarEntry *entry = receiver_var != NULL
            ? llvm_lookup_array_var_binding(ctx, receiver_name,
                  receiver_var->alloca)
            : llvm_lookup_array_var(ctx, receiver_name);
        if (entry != NULL && entry->elem_type != NULL)
            return entry->elem_type;
    }

    if (receiver->type == AST_CALL
        && ast_call_callee(receiver) != NULL
        && ast_call_callee(receiver)->type == AST_MEMBER_ACCESS) {
        LLVMTypeRef slice_type = llvm_mir_slice_fact_type_from_call(ctx,
            receiver, vars, var_count);
        LLVMTypeRef elem_type = llvm_mir_slice_fact_elem_type_from_slice_type(
            ctx, slice_type);
        if (elem_type != NULL)
            return elem_type;
    }

    return llvm_stmt_resolve_array_elem_type(ctx, receiver, NULL);
}

LLVMTypeRef
llvm_mir_slice_fact_type_from_call(LLVMGenCtx *ctx,
                                   ASTNode *expr,
                                   LLVMMirVar *vars,
                                   size_t var_count)
{
    ASTNode *value_expr = llvm_mir_slice_fact_initializer_expr(expr);
    ASTNode *callee;
    ASTNode *receiver;
    LLVMTypeRef elem_type;
    const char *suffix;

    if (ctx == NULL || value_expr == NULL || value_expr->type != AST_CALL)
        return NULL;
    callee = ast_call_callee(value_expr);
    if (callee == NULL || callee->type != AST_MEMBER_ACCESS
        || ast_member_name(callee) == NULL
        || strcmp(ast_member_name(callee), "Slice") != 0) {
        return NULL;
    }

    receiver = ast_member_object(callee);
    elem_type = llvm_mir_slice_fact_elem_type_from_receiver(ctx, receiver,
        vars, var_count);
    suffix = llvm_type_to_suffix(ctx, elem_type);
    if (suffix == NULL || strcmp(suffix, "Unknown") == 0)
        return NULL;
    return llvm_slice_struct_type(ctx, suffix);
}

#endif /* PGY_LLVM_ENABLED */
