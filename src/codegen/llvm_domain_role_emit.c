/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_domain_role_emit.h"

#include "llvm_domain_role_helpers.h"
#include "llvm_inventory_decl_lookup.h"
#include "llvm_inventory_host_methods.h"
#include "llvm_inventory_internal.h"
#include "../compiler/mir_decl_headers.h"

static const char *
llvm_role_method_name_from_ast(ASTNode *method)
{
    if (method != NULL && method->type == AST_FUNC_DECL)
        return ast_declaration_name(method);
    return NULL;
}

bool
llvm_emit_domain_role_method_bodies(LLVMGenCtx *ctx,
    ASTNode **roles,
    size_t role_count)
{
    if (ctx == NULL || roles == NULL)
        return true;

    for (size_t i = 0; i < role_count; i++) {
        ASTNode *stmt = roles[i];
        if (stmt == NULL || stmt->type != AST_ROLE_DECL)
            continue;

        const char *role_name = llvm_decl_node_name(stmt);
        LLVMHostedMethodView method_view =
            llvm_hosted_method_view_from_decl(ctx, role_name, stmt);
        if (role_name == NULL) {
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path missing role declaration name metadata");
            return false;
        }
        if (llvm_hosted_method_view_missing_mir_metadata(&method_view)) {
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path missing method declaration metadata for role '%s'",
                role_name != NULL ? role_name : "(anonymous-role)");
            return false;
        }
        if (!llvm_require_hosted_method_view_rows(ctx, &method_view,
                "MIR-only LLVM path has invalid method declaration metadata row for role '%s'",
                role_name != NULL ? role_name : "(anonymous-role)")) {
            return false;
        }

        for (size_t j = 0; j < method_view.count; j++) {
            const MIRDeclMethod *method_meta =
                llvm_hosted_method_view_metadata(&method_view, j);
            const char *method_name = llvm_mir_decl_method_name(method_meta);
            const MIRRoutine *mir_method = NULL;
            char fname[256];

            if (method_meta == NULL && llvm_active_has_mir(ctx)) {
                llvm_set_mir_inventory_missing(ctx,
                    "MIR-only LLVM path missing method body metadata row for role '%s'",
                    role_name != NULL ? role_name : "(anonymous-role)");
                return false;
            }
            if (method_meta == NULL)
                continue;

            if (method_name == NULL) {
                llvm_set_mir_inventory_missing(ctx,
                    "MIR-only LLVM path missing method name metadata for role '%s'",
                    role_name);
                return false;
            }

            if (!llvm_role_method_symbol_name(fname, sizeof(fname),
                    role_name, method_name)) {
                llvm_set_error(ctx, "role method name is too long");
                return false;
            }

            mir_method = llvm_mir_decl_method_routine(ctx, method_meta);
            if (mir_method != NULL) {
                llvm_emit_func_from_mir(mir_method, ctx);
                if (ctx->has_error)
                    return false;
                continue;
            }
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path missing routine for role method '%s.%s'",
                role_name != NULL ? role_name : "(anonymous-role)",
                method_name != NULL ? method_name : "(anonymous)");
            return false;
        }

        size_t role_impl_index = 0;
        for (size_t ii = 0; ii < ast_role_impl_count(stmt); ii++) {
            ASTNode *impl = ast_role_impl(stmt, ii);
            if (impl == NULL || impl->type != AST_IMPL_ABILITY)
                continue;

            ASTNode *ability_ref = NULL;
            const char *ab_name = NULL;
            const char *ab_tag = NULL;

            if (llvm_active_has_mir(ctx)) {
                const MIRDeclRoleImpl *impl_meta =
                    mir_decl_header_role_impl(
                        method_view.decl_header, role_impl_index);
                const MIRAbilityRef *mir_ref =
                    mir_decl_role_impl_ability_ref(impl_meta);
                role_impl_index++;
                if (mir_ref == NULL || mir_ability_ref_base_name(mir_ref) == NULL) {
                    llvm_set_mir_inventory_missing(ctx,
                        "MIR-only LLVM path missing role vtable ability-ref metadata for role '%s'",
                        role_name != NULL ? role_name : "(anonymous-role)");
                    return false;
                }
                ab_name = mir_ability_ref_base_name(mir_ref);
                ab_tag = llvm_render_mir_ability_ref_vtable_tag(ctx, mir_ref);
            } else {
                ability_ref = ast_impl_ability_ref(impl);
                ab_name = ast_impl_ability_name(impl);
                ab_tag =
                    llvm_render_ast_ability_ref_vtable_tag(ctx, ability_ref);
            }

            {
                PgyTokenType ops[] = {
                    TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, TOKEN_SLASH,
                    TOKEN_PERCENT, TOKEN_EQUAL, TOKEN_NOT_EQUAL, TOKEN_LESS,
                    TOKEN_LESS_EQUAL, TOKEN_GREATER, TOKEN_GREATER_EQUAL
                };
                const char *for_type_name = llvm_role_for_type_name(stmt);

                for (size_t oi = 0; for_type_name != NULL
                       && oi < sizeof(ops) / sizeof(ops[0]); oi++) {
                    const char *suffix = llvm_operator_suffix(ops[oi]);
                    const MIRDeclMethod *method_meta =
                        llvm_find_role_operator_method_metadata(
                            ctx, stmt, ops[oi], 0);
                    ASTNode *method = NULL;
                    const char *method_name =
                        llvm_mir_decl_method_name(method_meta);
                    if (method_meta == NULL) {
                        method = llvm_find_role_operator_method(
                            ctx, stmt, ops[oi], 0);
                        if (method != NULL && llvm_active_has_mir(ctx)) {
                            llvm_set_mir_inventory_missing(ctx,
                                "MIR-only LLVM path missing role operator method metadata for role '%s'",
                                role_name);
                            return false;
                        }
                        method_name = llvm_role_method_name_from_ast(method);
                    }
                    if (suffix == NULL || method_meta == NULL)
                        continue;
                    if (method_name == NULL) {
                        llvm_set_mir_inventory_missing(ctx,
                            "MIR-only LLVM path missing role operator method name metadata for role '%s'",
                            role_name);
                        return false;
                    }

                    char opname[256];
                    char mname[256];
                    if (!llvm_role_operator_symbol_name(opname,
                            sizeof(opname), suffix, for_type_name)) {
                        llvm_set_error(ctx,
                            "role operator bridge name is too long");
                        return false;
                    }
                    if (!llvm_role_method_symbol_name(mname, sizeof(mname),
                            role_name, method_name)) {
                        llvm_set_error(ctx,
                            "role operator method name is too long");
                        return false;
                    }

                    LLVMFuncEntry *op_entry = llvm_lookup_function(ctx, opname);
                    LLVMFuncEntry *method_entry = llvm_lookup_function(ctx, mname);
                    if (method_entry == NULL) {
                        llvm_set_mir_inventory_missing(ctx,
                            "MIR-only LLVM path missing registered role operator method function '%s.%s'",
                            role_name,
                            method_name);
                        return false;
                    }
                    if (op_entry == NULL)
                        continue;
                    if (LLVMCountBasicBlocks(op_entry->fn) > 0)
                        continue;

                    LLVMValueRef saved_fn = ctx->current_function;
                    LLVMTypeRef saved_ret = ctx->current_ret_type;
                    LLVMTypeRef saved_function_ret =
                        ctx->current_function_ret_type;
                    const char *saved_return_type_name =
                        ctx->current_return_type_name;
                    ASTNode *saved_return_callable_type =
                        ctx->current_return_callable_type;
                    LLVMBasicBlockRef saved_bb =
                        LLVMGetInsertBlock(ctx->builder);
                    LLVMTypeRef op_ret_type = op_entry->ret_type;
                    ctx->current_function = op_entry->fn;
                    ctx->current_ret_type = op_ret_type;
                    ctx->current_function_ret_type = op_ret_type;
                    ctx->current_return_type_name = NULL;
                    ctx->current_return_callable_type = NULL;

                    LLVMBasicBlockRef bb = LLVMAppendBasicBlockInContext(
                        ctx->context, op_entry->fn, "entry");
                    LLVMPositionBuilderAtEnd(ctx->builder, bb);

                    LLVMTypeRef lhs_type =
                        LLVMTypeOf(LLVMGetParam(op_entry->fn, 0));
                    LLVMValueRef lhs_alloca = llvm_create_entry_alloca(
                        ctx, lhs_type, "lhs.addr");
                    LLVMBuildStore(ctx->builder,
                        LLVMGetParam(op_entry->fn, 0), lhs_alloca);

                    LLVMValueRef lhs_self = LLVMBuildBitCast(ctx->builder,
                        lhs_alloca, ctx->type_i8ptr, llvm_tmp_name(ctx));
                    LLVMValueRef rhs_arg = LLVMGetParam(op_entry->fn, 1);
                    LLVMValueRef args[] = { lhs_self, rhs_arg };

                    if (op_ret_type == ctx->type_void) {
                        LLVMBuildCall2(ctx->builder, method_entry->fn_type,
                            method_entry->fn, args, 2, "");
                        LLVMBuildRetVoid(ctx->builder);
                    } else {
                        LLVMValueRef result = LLVMBuildCall2(ctx->builder,
                            method_entry->fn_type, method_entry->fn,
                            args, 2, llvm_tmp_name(ctx));
                        LLVMBuildRet(ctx->builder, result);
                    }

                    ctx->current_function = saved_fn;
                    ctx->current_ret_type = saved_ret;
                    ctx->current_function_ret_type = saved_function_ret;
                    ctx->current_return_type_name = saved_return_type_name;
                    ctx->current_return_callable_type =
                        saved_return_callable_type;
                    if (saved_bb != NULL)
                        LLVMPositionBuilderAtEnd(ctx->builder, saved_bb);
                }
            }

            /* Create vtable global constant. */
            if (ab_name == NULL || ab_tag == NULL) {
                llvm_set_mir_inventory_missing(ctx,
                    "MIR-only LLVM path missing role vtable ability-ref metadata for role '%s'",
                    role_name);
                return false;
            }
            char vt_type_name[256];
            if (!llvm_role_vtable_type_name(vt_type_name,
                    sizeof(vt_type_name), ab_tag)) {
                llvm_set_error(ctx, "role vtable type name is too long");
                return false;
            }
            LLVMClassTypeEntry *vt_cls = llvm_lookup_class(ctx,
                vt_type_name);
            if (vt_cls == NULL && strcmp(ab_tag, ab_name) != 0) {
                char base_vt_type_name[256];
                if (!llvm_role_vtable_type_name(base_vt_type_name,
                        sizeof(base_vt_type_name), ab_name)) {
                    llvm_set_error(ctx, "role vtable type name is too long");
                    return false;
                }
                vt_cls = llvm_lookup_class(ctx, base_vt_type_name);
            }
            if (vt_cls != NULL) {
                size_t mc = ast_impl_ability_method_count(impl);
                /*
                 * Vtable method value array is consumed by
                 * LLVMConstNamedStruct (copies) for the global initializer.
                 */
                LLVMValueRef *vals = pgy_arena_calloc(&ctx->scratch,
                    (mc > 0 ? mc : 1) * sizeof(LLVMValueRef));
                if (vals == NULL) {
                    llvm_set_error(ctx,
                        "LLVM role vtable value allocation failed for '%s.%s'",
                        role_name != NULL ? role_name : "(anonymous-role)",
                        ab_tag != NULL ? ab_tag : "(anonymous-ability)");
                    return false;
                }
                for (size_t j = 0; j < mc; j++) {
                    ASTNode *method = ast_impl_ability_method(impl, j);
                    const MIRDeclMethod *method_meta = NULL;
                    const char *method_name;
                    if (llvm_active_has_mir(ctx)) {
                        const char *ast_method_name =
                            llvm_role_method_name_from_ast(method);
                        method_meta = llvm_find_host_method_metadata_in_context(
                            ctx, role_name, ast_method_name);
                        if (method_meta == NULL) {
                            llvm_set_mir_inventory_missing(ctx,
                                "MIR-only LLVM path missing role vtable method metadata for role '%s'",
                                role_name);
                            return false;
                        }
                    }
                    method_name = method_meta != NULL
                        ? llvm_mir_decl_method_name(method_meta)
                        : llvm_role_method_name_from_ast(method);
                    if (method_meta == NULL
                        && (method == NULL || method->type != AST_FUNC_DECL)) {
                        llvm_set_mir_inventory_missing(ctx,
                            "MIR-only LLVM path missing role vtable method source metadata for role '%s'",
                            role_name);
                        return false;
                    }
                    if (method_name == NULL) {
                        llvm_set_mir_inventory_missing(ctx,
                            "MIR-only LLVM path missing role vtable method name metadata for role '%s'",
                            role_name);
                        return false;
                    }
                    char fname[256];
                    if (!llvm_role_method_symbol_name(fname, sizeof(fname),
                            role_name, method_name)) {
                        llvm_set_error(ctx,
                            "role vtable method name is too long");
                        return false;
                    }
                    LLVMFuncEntry *fe = llvm_lookup_function(ctx, fname);
                    if (fe == NULL) {
                        llvm_set_mir_inventory_missing(ctx,
                            "MIR-only LLVM path missing vtable function for role method '%s.%s'",
                            role_name != NULL ? role_name : "(anonymous-role)",
                            method_name != NULL ? method_name : "(anonymous)");
                        return false;
                    }
                    vals[j] = fe->fn;
                }

                LLVMValueRef vt_const = LLVMConstNamedStruct(
                    vt_cls->struct_type, vals, (unsigned)mc);

                char global_name[256];
                if (!llvm_role_vtable_global_name(global_name,
                        sizeof(global_name), role_name, ab_tag)) {
                    llvm_set_error(ctx,
                        "role vtable global name is too long");
                    return false;
                }
                LLVMValueRef global = LLVMAddGlobal(ctx->module,
                    vt_cls->struct_type, global_name);
                LLVMSetInitializer(global, vt_const);
                LLVMSetGlobalConstant(global, 1);
                LLVMSetLinkage(global, LLVMInternalLinkage);
            }
        }
    }

    return true;
}

#endif /* PGY_LLVM_ENABLED */
