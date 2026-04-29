/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM domain forward declarations and vtable registration.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_domain_forward.h"
#include "llvm_domain_role_helpers.h"
#include "llvm_inventory_host_methods.h"

static const char *
llvm_domain_method_name_metadata_first(const MIRDeclMethod *method_meta,
                                       ASTNode *method)
{
    const char *name = llvm_mir_decl_method_name(method_meta);
    if (name != NULL)
        return name;
    if (method != NULL && method->type == AST_FUNC_DECL)
        return method->data.func_decl.name;
    return NULL;
}

static size_t
llvm_domain_method_param_count_metadata_first(const MIRDeclMethod *method_meta,
                                              ASTNode *method)
{
    if (method_meta != NULL)
        return llvm_mir_decl_method_param_count(method_meta);
    if (method != NULL && method->type == AST_FUNC_DECL)
        return method->data.func_decl.param_count;
    return 0;
}

static FuncParam *
llvm_domain_method_param_metadata_first(const MIRDeclMethod *method_meta,
                                        ASTNode *method,
                                        size_t index)
{
    FuncParam *param = llvm_mir_decl_method_param(method_meta, index);
    if (param != NULL)
        return param;
    if (method != NULL && method->type == AST_FUNC_DECL
        && index < method->data.func_decl.param_count) {
        return method->data.func_decl.params[index];
    }
    return NULL;
}

static ASTNode *
llvm_domain_method_return_type_metadata_first(const MIRDeclMethod *method_meta,
                                              ASTNode *method)
{
    ASTNode *return_type = llvm_mir_decl_method_return_type(method_meta);
    if (return_type != NULL)
        return return_type;
    if (method != NULL && method->type == AST_FUNC_DECL)
        return method->data.func_decl.return_type;
    return NULL;
}

void
llvm_emit_domain_sync_forward_decl(LLVMGenCtx *ctx,
                                   const char *decl_name,
                                   LLVMTypeRef struct_ty,
                                   LLVMClassTypeEntry *entry)
{
    char sync_name[256];
    LLVMTypeRef sync_params[] = { LLVMPointerType(struct_ty, 0) };
    LLVMTypeRef sync_ft;
    LLVMValueRef sync_fn;

    if (ctx == NULL || decl_name == NULL || struct_ty == NULL)
        return;

    sync_ft = LLVMFunctionType(ctx->type_void, sync_params, 1, 0);
    snprintf(sync_name, sizeof(sync_name), "%s_sync", decl_name);
    sync_fn = LLVMAddFunction(ctx->module, sync_name, sync_ft);
    llvm_register_function(ctx, LLVMGetValueName(sync_fn),
        sync_fn, sync_ft, ctx->type_void);
    if (entry != NULL)
        entry->sync_function_name = pergyra_strdup(sync_name);
}

void
llvm_emit_domain_method_forward_decls(LLVMGenCtx *ctx,
                                      const char *decl_name,
                                      LLVMTypeRef struct_ty,
                                      ASTNode **methods,
                                      size_t method_count)
{
    if (ctx == NULL || decl_name == NULL || struct_ty == NULL)
        return;

    for (size_t j = 0; j < method_count; j++) {
        ASTNode *method = methods[j];
        const MIRDeclMethod *method_meta;
        const char *mname;
        ASTNode *return_type;
        size_t pc;
        LLVMTypeRef ret;
        size_t user_pc = 0;
        LLVMTypeRef *ptypes;
        size_t pidx = 1;
        LLVMTypeRef ft;
        char fname[256];
        LLVMValueRef fn;

        if (method == NULL || method->type != AST_FUNC_DECL)
            continue;

        method_meta = llvm_find_host_method_metadata_in_context(
            ctx, decl_name, method->data.func_decl.name);
        mname = llvm_domain_method_name_metadata_first(method_meta, method);
        pc = llvm_domain_method_param_count_metadata_first(method_meta, method);
        ret = ctx->type_void;
        return_type =
            llvm_domain_method_return_type_metadata_first(method_meta, method);
        if (return_type != NULL)
            ret = ast_type_to_llvm(ctx, return_type);

        for (size_t k = 0; k < pc; k++) {
            FuncParam *p =
                llvm_domain_method_param_metadata_first(method_meta, method, k);
            if (!llvm_param_is_implicit_self_local(p))
                user_pc++;
        }

        ptypes = pgy_arena_calloc(&ctx->scratch,
            (user_pc + 1) * sizeof(LLVMTypeRef));
        ptypes[0] = LLVMPointerType(struct_ty, 0);
        for (size_t k = 0; k < pc; k++) {
            FuncParam *p =
                llvm_domain_method_param_metadata_first(method_meta, method, k);
            const char *type_name = NULL;
            LLVMClassTypeEntry *param_cls = NULL;
            if (llvm_param_is_implicit_self_local(p))
                continue;
            if (p->type != NULL && p->type->type == AST_TYPE)
                type_name = p->type->data.type.name;
            param_cls = type_name != NULL ? llvm_lookup_class(ctx, type_name) : NULL;
            if (param_cls != NULL && param_cls->is_pointer_self_host)
                ptypes[pidx++] = LLVMPointerType(param_cls->struct_type, 0);
            else
                ptypes[pidx++] = (p->type != NULL)
                    ? ast_type_to_llvm(ctx, p->type)
                    : ctx->type_i32;
        }

        ft = LLVMFunctionType(ret, ptypes, (unsigned)(user_pc + 1), 0);
        if (mname == NULL)
            continue;
        snprintf(fname, sizeof(fname), "%s_%s", decl_name, mname);
        fn = LLVMAddFunction(ctx->module, fname, ft);
        llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ret);
    }
}

