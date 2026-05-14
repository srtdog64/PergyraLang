/*
 * LLVM spawn/await expression lowering.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_spawn_call_helpers.h"
#include "llvm_expr_spawn_names.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "llvm_boundary_slot_param.h"
#include "llvm_expr_boundary_projection_helpers.h"
#include "llvm_internal_api.h"
#include "llvm_inventory_decl_lookup.h"
#include "../common/string_compat.h"

static LLVMTypeRef
llvm_spawn_required_param_type(LLVMGenCtx *ctx,
                               ASTNode *owner,
                               FuncParam *param,
                               const char *callee_name)
{
    if (ctx == NULL)
        return NULL;
    if (param != NULL && param->type != NULL)
        return ast_type_to_llvm(ctx, param->type);

    llvm_set_error_at_with_hints(ctx, owner,
        PGY_CODE_LLVM_TYPE_UNSUPPORTED,
        PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
        PGY_FIX_ANNOTATE_CONCRETE_TYPE,
        "LLVM generic call '%s' parameter requires explicit type metadata; silent i32 fallback is not allowed",
        callee_name != NULL ? callee_name : "<anonymous>");
    return NULL;
}

static const char *
llvm_generic_call_required_suffix(LLVMGenCtx *ctx,
                                  ASTNode *owner,
                                  const char *callee_name,
                                  LLVMValueRef value,
                                  size_t arg_index)
{
    const char *suffix;

    if (ctx == NULL || value == NULL) {
        llvm_set_error_at_with_hints(ctx, owner,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "LLVM generic call '%s' requires a lowered argument %zu for specialization",
            callee_name != NULL ? callee_name : "<anonymous>",
            arg_index + 1);
        return NULL;
    }

    suffix = llvm_type_to_suffix(ctx, LLVMTypeOf(value));
    if (suffix != NULL && strcmp(suffix, "Unknown") != 0)
        return suffix;

    llvm_set_error_at_with_hints(ctx, owner,
        PGY_CODE_LLVM_TYPE_UNSUPPORTED,
        PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
        PGY_FIX_ANNOTATE_CONCRETE_TYPE,
        "LLVM generic call '%s' requires concrete argument %zu type metadata for specialization",
        callee_name != NULL ? callee_name : "<anonymous>",
        arg_index + 1);
    return NULL;
}

LLVMFuncEntry *
llvm_resolve_callee_entry(LLVMGenCtx *ctx, const char *callee_name,
                          LLVMValueRef *args, size_t argc)
{
    ASTNode *generic_ast = llvm_lookup_generic_template(ctx, callee_name);
    char mangled[256];

    if (generic_ast == NULL)
        return llvm_lookup_function(ctx, callee_name);

    if (!llvm_spawn_copy_name(ctx, generic_ast, mangled, sizeof(mangled),
            callee_name, "generic callee"))
        return NULL;
    for (size_t i = 0; i < argc; i++) {
        const char *suf = llvm_generic_call_required_suffix(ctx, generic_ast,
            callee_name, args != NULL ? args[i] : NULL, i);
        if (suf == NULL)
            return NULL;
        llvm_append_mangled_suffix(mangled, sizeof(mangled), suf);
    }

    if (!llvm_mono_already_emitted(ctx, mangled)) {
        GenericParams *gp;
        int saved_subst;
        LLVMBasicBlockRef saved_bb;
        LLVMValueRef saved_fn;
        LLVMTypeRef saved_ret;
        LLVMTypeRef ret = ctx->type_void;
        size_t pc;
        LLVMTypeRef *ptypes;
        size_t real_pc = 0;
        LLVMTypeRef ft;
        LLVMValueRef mono_fn;
        LLVMBasicBlockRef entry;

        llvm_register_mono(ctx, mangled);

        gp = generic_ast->data.func_decl.generic_params;
        saved_subst = ctx->type_subst_count;
        ctx->type_subst_count = 0;
        for (size_t gi = 0; gi < gp->count && gi < 8; gi++) {
            const char *suffix;
            LLVMTypeRef concrete;
            if (gi >= argc || args == NULL || args[gi] == NULL) {
                llvm_set_error_at_with_hints(ctx, generic_ast,
                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                    PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                    "LLVM generic call '%s' requires argument %zu to bind generic parameter '%s'",
                    callee_name != NULL ? callee_name : "<anonymous>",
                    gi + 1,
                    gp->params[gi] != NULL && gp->params[gi]->name != NULL
                        ? gp->params[gi]->name : "<anonymous>");
                ctx->type_subst_count = saved_subst;
                return NULL;
            }
            concrete = LLVMTypeOf(args[gi]);
            suffix = llvm_generic_call_required_suffix(ctx, generic_ast,
                callee_name, args[gi], gi);
            if (suffix == NULL) {
                ctx->type_subst_count = saved_subst;
                return NULL;
            }
            ctx->type_subst[ctx->type_subst_count].param_name =
                gp->params[gi]->name;
            ctx->type_subst[ctx->type_subst_count].llvm_type = concrete;
            ctx->type_subst[ctx->type_subst_count].type_name = suffix;
            ctx->type_subst_count++;
        }

        saved_bb = LLVMGetInsertBlock(ctx->builder);
        saved_fn = ctx->current_function;
        saved_ret = ctx->current_ret_type;

        if (generic_ast->data.func_decl.return_type != NULL)
            ret = ast_type_to_llvm(ctx, generic_ast->data.func_decl.return_type);

        pc = generic_ast->data.func_decl.param_count;
        ptypes = pgy_arena_calloc(&ctx->scratch,
            ((pc * 2) > 0 ? (pc * 2) : 1) * sizeof(LLVMTypeRef));
        if (ptypes == NULL) {
            llvm_set_error_at_with_hints(ctx, generic_ast,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_INSPECT_MIR_INVENTORY,
                "LLVM generic spawn specialization parameter type allocation failed for '%s'",
                callee_name != NULL ? callee_name : "<anonymous>");
            ctx->type_subst_count = saved_subst;
            return NULL;
        }
        real_pc = 0;
        for (size_t k = 0; k < pc; k++) {
            FuncParam *p = generic_ast->data.func_decl.params[k];
            if (llvm_param_is_implicit_self(p))
                continue;
            if (p == NULL || p->name == NULL)
                continue;
            {
                bool is_secure = false;
                const char *inner = llvm_boundary_slot_inner_name(ctx, p,
                    &is_secure);
                if (inner != NULL) {
                    LLVMTypeRef slot_ty = ast_type_to_llvm(ctx, p->type);
                    ptypes[real_pc++] = LLVMPointerType(slot_ty, 0);
                    if (is_secure)
                        ptypes[real_pc++] = llvm_secure_token_type(ctx, inner);
                } else {
                    LLVMTypeRef pt = llvm_spawn_required_param_type(
                        ctx, generic_ast, p, callee_name);
                    if (pt == NULL) {
                        ctx->type_subst_count = saved_subst;
                        return NULL;
                    }
                    ptypes[real_pc++] = pt;
                }
            }
        }
        ft = LLVMFunctionType(ret, ptypes, (unsigned)real_pc, 0);
        mono_fn = LLVMAddFunction(ctx->module, mangled, ft);
        llvm_register_function(ctx, mangled, mono_fn, ft, ret);
        ctx->current_function = mono_fn;
        ctx->current_ret_type = ret;
        entry = LLVMAppendBasicBlockInContext(ctx->context, mono_fn, "entry");
        LLVMPositionBuilderAtEnd(ctx->builder, entry);
        llvm_scope_push(ctx);

        real_pc = 0;
        for (size_t k = 0; k < pc; k++) {
            FuncParam *p = generic_ast->data.func_decl.params[k];
            if (llvm_param_is_implicit_self(p))
                continue;
            if (p == NULL || p->name == NULL)
                continue;
            {
                bool is_secure = false;
                const char *inner = llvm_boundary_slot_inner_name(ctx, p,
                    &is_secure);
                LLVMTypeRef pt = llvm_spawn_required_param_type(
                    ctx, generic_ast, p, callee_name);
                if (pt == NULL) {
                    llvm_scope_pop(ctx);
                    ctx->type_subst_count = saved_subst;
                    ctx->current_function = saved_fn;
                    ctx->current_ret_type = saved_ret;
                    if (saved_bb != NULL)
                        LLVMPositionBuilderAtEnd(ctx->builder, saved_bb);
                    return NULL;
                }
                if (inner != NULL) {
                    LLVMValueRef slot_ptr = LLVMGetParam(mono_fn,
                        (unsigned)real_pc++);
                    llvm_scope_declare(ctx, p->name, slot_ptr, pt);
                    llvm_register_slot_var(ctx, p->name, inner, is_secure);
                    if (is_secure) {
                        LLVMTypeRef token_ty = llvm_secure_token_type(ctx,
                            inner);
                        char token_name[256];
                        LLVMValueRef token_alloca;
                        if (!llvm_spawn_format_name(ctx, generic_ast,
                                token_name, sizeof(token_name), p->name,
                                "_token", "secure token")) {
                            llvm_scope_pop(ctx);
                            ctx->type_subst_count = saved_subst;
                            ctx->current_function = saved_fn;
                            ctx->current_ret_type = saved_ret;
                            if (saved_bb != NULL)
                                LLVMPositionBuilderAtEnd(ctx->builder, saved_bb);
                            return NULL;
                        }
                        token_alloca = llvm_create_entry_alloca(ctx, token_ty,
                            token_name);
                        LLVMBuildStore(ctx->builder,
                            LLVMGetParam(mono_fn, (unsigned)real_pc++),
                            token_alloca);
                        llvm_scope_declare(ctx, pergyra_strdup(token_name),
                            token_alloca, token_ty);
                    }
                } else {
                    LLVMValueRef alloca = llvm_create_entry_alloca(ctx, pt,
                        p->name);
                    LLVMBuildStore(ctx->builder,
                        LLVMGetParam(mono_fn, (unsigned)real_pc), alloca);
                    llvm_scope_declare(ctx, p->name, alloca, pt);
                    if (p->type != NULL && p->type->type == AST_TYPE
                        && p->type->data.type.name != NULL
                        && llvm_lookup_class(ctx,
                            p->type->data.type.name) != NULL) {
                        llvm_register_var_class(ctx, p->name,
                            p->type->data.type.name);
                    }
                    real_pc++;
                }
            }
        }

        if (generic_ast->data.func_decl.body != NULL)
            llvm_emit_block(generic_ast->data.func_decl.body, ctx);

        if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL) {
            if (ret == ctx->type_void)
                LLVMBuildRetVoid(ctx->builder);
            else
                LLVMBuildRet(ctx->builder, LLVMConstInt(ret, 0, 0));
        }

        llvm_scope_pop(ctx);
        ctx->type_subst_count = saved_subst;
        ctx->current_function = saved_fn;
        ctx->current_ret_type = saved_ret;
        if (saved_bb != NULL)
            LLVMPositionBuilderAtEnd(ctx->builder, saved_bb);
    }

    return llvm_lookup_function(ctx, mangled);
}

LLVMValueRef
llvm_emit_spawn_expr(ASTNode *node, LLVMGenCtx *ctx)
{
    ASTNode *target = node->data.spawn_expr.function;
    ASTNode *call = NULL;
    ASTNode *callee = NULL;
    const char *callee_name = NULL;
    size_t argc = 0;
    LLVMValueRef *args = NULL;
    LLVMFuncEntry *callee_entry = NULL;
    LLVMFuncEntry *spawn_fn = NULL;
    LLVMFuncEntry *malloc_fn = NULL;
    LLVMFuncEntry *free_fn = NULL;
    int wrapper_id = ++ctx->tmp_counter;

    if (target == NULL) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_SYMBOL_UNDEFINED,
            PGY_FIX_IMPORT_OR_DECLARE_SYMBOL,
            "LLVM spawn expression requires a target expression");
        return NULL;
    }

    if (target->type == AST_CALL) {
        call = target;
        callee = target->data.call.callee;
        argc = target->data.call.arg_count;
    } else {
        callee = target;
    }

    if (callee != NULL && callee->type == AST_IDENTIFIER)
        callee_name = callee->data.identifier.name;
    if (callee_name == NULL) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_SYMBOL_UNDEFINED,
            PGY_FIX_IMPORT_OR_DECLARE_SYMBOL,
            "LLVM spawn expression requires an identifier target");
        return NULL;
    }

    if (argc > 0) {
        if (argc > (size_t)UINT_MAX || argc > SIZE_MAX / sizeof(LLVMValueRef)) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ALIGN_GENERIC_ARG_LIST,
                "LLVM spawn expression argument count exceeds backend ABI limits");
            return NULL;
        }
        args = pgy_arena_calloc(&ctx->scratch, argc * sizeof(LLVMValueRef));
        if (args == NULL) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_INSPECT_MIR_INVENTORY,
                "LLVM spawn expression argument allocation failed");
            return NULL;
        }
        for (size_t i = 0; i < argc; i++) {
            args[i] = llvm_emit_expression(call->data.call.arguments[i], ctx);
            if (args[i] == NULL) {
                llvm_set_error_at_with_hints(ctx, node,
                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                    PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                    "LLVM spawn expression target '%s' could not lower argument %zu",
                    callee_name,
                    i + 1);
                return NULL;
            }
        }
    }

    callee_entry = llvm_resolve_callee_entry(ctx, callee_name, args, argc);
    spawn_fn = llvm_lookup_function(ctx,
        node->data.spawn_expr.is_blocking
            ? "pgy_spawn_blocking_export"
            : "pgy_async_spawn_export");
    malloc_fn = llvm_lookup_function(ctx, "malloc");
    free_fn = llvm_lookup_function(ctx, "free");
    if (spawn_fn == NULL || malloc_fn == NULL || free_fn == NULL) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM spawn expression requires registered runtime functions '%s', 'malloc', and 'free'",
            node->data.spawn_expr.is_blocking
                ? "pgy_spawn_blocking_export"
                : "pgy_async_spawn_export");
        return NULL;
    }
    if (callee_entry == NULL) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_SYMBOL_UNDEFINED,
            PGY_FIX_IMPORT_OR_DECLARE_SYMBOL,
            "LLVM spawn expression target '%s' is not declared in the backend function registry",
            callee_name);
        return NULL;
    }

    {
        LLVMValueRef saved_fn = ctx->current_function;
        LLVMTypeRef saved_ret = ctx->current_ret_type;
        LLVMBasicBlockRef saved_bb = LLVMGetInsertBlock(ctx->builder);
        char wrapper_name[96];
        LLVMTypeRef wrapper_params[] = { ctx->type_i8ptr };
        LLVMTypeRef wrapper_type;
        LLVMValueRef wrapper_fn;
        LLVMTypeRef arg_struct_type = NULL;
        LLVMValueRef loaded_args_storage[16];
        LLVMValueRef *loaded_args = loaded_args_storage;
        LLVMValueRef raw_arg;
        LLVMValueRef call_result = NULL;
        LLVMValueRef raw_spawn_arg = LLVMConstNull(ctx->type_i8ptr);
        LLVMValueRef fn_ptr;
        LLVMValueRef spawn_args[2];
        LLVMValueRef handle;
        LLVMBasicBlockRef entry;

        if (!llvm_spawn_wrapper_name(ctx, node, wrapper_name,
                sizeof(wrapper_name), wrapper_id))
            return NULL;
        wrapper_type = LLVMFunctionType(ctx->type_i8ptr, wrapper_params, 1, 0);
        wrapper_fn = LLVMAddFunction(ctx->module, wrapper_name, wrapper_type);

        if (argc > 0) {
            LLVMTypeRef *field_types = pgy_arena_calloc(&ctx->scratch,
                argc * sizeof(LLVMTypeRef));
            if (field_types == NULL) {
                llvm_set_error_at_with_hints(ctx, node,
                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                    PGY_FIX_INSPECT_MIR_INVENTORY,
                    "LLVM spawn expression argument type allocation failed");
                return NULL;
            }
            for (size_t i = 0; i < argc; i++)
                field_types[i] = LLVMTypeOf(args[i]);
            arg_struct_type = LLVMStructTypeInContext(ctx->context, field_types,
                (unsigned)argc, 0);
        }

        if (argc > 16) {
            loaded_args = pgy_arena_calloc(&ctx->scratch,
                argc * sizeof(LLVMValueRef));
            if (loaded_args == NULL) {
                llvm_set_error_at_with_hints(ctx, node,
                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                    PGY_FIX_INSPECT_MIR_INVENTORY,
                    "LLVM spawn expression loaded-argument allocation failed");
                return NULL;
            }
        }

        ctx->current_function = wrapper_fn;
        ctx->current_ret_type = ctx->type_i8ptr;
        entry = LLVMAppendBasicBlockInContext(ctx->context, wrapper_fn, "entry");
        LLVMPositionBuilderAtEnd(ctx->builder, entry);
        llvm_scope_push(ctx);

        raw_arg = LLVMGetParam(wrapper_fn, 0);
        if (argc > 0) {
            LLVMValueRef typed_ctx = LLVMBuildBitCast(ctx->builder, raw_arg,
                LLVMPointerType(arg_struct_type, 0), llvm_tmp_name(ctx));
            for (size_t i = 0; i < argc; i++) {
                LLVMValueRef gep = LLVMBuildStructGEP2(ctx->builder,
                    arg_struct_type, typed_ctx, (unsigned)i, llvm_tmp_name(ctx));
                loaded_args[i] = LLVMBuildLoad2(ctx->builder, LLVMTypeOf(args[i]),
                    gep, llvm_tmp_name(ctx));
            }
            {
                LLVMValueRef free_args[] = { raw_arg };
                LLVMBuildCall2(ctx->builder, free_fn->fn_type, free_fn->fn,
                    free_args, 1, "");
            }
        }

        if (callee_entry->ret_type == ctx->type_void) {
            LLVMBuildCall2(ctx->builder, callee_entry->fn_type, callee_entry->fn,
                loaded_args, (unsigned)argc, "");
            LLVMBuildRet(ctx->builder, LLVMConstNull(ctx->type_i8ptr));
        } else {
            LLVMTargetDataRef td;
            uint64_t size;
            LLVMValueRef malloc_args[1];
            LLVMValueRef raw_result;
            LLVMValueRef typed_result;

            call_result = LLVMBuildCall2(ctx->builder, callee_entry->fn_type,
                callee_entry->fn, loaded_args, (unsigned)argc,
                llvm_tmp_name(ctx));
            td = LLVMGetModuleDataLayout(ctx->module);
            size = LLVMABISizeOfType(td, callee_entry->ret_type);
            malloc_args[0] = LLVMConstInt(ctx->type_i64, size, 0);
            raw_result = LLVMBuildCall2(ctx->builder, malloc_fn->fn_type,
                malloc_fn->fn, malloc_args, 1, llvm_tmp_name(ctx));
            typed_result = LLVMBuildBitCast(ctx->builder, raw_result,
                LLVMPointerType(callee_entry->ret_type, 0), llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, call_result, typed_result);
            LLVMBuildRet(ctx->builder, raw_result);
        }

        llvm_scope_pop(ctx);
        ctx->current_function = saved_fn;
        ctx->current_ret_type = saved_ret;
        if (saved_bb != NULL)
            LLVMPositionBuilderAtEnd(ctx->builder, saved_bb);

        if (argc > 0) {
            LLVMTargetDataRef td = LLVMGetModuleDataLayout(ctx->module);
            uint64_t size = LLVMABISizeOfType(td, arg_struct_type);
            LLVMValueRef malloc_args[] = { LLVMConstInt(ctx->type_i64, size, 0) };
            LLVMValueRef typed_arg;

            raw_spawn_arg = LLVMBuildCall2(ctx->builder, malloc_fn->fn_type,
                malloc_fn->fn, malloc_args, 1, llvm_tmp_name(ctx));
            typed_arg = LLVMBuildBitCast(ctx->builder, raw_spawn_arg,
                LLVMPointerType(arg_struct_type, 0), llvm_tmp_name(ctx));
            for (size_t i = 0; i < argc; i++) {
                LLVMValueRef gep = LLVMBuildStructGEP2(ctx->builder,
                    arg_struct_type, typed_arg, (unsigned)i, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder, args[i], gep);
            }
        }

        fn_ptr = LLVMBuildBitCast(ctx->builder, wrapper_fn, ctx->type_i8ptr,
            llvm_tmp_name(ctx));
        spawn_args[0] = fn_ptr;
        spawn_args[1] = raw_spawn_arg;
        handle = LLVMBuildCall2(ctx->builder, spawn_fn->fn_type, spawn_fn->fn,
            spawn_args, 2, llvm_tmp_name(ctx));

        return handle;
    }
}

#endif
