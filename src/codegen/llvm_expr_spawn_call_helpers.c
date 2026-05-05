/*
 * LLVM spawn/await expression lowering.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_spawn_call_helpers.h"

#include <stdio.h>
#include <string.h>

#include "llvm_boundary_slot_param.h"
#include "llvm_expr_boundary_projection_helpers.h"
#include "llvm_internal_api.h"
#include "llvm_inventory_decl_lookup.h"
#include "../common/string_compat.h"

LLVMValueRef
llvm_await_task_handle(LLVMGenCtx *ctx, ASTNode *node, LLVMValueRef task,
                       const char *inner, bool is_remote)
{
    LLVMFuncEntry *await_fn = llvm_lookup_function(ctx, "pgy_await_export");
    LLVMValueRef args[1];
    LLVMValueRef raw;
    LLVMTypeRef inner_ty;
    LLVMValueRef value;

    if (await_fn == NULL) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM await expression requires registered runtime function '%s'",
            "pgy_await_export");
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }
    if (task == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    args[0] = task;
    raw = LLVMBuildCall2(ctx->builder, await_fn->fn_type,
        await_fn->fn, args, 1, llvm_tmp_name(ctx));

    if (inner == NULL || strcmp(inner, "Void") == 0) {
        if (!is_remote)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        LLVMTypeRef result_ty = LLVMStructTypeInContext(ctx->context,
            (LLVMTypeRef[]){ ctx->type_i32, ctx->type_i32, ctx->type_i8ptr },
            3, 0);
        LLVMValueRef r = LLVMGetUndef(result_ty);
        r = LLVMBuildInsertValue(ctx->builder, r,
            LLVMConstInt(ctx->type_i32, 0, 0), 0, llvm_tmp_name(ctx));
        r = LLVMBuildInsertValue(ctx->builder, r,
            LLVMConstInt(ctx->type_i32, 0, 0), 1, llvm_tmp_name(ctx));
        r = LLVMBuildInsertValue(ctx->builder, r,
            LLVMConstNull(ctx->type_i8ptr), 2, llvm_tmp_name(ctx));
        return r;
    }

    inner_ty = pergyra_type_to_llvm(ctx, inner);
    if (strcmp(inner, "String") == 0) {
        LLVMValueRef typed_ptr = LLVMBuildBitCast(ctx->builder, raw,
            LLVMPointerType(ctx->type_i8ptr, 0), llvm_tmp_name(ctx));
        value = LLVMBuildLoad2(ctx->builder, ctx->type_i8ptr,
            typed_ptr, llvm_tmp_name(ctx));
    } else {
        LLVMValueRef typed_ptr = LLVMBuildBitCast(ctx->builder, raw,
            LLVMPointerType(inner_ty, 0), llvm_tmp_name(ctx));
        value = LLVMBuildLoad2(ctx->builder, inner_ty, typed_ptr,
            llvm_tmp_name(ctx));
    }

    if (!is_remote)
        return value;

    {
        LLVMTypeRef result_fields[] = { ctx->type_i32, inner_ty,
            ctx->type_i8ptr };
        LLVMTypeRef result_ty = LLVMStructTypeInContext(ctx->context,
            result_fields, 3, 0);
        LLVMValueRef result = LLVMGetUndef(result_ty);
        result = LLVMBuildInsertValue(ctx->builder, result,
            LLVMConstInt(ctx->type_i32, 0, 0), 0, llvm_tmp_name(ctx));
        result = LLVMBuildInsertValue(ctx->builder, result,
            value, 1, llvm_tmp_name(ctx));
        result = LLVMBuildInsertValue(ctx->builder, result,
            LLVMConstNull(ctx->type_i8ptr), 2, llvm_tmp_name(ctx));
        return result;
    }
}

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
    return ctx->type_i32;
}

LLVMFuncEntry *
llvm_resolve_callee_entry(LLVMGenCtx *ctx, const char *callee_name,
                          LLVMValueRef *args, size_t argc)
{
    ASTNode *generic_ast = llvm_lookup_generic_template(ctx, callee_name);
    char mangled[256];

    if (generic_ast == NULL)
        return llvm_lookup_function(ctx, callee_name);

    snprintf(mangled, sizeof(mangled), "%s", callee_name);
    for (size_t i = 0; i < argc; i++) {
        LLVMTypeRef at = (args != NULL && args[i] != NULL)
            ? LLVMTypeOf(args[i]) : ctx->type_i32;
        const char *suf = llvm_type_to_suffix(ctx, at);
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
            LLVMTypeRef concrete = (gi < argc && args != NULL && args[gi] != NULL)
                ? LLVMTypeOf(args[gi]) : ctx->type_i32;
            ctx->type_subst[ctx->type_subst_count].param_name =
                gp->params[gi]->name;
            ctx->type_subst[ctx->type_subst_count].llvm_type = concrete;
            ctx->type_subst[ctx->type_subst_count].type_name =
                llvm_type_to_suffix(ctx, concrete);
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
                    ptypes[real_pc++] = llvm_spawn_required_param_type(
                        ctx, generic_ast, p, callee_name);
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
                        snprintf(token_name, sizeof(token_name), "%s_token",
                            p->name);
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

    if (target == NULL)
        return LLVMConstNull(ctx->type_task_handle);

    if (target->type == AST_CALL) {
        call = target;
        callee = target->data.call.callee;
        argc = target->data.call.arg_count;
    } else {
        callee = target;
    }

    if (callee != NULL && callee->type == AST_IDENTIFIER)
        callee_name = callee->data.identifier.name;
    if (callee_name == NULL)
        return LLVMConstNull(ctx->type_task_handle);

    if (argc > 0) {
        args = pgy_arena_calloc(&ctx->scratch, argc * sizeof(LLVMValueRef));
        for (size_t i = 0; i < argc; i++)
            args[i] = llvm_emit_expression(call->data.call.arguments[i], ctx);
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
        return LLVMConstNull(ctx->type_task_handle);
    }
    if (callee_entry == NULL) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_SYMBOL_UNDEFINED,
            PGY_FIX_IMPORT_OR_DECLARE_SYMBOL,
            "LLVM spawn expression target '%s' is not declared in the backend function registry",
            callee_name);
        return LLVMConstNull(ctx->type_task_handle);
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

        snprintf(wrapper_name, sizeof(wrapper_name), "_pgy_spawn_expr_%d",
            wrapper_id);
        wrapper_type = LLVMFunctionType(ctx->type_i8ptr, wrapper_params, 1, 0);
        wrapper_fn = LLVMAddFunction(ctx->module, wrapper_name, wrapper_type);

        if (argc > 0) {
            LLVMTypeRef *field_types = pgy_arena_calloc(&ctx->scratch,
                argc * sizeof(LLVMTypeRef));
            for (size_t i = 0; i < argc; i++)
                field_types[i] = LLVMTypeOf(args[i]);
            arg_struct_type = LLVMStructTypeInContext(ctx->context, field_types,
                (unsigned)argc, 0);
        }

        ctx->current_function = wrapper_fn;
        ctx->current_ret_type = ctx->type_i8ptr;
        entry = LLVMAppendBasicBlockInContext(ctx->context, wrapper_fn, "entry");
        LLVMPositionBuilderAtEnd(ctx->builder, entry);
        llvm_scope_push(ctx);

        if (argc > 16)
            loaded_args = pgy_arena_calloc(&ctx->scratch,
                argc * sizeof(LLVMValueRef));

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