void
llvm_emit_domain_ability_vtables(LLVMGenCtx *ctx,
                                 ASTNode **abilities,
                                 size_t ability_count)
{
    if (ctx == NULL)
        return;

    for (size_t i = 0; i < ability_count; i++) {
        ASTNode *stmt = abilities[i];
        const char *ab_name;
        size_t mc;
        LLVMTypeRef *vt_fields;
        char vt_name[256];
        LLVMTypeRef vt_struct;
        LLVMClassTypeEntry *entry;

        if (stmt == NULL || stmt->type != AST_ABILITY_DECL)
            continue;

        ab_name = stmt->data.ability_decl.name;
        mc = stmt->data.ability_decl.method_count;
        vt_fields = pgy_arena_calloc(&ctx->scratch,
            (mc > 0 ? mc : 1) * sizeof(LLVMTypeRef));
        for (size_t j = 0; j < mc; j++) {
            ASTNode *method = stmt->data.ability_decl.methods[j];
            LLVMTypeRef ret;
            size_t pc;
            size_t user_pc = 0;
            LLVMTypeRef *ptypes;
            size_t pidx = 1;
            LLVMTypeRef fn_type;

            if (method == NULL || method->type != AST_FUNC_DECL) {
                vt_fields[j] = ctx->type_i8ptr;
                continue;
            }

            ret = ctx->type_void;
            if (method->data.func_decl.return_type != NULL)
                ret = ast_type_to_llvm(ctx, method->data.func_decl.return_type);

            pc = method->data.func_decl.param_count;
            for (size_t k = 0; k < pc; k++) {
                FuncParam *p = method->data.func_decl.params[k];
                if (!llvm_param_is_implicit_self_local(p))
                    user_pc++;
            }
            ptypes = pgy_arena_calloc(&ctx->scratch,
                (user_pc + 1) * sizeof(LLVMTypeRef));
            ptypes[0] = ctx->type_i8ptr;
            for (size_t k = 0; k < pc; k++) {
                FuncParam *p = method->data.func_decl.params[k];
                if (llvm_param_is_implicit_self_local(p))
                    continue;
                ptypes[pidx++] = (p->type != NULL)
                    ? ast_type_to_llvm(ctx, p->type)
                    : ctx->type_i32;
            }

            fn_type = LLVMFunctionType(ret, ptypes, (unsigned)(user_pc + 1), 0);
            vt_fields[j] = LLVMPointerType(fn_type, 0);
        }

        snprintf(vt_name, sizeof(vt_name), "%s_vtable", ab_name);
        vt_struct = LLVMStructCreateNamed(ctx->context, vt_name);
        LLVMStructSetBody(vt_struct, vt_fields, (unsigned)mc, 0);
        entry = llvm_register_class(ctx, pergyra_strdup(vt_name), vt_struct, false, false);
        if (entry != NULL) {
            for (size_t j = 0; j < mc; j++) {
                ASTNode *method = stmt->data.ability_decl.methods[j];
                if (method != NULL && method->type == AST_FUNC_DECL)
                    llvm_class_add_field(entry,
                        method->data.func_decl.name,
                        LLVMStructGetTypeAtIndex(vt_struct, (unsigned)j),
                        (int)j);
            }
        }
    }
}

