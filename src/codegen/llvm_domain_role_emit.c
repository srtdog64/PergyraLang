/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_domain_role_emit.h"

#include "llvm_domain_role_helpers.h"

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

        const char *role_name = stmt->data.role_decl.name;

        for (size_t ii = 0; ii < stmt->data.role_decl.impl_count; ii++) {
            ASTNode *impl = stmt->data.role_decl.impl_abilities[ii];
            if (impl == NULL || impl->type != AST_IMPL_ABILITY)
                continue;

            const char *ab_name =
                (impl->data.impl_ability.ability_ref != NULL
                 && impl->data.impl_ability.ability_ref->type == AST_TYPE)
                ? impl->data.impl_ability.ability_ref->data.type.name : NULL;

            /* Emit method bodies. */
            for (size_t j = 0; j < impl->data.impl_ability.method_count;
                 j++) {
                ASTNode *method = impl->data.impl_ability.methods[j];
                if (method == NULL || method->type != AST_FUNC_DECL)
                    continue;
                if (role_name == NULL || method->data.func_decl.name == NULL)
                    continue;

                char fname[256];
                snprintf(fname, sizeof(fname), "%s_%s",
                         role_name, method->data.func_decl.name);

                LLVMFuncEntry *fentry = llvm_lookup_function(ctx, fname);
                const MIRRoutine *mir_method =
                    llvm_find_mir_method_routine_local(ctx, role_name, method);
                if (fentry == NULL)
                    continue;
                if (mir_method != NULL) {
                    llvm_emit_func_from_mir(mir_method, ctx);
                    continue;
                }
                if (ctx->mir != NULL) {
                    llvm_set_error_with_hints(ctx,
                        PGY_CODE_LLVM_MIR_ROUTINE_MISSING,
                        PGY_CAUSE_LLVM_MIR_ROUTINE_MISSING,
                        PGY_FIX_INSPECT_MIR_INVENTORY,
                        "MIR-only LLVM path missing routine for "
                        "domain method '%s.%s'",
                        role_name, method->data.func_decl.name);
                    return false;
                }

                LLVMValueRef fn = fentry->fn;
                LLVMTypeRef ret_type = fentry->ret_type;
                LLVMValueRef saved_fn = ctx->current_function;
                LLVMTypeRef saved_ret = ctx->current_ret_type;
                ctx->current_function = fn;
                ctx->current_ret_type = ret_type;

                LLVMBasicBlockRef bb = LLVMAppendBasicBlockInContext(
                    ctx->context, fn, "entry");
                LLVMPositionBuilderAtEnd(ctx->builder, bb);
                llvm_scope_push(ctx);

                /* self param */
                LLVMValueRef self_val = LLVMGetParam(fn, 0);
                LLVMValueRef self_alloca = llvm_create_entry_alloca(
                    ctx, ctx->type_i8ptr, "self.addr");
                LLVMBuildStore(ctx->builder, self_val, self_alloca);
                llvm_scope_declare(ctx, "self", self_alloca,
                                    ctx->type_i8ptr);

                /* User params */
                size_t pc = method->data.func_decl.param_count;
                unsigned lpidx = 1;
                for (size_t k = 0; k < pc; k++) {
                    FuncParam *p = method->data.func_decl.params[k];
                    if (llvm_param_is_implicit_self_local(p))
                        continue;
                    if (p == NULL || p->name == NULL) {
                        lpidx++;
                        continue;
                    }
                    LLVMTypeRef pt = (p->type != NULL)
                        ? ast_type_to_llvm(ctx, p->type)
                        : ctx->type_i32;
                    LLVMValueRef a = llvm_create_entry_alloca(
                        ctx, pt, p->name);
                    LLVMBuildStore(ctx->builder,
                        LLVMGetParam(fn, lpidx++), a);
                    llvm_scope_declare(ctx, p->name, a, pt);
                }

                {
                    char msg[384];
                    snprintf(msg, sizeof(msg),
                             "MIR-only LLVM path missing routine for role method '%s.%s'",
                             role_name != NULL ? role_name : "(anonymous-role)",
                             method->data.func_decl.name != NULL
                                 ? method->data.func_decl.name
                                 : "(anonymous)");
                    llvm_set_error_with_hints(ctx,
                        PGY_CODE_LLVM_MIR_ROUTINE_MISSING,
                        PGY_CAUSE_LLVM_MIR_ROUTINE_MISSING,
                        PGY_FIX_INSPECT_MIR_INVENTORY,
                        "%s", msg);
                    llvm_scope_pop(ctx);
                    return false;
                }

                if (LLVMGetBasicBlockTerminator(
                        LLVMGetInsertBlock(ctx->builder)) == NULL) {
                    if (ret_type == ctx->type_void)
                        LLVMBuildRetVoid(ctx->builder);
                    else
                        LLVMBuildRet(ctx->builder,
                            LLVMConstInt(ret_type, 0, 0));
                }

                llvm_scope_pop(ctx);
                ctx->current_function = saved_fn;
                ctx->current_ret_type = saved_ret;

                if (saved_fn != NULL) {
                    LLVMBasicBlockRef last =
                        LLVMGetLastBasicBlock(saved_fn);
                    if (last != NULL)
                        LLVMPositionBuilderAtEnd(ctx->builder, last);
                }
            }

            {
                PgyTokenType ops[] = {
                    TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, TOKEN_SLASH,
                    TOKEN_PERCENT, TOKEN_EQUAL, TOKEN_NOT_EQUAL, TOKEN_LESS,
                    TOKEN_LESS_EQUAL, TOKEN_GREATER, TOKEN_GREATER_EQUAL
                };
                const char *for_type_name = NULL;
                if (stmt->data.role_decl.for_type != NULL
                    && stmt->data.role_decl.for_type->type == AST_TYPE) {
                    for_type_name =
                        stmt->data.role_decl.for_type->data.type.name;
                }

                for (size_t oi = 0; for_type_name != NULL
                       && oi < sizeof(ops) / sizeof(ops[0]); oi++) {
                    const char *suffix = llvm_operator_suffix(ops[oi]);
                    ASTNode *method =
                        llvm_find_role_operator_method(ctx, stmt, ops[oi], 0);
                    if (suffix == NULL || method == NULL)
                        continue;
                    if (role_name == NULL || method->type != AST_FUNC_DECL
                        || method->data.func_decl.name == NULL) {
                        continue;
                    }

                    char opname[256];
                    char mname[256];
                    snprintf(opname, sizeof(opname), "operator_%s_%s",
                             suffix, for_type_name);
                    snprintf(mname, sizeof(mname), "%s_%s",
                             role_name, method->data.func_decl.name);

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
            snprintf(vt_type_name, sizeof(vt_type_name),
                     "%s_vtable", ab_name);
            LLVMClassTypeEntry *vt_cls = llvm_lookup_class(ctx,
                vt_type_name);
            if (vt_cls != NULL) {
                size_t mc = impl->data.impl_ability.method_count;
                /*
                 * Vtable method value array is consumed by
                 * LLVMConstNamedStruct (copies) for the global initializer.
                 */
                LLVMValueRef *vals = pgy_arena_calloc(&ctx->scratch,
                    (mc > 0 ? mc : 1) * sizeof(LLVMValueRef));
                for (size_t j = 0; j < mc; j++) {
                    ASTNode *method = impl->data.impl_ability.methods[j];
                    if (method == NULL || method->type != AST_FUNC_DECL) {
                        vals[j] = LLVMConstNull(ctx->type_i8ptr);
                        continue;
                    }
                    if (role_name == NULL || method->data.func_decl.name == NULL) {
                        vals[j] = LLVMConstNull(ctx->type_i8ptr);
                        continue;
                    }
                    char fname[256];
                    snprintf(fname, sizeof(fname), "%s_%s",
                             role_name, method->data.func_decl.name);
                    LLVMFuncEntry *fe = llvm_lookup_function(ctx, fname);
                    vals[j] = (fe != NULL) ? fe->fn
                        : LLVMConstNull(ctx->type_i8ptr);
                }

                LLVMValueRef vt_const = LLVMConstNamedStruct(
                    vt_cls->struct_type, vals, (unsigned)mc);

                char global_name[256];
                snprintf(global_name, sizeof(global_name),
                         "%s_%s_vtable_instance", role_name, ab_name);
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
