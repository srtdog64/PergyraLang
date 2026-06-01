/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_domain_role_emit.h"

#include "llvm_domain_role_helpers.h"
#include "llvm_inventory_decl_lookup.h"
#include "llvm_inventory_host_methods.h"

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
        if (llvm_hosted_method_view_missing_mir_metadata(&method_view)) {
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path missing method declaration metadata for role '%s'",
                role_name != NULL ? role_name : "(anonymous-role)");
            return false;
        }

        for (size_t j = 0; j < method_view.count; j++) {
            const MIRDeclMethod *method_meta =
                llvm_hosted_method_view_metadata(&method_view, j);
            ASTNode *method =
                llvm_hosted_method_view_source_ast(&method_view, j);
            const char *method_name = llvm_mir_decl_method_name(method_meta);
            const MIRRoutine *mir_method = NULL;
            char fname[256];
            LLVMFuncEntry *fentry;

            if (llvm_hosted_method_view_missing_mir_method_row(&method_view, j)) {
                llvm_set_mir_inventory_missing(ctx,
                    "MIR-only LLVM path has invalid method declaration metadata row for role '%s'",
                    role_name != NULL ? role_name : "(anonymous-role)");
                return false;
            }
            if (method_name == NULL && method != NULL
                && method->type == AST_FUNC_DECL)
                method_name = llvm_role_method_name_from_ast(method);
            if (role_name == NULL || method_name == NULL)
                continue;

            if (!llvm_role_method_symbol_name(fname, sizeof(fname),
                    role_name, method_name)) {
                llvm_set_error(ctx, "role method name is too long");
                return false;
            }
            fentry = llvm_lookup_function(ctx, fname);
            if (fentry == NULL) {
                llvm_set_mir_inventory_missing(ctx,
                    "MIR-only LLVM path missing registered function for role method '%s.%s'",
                    role_name != NULL ? role_name : "(anonymous-role)",
                    method_name != NULL ? method_name : "(anonymous)");
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

        for (size_t ii = 0; ii < ast_role_impl_count(stmt); ii++) {
            ASTNode *impl = ast_role_impl(stmt, ii);
            if (impl == NULL || impl->type != AST_IMPL_ABILITY)
                continue;

            const char *ab_name = ast_impl_ability_name(impl);

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
                    ASTNode *method =
                        llvm_find_role_operator_method(ctx, stmt, ops[oi], 0);
                    const char *method_name = llvm_role_method_name_from_ast(method);
                    if (suffix == NULL || method == NULL)
                        continue;
                    if (role_name == NULL || method->type != AST_FUNC_DECL
                        || method_name == NULL) {
                        continue;
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
                    if (op_entry == NULL || method_entry == NULL)
                        continue;
                    if (LLVMCountBasicBlocks(op_entry->fn) > 0)
                        continue;

                    LLVMValueRef saved_fn = ctx->current_function;
                    LLVMTypeRef saved_ret = ctx->current_ret_type;
                    LLVMTypeRef op_ret_type = op_entry->ret_type;
                    ctx->current_function = op_entry->fn;
                    ctx->current_ret_type = op_ret_type;

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
                    if (saved_fn != NULL) {
                        LLVMBasicBlockRef last =
                            LLVMGetLastBasicBlock(saved_fn);
                        if (last != NULL)
                            LLVMPositionBuilderAtEnd(ctx->builder, last);
                    }
                }
            }

            /* Create vtable global constant. */
            if (role_name == NULL || ab_name == NULL)
                continue;
            char vt_type_name[256];
            if (!llvm_role_vtable_type_name(vt_type_name,
                    sizeof(vt_type_name), ab_name)) {
                llvm_set_error(ctx, "role vtable type name is too long");
                return false;
            }
            LLVMClassTypeEntry *vt_cls = llvm_lookup_class(ctx,
                vt_type_name);
            if (vt_cls != NULL) {
                size_t mc = ast_impl_ability_method_count(impl);
                /*
                 * Vtable method value array is consumed by
                 * LLVMConstNamedStruct (copies) for the global initializer.
                 */
                LLVMValueRef *vals = pgy_arena_calloc(&ctx->scratch,
                    (mc > 0 ? mc : 1) * sizeof(LLVMValueRef));
                for (size_t j = 0; j < mc; j++) {
                    ASTNode *method = ast_impl_ability_method(impl, j);
                    const char *method_name = llvm_role_method_name_from_ast(method);
                    if (method == NULL || method->type != AST_FUNC_DECL) {
                        vals[j] = LLVMConstNull(ctx->type_i8ptr);
                        continue;
                    }
                    if (role_name == NULL || method_name == NULL) {
                        vals[j] = LLVMConstNull(ctx->type_i8ptr);
                        continue;
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
                        sizeof(global_name), role_name, ab_name)) {
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
