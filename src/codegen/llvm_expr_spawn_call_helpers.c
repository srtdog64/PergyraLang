/*
 * LLVM spawn/await expression lowering.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_spawn_call_helpers.h"
#include "llvm_expr_spawn_names.h"

#include <limits.h>
#include <stdint.h>

#include "llvm_internal_api.h"

LLVMValueRef
llvm_emit_spawn_expr(ASTNode *node, LLVMGenCtx *ctx)
{
    ASTNode *target = ast_spawn_function(node);
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
        callee = ast_call_callee(target);
        argc = ast_call_arg_count(target);
    } else {
        callee = target;
    }

    if (callee != NULL && callee->type == AST_IDENTIFIER)
        callee_name = ast_identifier_name(callee);
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
            args[i] = llvm_emit_expression(ast_call_argument(call, i), ctx);
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
        ast_spawn_is_blocking(node)
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
            ast_spawn_is_blocking(node)
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