void
llvm_emit_domain_role_forward_decls(LLVMGenCtx *ctx,
                                    ASTNode **roles,
                                    size_t role_count)
{
    if (ctx == NULL)
        return;

    for (size_t i = 0; i < role_count; i++) {
        ASTNode *stmt = roles[i];
        const char *role_name;

        if (stmt == NULL || stmt->type != AST_ROLE_DECL)
            continue;

        role_name = stmt->data.role_decl.name;
        for (size_t ii = 0; ii < stmt->data.role_decl.impl_count; ii++) {
            ASTNode *impl = stmt->data.role_decl.impl_abilities[ii];
            if (impl == NULL || impl->type != AST_IMPL_ABILITY)
                continue;

            for (size_t j = 0; j < impl->data.impl_ability.method_count; j++) {
                ASTNode *method = impl->data.impl_ability.methods[j];
                const char *mname;
                size_t pc;
                LLVMTypeRef ret;
                size_t user_pc = 0;
                LLVMTypeRef *ptypes;
                size_t pidx = 1;
                LLVMTypeRef ft;
                char fname[256];
                LLVMValueRef fn;

                if (method == NULL || method->type != AST_FUNC_DECL)
                    continue;

                mname = method->data.func_decl.name;
                pc = method->data.func_decl.param_count;
                ret = ctx->type_void;
                if (method->data.func_decl.return_type != NULL)
                    ret = ast_type_to_llvm(ctx, method->data.func_decl.return_type);

                for (size_t k = 0; k < pc; k++) {
                    FuncParam *p = method->data.func_decl.params[k];
                    if (!llvm_param_is_implicit_self_local(p))
                        user_pc++;
                }

                ptypes = pgy_arena_calloc(&ctx->scratch,
                    (user_pc + 1) * sizeof(LLVMTypeRef));
                ptypes[0] = ctx->type_i8ptr;
                for (size_t k = 0; k < pc; k++) {
                    FuncParam *p = method->data.func_decl.params[k];
                    if (llvm_param_is_implicit_self_local(p))
                        continue;
                    ptypes[pidx++] = (p->type != NULL)
                        ? ast_type_to_llvm(ctx, p->type)
                        : ctx->type_i32;
                }

                ft = LLVMFunctionType(ret, ptypes, (unsigned)(user_pc + 1), 0);
                snprintf(fname, sizeof(fname), "%s_%s", role_name, mname);
                fn = LLVMAddFunction(ctx->module, fname, ft);
                llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ret);
            }
        }

        {
            PgyTokenType ops[] = {
                TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, TOKEN_SLASH, TOKEN_PERCENT,
                TOKEN_EQUAL, TOKEN_NOT_EQUAL, TOKEN_LESS, TOKEN_LESS_EQUAL,
                TOKEN_GREATER, TOKEN_GREATER_EQUAL
            };
            const char *for_type_name = NULL;
            if (stmt->data.role_decl.for_type != NULL
                && stmt->data.role_decl.for_type->type == AST_TYPE)
                for_type_name = stmt->data.role_decl.for_type->data.type.name;

            for (size_t oi = 0; for_type_name != NULL
                   && oi < sizeof(ops) / sizeof(ops[0]); oi++) {
                const char *suffix = llvm_operator_suffix(ops[oi]);
                ASTNode *method = llvm_find_role_operator_method(ctx, stmt, ops[oi], 0);
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
                    continue;

                snprintf(opname, sizeof(opname), "operator_%s_%s", suffix, for_type_name);
                if (llvm_lookup_function(ctx, opname) != NULL)
                    continue;

                for (size_t pj = 0; pj < method->data.func_decl.param_count; pj++) {
                    FuncParam *p = method->data.func_decl.params[pj];
                    if (!llvm_param_is_implicit_self_local(p)) {
                        rhs_param = p;
                        rhs_param_count++;
                    }
                }
                if (rhs_param_count != 1)
                    continue;

                lhs_type = ast_type_to_llvm(ctx, stmt->data.role_decl.for_type);
                rhs_type = (rhs_param != NULL && rhs_param->type != NULL)
                    ? ast_type_to_llvm(ctx, rhs_param->type) : ctx->type_i32;
                ret = method->data.func_decl.return_type != NULL
                    ? ast_type_to_llvm(ctx, method->data.func_decl.return_type)
                    : ctx->type_void;
                params[0] = lhs_type;
                params[1] = rhs_type;
                ft = LLVMFunctionType(ret, params, 2, 0);
                fn = LLVMAddFunction(ctx->module, opname, ft);
                llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ret);
            }
        }
    }
}

#endif
