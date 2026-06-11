#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"
#include "llvm_mir_signature.h"
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
llvm_stmt_array_elem_type_from_type_name(LLVMGenCtx *ctx,
                                         const char *type_name)
{
    char inner_buf[256];

    if (ctx == NULL || type_name == NULL)
        return NULL;
    switch (pgy_classify_type(type_name)) {
    case PGY_TK_ARRAY:
    case PGY_TK_SLICE:
        break;
    default:
        return NULL;
    }
    if (!llvm_constructed_arg_name_copy(type_name, 0,
            inner_buf, sizeof(inner_buf))) {
        return NULL;
    }
    if (strcmp(inner_buf, "Unknown") == 0)
        return NULL;
    return pergyra_type_to_llvm(ctx, inner_buf);
}

static LLVMTypeRef
llvm_stmt_array_elem_type_from_collection_type(LLVMGenCtx *ctx,
                                               LLVMTypeRef type)
{
    const char *struct_name;
    const char *suffix = NULL;

    if (ctx == NULL || type == NULL)
        return NULL;
    if (LLVMGetTypeKind(type) != LLVMStructTypeKind)
        return NULL;

    struct_name = LLVMGetStructName(type);
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

static LLVMTypeRef
llvm_stmt_array_elem_type_from_declared_return(LLVMGenCtx *ctx, ASTNode *call)
{
    ASTNode *decl;
    ASTNode *ret;
    GenericParams *generic_args;
    GenericParam *gp;
    const char *ret_name;
    const char *callee_name;
    bool extern_func;
    const MIRRoutine *routine = NULL;
    bool generic_func = false;

    if (ctx == NULL || call == NULL || call->type != AST_CALL
        || ast_call_callee(call) == NULL
        || ast_call_callee(call)->type != AST_IDENTIFIER
        || ast_identifier_name(ast_call_callee(call)) == NULL) {
        return NULL;
    }

    callee_name = ast_identifier_name(ast_call_callee(call));
    decl = llvm_stmt_find_function_decl_by_name(ctx, callee_name);
    if (decl == NULL || decl->type != AST_FUNC_DECL)
        return NULL;
    extern_func = llvm_decl_is_extern_function(ctx, decl);
    if (llvm_active_has_mir(ctx) && !extern_func)
        routine = llvm_active_function_routine_for_source_ast(ctx, decl);
    generic_func = llvm_mir_or_ast_function_is_generic(routine, decl);
    if (llvm_active_has_mir(ctx) && !generic_func && !extern_func) {
        const char *return_type_name = NULL;
        if (routine == NULL) {
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path missing array return inference routine for '%s'",
                callee_name != NULL ? callee_name : "(anonymous-call)");
            return NULL;
        }
        if (!llvm_mir_routine_signature_metadata_complete_for(ctx,
                routine, decl,
                LLVM_MIR_SIGNATURE_REQUIRE_RETURN_TYPE_NAME,
                "MIR-only LLVM path missing array return inference signature metadata for '%s'",
                "MIR-only LLVM path missing array return inference return type-name metadata for '%s'",
                NULL)) {
            return NULL;
        }
        return_type_name = llvm_mir_routine_return_type_name(routine);
        if (return_type_name != NULL)
            return llvm_stmt_array_elem_type_from_type_name(
                ctx, return_type_name);
        return NULL;
    }

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

    declared = llvm_stmt_array_elem_type_from_declared_return(ctx, receiver);
    if (declared != NULL)
        return declared;
    return llvm_stmt_resolve_array_elem_type(ctx, receiver, NULL);
}

static LLVMTypeRef
llvm_stmt_array_elem_type_from_scope_entry(LLVMGenCtx *ctx, const char *name)
{
    LLVMVarEntry var;

    if (ctx == NULL || name == NULL)
        return NULL;
    if (!llvm_scope_lookup_snapshot(ctx, name, &var) || var.type == NULL)
        return NULL;
    return llvm_stmt_array_elem_type_from_collection_type(ctx, var.type);
}

static LLVMTypeRef
llvm_stmt_array_elem_type_from_current_field(LLVMGenCtx *ctx,
                                             const char *name)
{
    const char *host_name;
    LLVMClassTypeEntry *host_cls;
    int field_idx;

    if (ctx == NULL || name == NULL || strcmp(name, "self") == 0)
        return NULL;

    host_name = llvm_current_host_class_name(ctx);
    host_cls = host_name != NULL ? llvm_lookup_class(ctx, host_name) : NULL;
    field_idx = host_cls != NULL ? llvm_class_field_index(host_cls, name) : -1;
    if (field_idx < 0)
        return NULL;
    return llvm_stmt_array_elem_type_from_collection_type(
        ctx, llvm_class_field_type_at_index(host_cls, field_idx));
}

static LLVMTypeRef
llvm_stmt_array_elem_type_from_member_access(LLVMGenCtx *ctx, ASTNode *expr)
{
    ASTNode *object;
    const char *base_name;
    LLVMClassTypeEntry *base_cls;
    int field_idx;

    if (ctx == NULL || expr == NULL || expr->type != AST_MEMBER_ACCESS
        || ast_member_name(expr) == NULL) {
        return NULL;
    }

    object = ast_member_object(expr);
    if (object != NULL && object->type == AST_IDENTIFIER
        && ast_identifier_name(object) != NULL
        && strcmp(ast_identifier_name(object), "self") == 0) {
        base_name = llvm_current_host_class_name(ctx);
    } else {
        base_name = llvm_stmt_infer_nominal_name_from_init(ctx, object);
    }
    base_cls = base_name != NULL ? llvm_lookup_class(ctx, base_name) : NULL;
    field_idx = base_cls != NULL
        ? llvm_class_field_index(base_cls, ast_member_name(expr)) : -1;
    if (field_idx < 0)
        return NULL;
    return llvm_stmt_array_elem_type_from_collection_type(
        ctx, llvm_class_field_type_at_index(base_cls, field_idx));
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
        inferred = llvm_stmt_array_elem_type_from_current_field(
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

    inferred = llvm_stmt_array_elem_type_from_member_access(ctx, expr);
    if (inferred != NULL)
        return inferred;

    if (elem_type != NULL)
        return elem_type;
    return llvm_stmt_unknown_expr_type(ctx, expr,
        "array or slice element type requires registered Array<T>/Slice<T> metadata");
}

#endif /* PGY_LLVM_ENABLED */
