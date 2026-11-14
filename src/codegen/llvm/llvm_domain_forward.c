/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM domain forward declarations and vtable registration.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_domain_forward_internal.h"
#include "llvm_backend_type_map_internal.h"
#include "llvm_inventory_host_methods.h"
#include "llvm_inventory_internal.h"

bool
llvm_domain_forward_suffix_name(char *out,
                                size_t out_size,
                                const char *name,
                                const char *suffix)
{
    int written;

    if (out == NULL || out_size == 0 || name == NULL || suffix == NULL)
        return false;
    written = snprintf(out, out_size, "%s_%s", name, suffix);
    return written >= 0 && (size_t)written < out_size;
}

bool
llvm_domain_forward_join_name(char *out,
                              size_t out_size,
                              const char *left,
                              const char *right)
{
    return llvm_domain_forward_suffix_name(out, out_size, left, right);
}

bool
llvm_domain_forward_operator_name(char *out,
                                  size_t out_size,
                                  const char *suffix,
                                  const char *type_name)
{
    int written;

    if (out == NULL || out_size == 0 || suffix == NULL || type_name == NULL)
        return false;
    written = snprintf(out, out_size, "operator_%s_%s", suffix, type_name);
    return written >= 0 && (size_t)written < out_size;
}

const char *
llvm_domain_method_name_metadata_first(const MIRDeclMethod *method_meta,
                                       ASTNode *method,
                                       bool allow_ast_compat)
{
    const char *name = llvm_mir_decl_method_name(method_meta);
    if (name != NULL)
        return name;
    if (allow_ast_compat && method != NULL && method->type == AST_FUNC_DECL)
        return ast_declaration_name(method);
    return NULL;
}

size_t
llvm_domain_method_param_count_metadata_first(const MIRDeclMethod *method_meta,
                                              ASTNode *method,
                                              bool allow_ast_compat)
{
    if (method_meta != NULL)
        return llvm_mir_decl_method_param_count(method_meta);
    if (allow_ast_compat && method != NULL && method->type == AST_FUNC_DECL)
        return ast_func_param_count(method);
    return 0;
}

FuncParam *
llvm_domain_method_param_metadata_first(const MIRDeclMethod *method_meta,
                                        ASTNode *method,
                                        size_t index,
                                        bool allow_ast_compat)
{
    FuncParam *param = llvm_mir_decl_method_param(method_meta, index);
    if (param != NULL)
        return param;
    if (allow_ast_compat && method != NULL && method->type == AST_FUNC_DECL)
        return ast_func_param(method, index);
    return NULL;
}

const char *
llvm_domain_method_param_type_name_metadata_first(
    const MIRDeclMethod *method_meta,
    ASTNode *method,
    size_t index,
    bool allow_ast_compat)
{
    const char *type_name =
        llvm_mir_decl_method_param_type_name(method_meta, index);
    if (type_name != NULL)
        return type_name;
    if (allow_ast_compat && method != NULL && method->type == AST_FUNC_DECL) {
        FuncParam *param = ast_func_param(method, index);
        if (param != NULL && param->type != NULL)
            return ast_type_name(param->type);
    }
    return NULL;
}

ASTNode *
llvm_domain_method_return_type_metadata_first(const MIRDeclMethod *method_meta,
                                              ASTNode *method,
                                              bool allow_ast_compat)
{
    ASTNode *return_type = llvm_mir_decl_method_return_type(method_meta);
    if (return_type != NULL)
        return return_type;
    if (allow_ast_compat && method != NULL && method->type == AST_FUNC_DECL)
        return ast_func_return_type(method);
    return NULL;
}

