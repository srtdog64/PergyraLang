/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM role method/operator forward declarations.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_domain_forward_internal.h"
#include "llvm_domain_role_helpers.h"
#include "llvm_inventory_decl_lookup.h"
#include "llvm_inventory_host_methods.h"

static void
llvm_emit_role_method_forward_decls_metadata_first(
    LLVMGenCtx *ctx,
    const char *role_name,
    const LLVMHostedMethodView *methods)
{
    if (ctx == NULL || role_name == NULL || methods == NULL)
        return;
    if (llvm_hosted_method_view_missing_mir_metadata(methods)) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing method forward metadata for role '%s'",
            role_name != NULL ? role_name : "(anonymous-role)");
        return;
    }

    for (size_t j = 0; j < methods->count; j++) {
        const MIRDeclMethod *method_meta =
            llvm_hosted_method_view_metadata(methods, j);
        ASTNode *method = llvm_hosted_method_view_source_ast(methods, j);
        const char *mname =
            llvm_domain_method_name_metadata_first(method_meta, method);
        size_t pc =
            llvm_domain_method_param_count_metadata_first(method_meta, method);
        ASTNode *return_type =
            llvm_domain_method_return_type_metadata_first(method_meta, method);
        LLVMTypeRef ret = ctx->type_void;
        size_t user_pc = 0;
        LLVMTypeRef *ptypes;
        size_t pidx = 1;
        LLVMTypeRef ft;
        char fname[256];
        LLVMValueRef fn;

        if (method_meta == NULL
            && (method == NULL || method->type != AST_FUNC_DECL))
            continue;
        if (mname == NULL)
            continue;
        if (return_type != NULL) {
            ret = ast_type_to_llvm(ctx, return_type);
            if (ctx->has_error || ret == NULL)
                return;
        }

        for (size_t k = 0; k < pc; k++) {
            FuncParam *p =
                llvm_domain_method_param_metadata_first(method_meta, method, k);
            if (!llvm_param_is_implicit_self_local(p))
                user_pc++;
        }

        ptypes = pgy_arena_calloc(&ctx->scratch,
            (user_pc + 1) * sizeof(LLVMTypeRef));
        ptypes[0] = ctx->type_i8ptr;
        for (size_t k = 0; k < pc; k++) {
            FuncParam *p =
                llvm_domain_method_param_metadata_first(method_meta, method, k);
            LLVMTypeRef pt;
            if (llvm_param_is_implicit_self_local(p))
                continue;
            pt = llvm_domain_forward_required_param_type(
                ctx, method, p, "role method", mname);
            if (ctx->has_error || pt == NULL)
                return;
            ptypes[pidx++] = pt;
        }

        ft = LLVMFunctionType(ret, ptypes, (unsigned)(user_pc + 1), 0);
        if (!llvm_role_method_symbol_name(fname, sizeof(fname), role_name,
                mname)) {
            llvm_set_error(ctx,
                "LLVM role method routine name is too long for '%s.%s'",
                role_name, mname);
            return;
        }
        fn = LLVMAddFunction(ctx->module, fname, ft);
        llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ret);
    }
}

static bool
llvm_emit_role_operator_forward_decl(LLVMGenCtx *ctx,
                                     ASTNode *role,
                                     ASTNode *for_type,
                                     const char *for_type_name,
                                     PgyTokenType op)
{
    const char *suffix = llvm_operator_suffix(op);
    ASTNode *method = llvm_find_role_operator_method(ctx, role, op, 0);
    char opname[256];
    FuncParam *rhs_param = NULL;
    size_t rhs_param_count = 0;
    LLVMTypeRef lhs_type;
    LLVMTypeRef rhs_type;
    LLVMTypeRef ret;
    LLVMTypeRef params[2];
    LLVMTypeRef ft;
    LLVMValueRef fn;

    if (suffix == NULL || method == NULL)
        return true;
    if (method->type != AST_FUNC_DECL
        || llvm_domain_method_name_metadata_first(NULL, method) == NULL)
        return true;

    if (!llvm_role_operator_symbol_name(opname, sizeof(opname),
            suffix, for_type_name)) {
        llvm_set_error(ctx,
            "LLVM role operator routine name is too long for '%s'",
            for_type_name);
        return false;
    }
    if (llvm_lookup_function(ctx, opname) != NULL)
        return true;

    for (size_t pj = 0;
         pj < llvm_domain_method_param_count_metadata_first(NULL, method);
         pj++) {
        FuncParam *p =
            llvm_domain_method_param_metadata_first(NULL, method, pj);
        if (!llvm_param_is_implicit_self_local(p)) {
            rhs_param = p;
            rhs_param_count++;
        }
    }
    if (rhs_param_count != 1)
        return true;

    lhs_type = ast_type_to_llvm(ctx, for_type);
    if (ctx->has_error || lhs_type == NULL)
        return false;
    rhs_type = llvm_domain_forward_required_param_type(
        ctx, method, rhs_param, "role operator", opname);
    if (ctx->has_error || rhs_type == NULL)
        return false;
    {
        ASTNode *return_type =
            llvm_domain_method_return_type_metadata_first(NULL, method);
        ret = return_type != NULL
            ? ast_type_to_llvm(ctx, return_type)
            : ctx->type_void;
    }
    if (ctx->has_error || ret == NULL)
        return false;
    params[0] = lhs_type;
    params[1] = rhs_type;
    ft = LLVMFunctionType(ret, params, 2, 0);
    fn = LLVMAddFunction(ctx->module, opname, ft);
    llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ret);
    return true;
}

void
llvm_emit_domain_role_forward_decls(LLVMGenCtx *ctx,
                                    ASTNode **roles,
                                    size_t role_count)
{
    static const PgyTokenType ops[] = {
        TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, TOKEN_SLASH, TOKEN_PERCENT,
        TOKEN_EQUAL, TOKEN_NOT_EQUAL, TOKEN_LESS, TOKEN_LESS_EQUAL,
        TOKEN_GREATER, TOKEN_GREATER_EQUAL
    };

    if (ctx == NULL)
        return;

    for (size_t i = 0; i < role_count; i++) {
        ASTNode *stmt = roles[i];
        const char *role_name;
        ASTNode *for_type;
        const char *for_type_name;

        if (stmt == NULL || stmt->type != AST_ROLE_DECL)
            continue;

        role_name = llvm_decl_node_name(stmt);
        {
            LLVMHostedMethodView method_view =
                llvm_hosted_method_view_from_decl(ctx, role_name, stmt);
            llvm_emit_role_method_forward_decls_metadata_first(
                ctx, role_name, &method_view);
            if (ctx->has_error)
                return;
        }

        for_type = llvm_role_for_type_node(stmt);
        for_type_name = llvm_role_for_type_name(stmt);
        for (size_t oi = 0; for_type_name != NULL
               && oi < sizeof(ops) / sizeof(ops[0]); oi++) {
            if (!llvm_emit_role_operator_forward_decl(
                    ctx, stmt, for_type, for_type_name, ops[oi]))
                return;
        }
    }
}

#endif