const char *
llvm_domain_method_return_type_name_metadata_first(
    const MIRDeclMethod *method_meta,
    ASTNode *method,
    bool allow_ast_compat)
{
    const char *type_name =
        llvm_mir_decl_method_return_type_name(method_meta);
    if (type_name != NULL)
        return type_name;
    if (allow_ast_compat && method != NULL && method->type == AST_FUNC_DECL) {
        ASTNode *return_type = ast_func_return_type(method);
        if (return_type != NULL)
            return ast_type_name(return_type);
    }
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
    if (!llvm_domain_forward_suffix_name(sync_name, sizeof(sync_name),
            decl_name, "sync")) {
        llvm_set_error(ctx,
            "LLVM domain sync routine name is too long for '%s'", decl_name);
        return;
    }
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
                                      const LLVMHostedMethodView *methods)
{
    if (ctx == NULL || decl_name == NULL || struct_ty == NULL)
        return;

    if (methods == NULL)
        return;
    if (llvm_hosted_method_view_missing_mir_metadata(methods)) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing method forward metadata for domain '%s'",
            decl_name != NULL ? decl_name : "(anonymous-domain)");
        return;
    }
    if (!llvm_require_hosted_method_view_rows(ctx, methods,
            "MIR-only LLVM path has invalid method forward metadata row for domain '%s'",
            decl_name != NULL ? decl_name : "(anonymous-domain)")) {
        return;
    }

    for (size_t j = 0; j < methods->count; j++) {
        const MIRDeclMethod *method_meta =
            llvm_hosted_method_view_metadata(methods, j);
        const char *mname;
        ASTNode *return_type = NULL;
        const char *return_type_name;
        const MIRRoutine *method_routine;
        const MIRCallableSig *return_callable_sig;
        size_t pc;
        LLVMTypeRef ret;
        size_t user_pc = 0;
        LLVMTypeRef *ptypes;
        size_t pidx = 1;
        LLVMTypeRef ft;
        char fname[256];
        LLVMValueRef fn;

        if (method_meta == NULL && llvm_active_has_mir(ctx)) {
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path missing method forward metadata row for domain '%s'",
                decl_name != NULL ? decl_name : "(anonymous-domain)");
            return;
        }
        if (method_meta == NULL)
            continue;

        method_routine = llvm_mir_decl_method_routine(ctx, method_meta);
        return_callable_sig = method_routine != NULL
            ? llvm_mir_routine_return_callable_sig(method_routine)
            : NULL;

        mname = llvm_domain_method_name_metadata_first(
            method_meta, NULL, false);
        if (mname == NULL) {
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path missing method forward name metadata for domain '%s'",
                decl_name != NULL ? decl_name : "(anonymous-domain)");
            return;
        }
        pc = llvm_domain_method_param_count_metadata_first(
            method_meta, NULL, false);
        ret = ctx->type_void;
        return_type_name =
            llvm_domain_method_return_type_name_metadata_first(
                method_meta, NULL, false);
        return_type =
            llvm_domain_method_return_type_metadata_first(
                method_meta, NULL, false);
        if (!llvm_mir_decl_method_metadata_complete_for(ctx,
                method_meta,
                decl_name,
                mname,
                LLVM_MIR_DECL_METHOD_REQUIRE_ALL_TYPE_NAMES,
                "MIR-only LLVM path missing domain method forward return type-name metadata for '%s.%s'",
                "MIR-only LLVM path missing domain method forward parameter type-name metadata for '%s.%s'")) {
            return;
        }
        if (return_callable_sig != NULL) {
            ret = llvm_mir_callable_sig_to_llvm(ctx, return_callable_sig);
            if (ctx->has_error || ret == NULL)
                return;
        } else if (return_type_name != NULL) {
            ret = pergyra_type_to_llvm(ctx, return_type_name);
            if (ctx->has_error || ret == NULL)
                return;
        } else if (return_type != NULL) {
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path missing domain method return ABI fact for '%s.%s'",
                decl_name, mname);
            return;
        } else {
            ret = ctx->type_void;
            if (ctx->has_error || ret == NULL)
                return;
        }

        for (size_t k = 0; k < pc; k++) {
            FuncParam *p =
                llvm_domain_method_param_metadata_first(
                    method_meta, NULL, k, false);
            if (!llvm_param_is_implicit_self_local(p))
                user_pc++;
        }

        ptypes = pgy_arena_calloc(&ctx->scratch,
            (user_pc + 1) * sizeof(LLVMTypeRef));
        if (ptypes == NULL) {
            llvm_set_error(ctx,
                "LLVM domain method parameter allocation failed for '%s.%s'",
                decl_name,
                mname != NULL ? mname : "<anonymous>");
            return;
        }
        ptypes[0] = LLVMPointerType(struct_ty, 0);
        for (size_t k = 0; k < pc; k++) {
            FuncParam *p =
                llvm_domain_method_param_metadata_first(
                    method_meta, NULL, k, false);
            const char *type_name =
                llvm_domain_method_param_type_name_metadata_first(
                    method_meta, NULL, k, false);
            const MIRCallableSig *param_callable_sig =
                method_routine != NULL
                    ? llvm_mir_routine_param_callable_sig(method_routine, k)
                    : NULL;
            LLVMClassTypeEntry *param_cls = NULL;
            if (llvm_param_is_implicit_self_local(p))
                continue;
            param_cls = type_name != NULL ? llvm_lookup_class(ctx, type_name) : NULL;
            if (param_callable_sig != NULL) {
                LLVMTypeRef pt = llvm_mir_callable_sig_to_llvm(
                    ctx, param_callable_sig);
                if (ctx->has_error || pt == NULL)
                    return;
                ptypes[pidx++] = pt;
            } else if (param_cls != NULL && param_cls->is_pointer_self_host) {
                ptypes[pidx++] = LLVMPointerType(param_cls->struct_type, 0);
            } else if (type_name != NULL) {
                LLVMTypeRef pt = pergyra_type_to_llvm(ctx, type_name);
                if (ctx->has_error || pt == NULL)
                    return;
                ptypes[pidx++] = pt;
            } else {
                llvm_set_mir_inventory_missing(ctx,
                    "MIR-only LLVM path missing domain method parameter ABI fact for '%s.%s'",
                    decl_name, mname);
                return;
            }
        }

        ft = LLVMFunctionType(ret, ptypes, (unsigned)(user_pc + 1), 0);
        if (!llvm_domain_forward_join_name(fname, sizeof(fname), decl_name,
                mname)) {
            llvm_set_error(ctx,
                "LLVM domain method routine name is too long for '%s.%s'",
                decl_name, mname);
            return;
        }
        fn = LLVMAddFunction(ctx->module, fname, ft);
        llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ret);
    }
}

#endif
