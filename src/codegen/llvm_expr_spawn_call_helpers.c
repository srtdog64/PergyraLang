/*
 * LLVM spawn/await expression lowering.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_spawn_call_helpers.h"
#include "llvm_expr_spawn_names.h"
#include "llvm_expr_spawn_worker_boundary.h"
#include "llvm_mir_signature.h"
#include "../compiler/execution_lane.h"
#include "../compiler/verified_projection_plan.h"
#include "../parser/ast_api.h"

#include <limits.h>
#include <stdint.h>

#include "llvm_internal_api.h"

static LLVMFuncEntry *
llvm_spawn_required_panic_fn(LLVMGenCtx *ctx, ASTNode *node)
{
    LLVMFuncEntry *panic_fn = llvm_lookup_function(ctx,
        "pgy_runtime_panic_internal_invariant_export");
    if (panic_fn == NULL && ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM spawn expression requires registered runtime function '%s'",
            "pgy_runtime_panic_internal_invariant_export");
    }
    return panic_fn;
}

static bool
llvm_spawn_emit_nonnull_guard(LLVMGenCtx *ctx, ASTNode *node,
                              LLVMValueRef ptr,
                              LLVMFuncEntry *panic_fn,
                              LLVMFuncEntry *free_fn,
                              LLVMValueRef cleanup_ptr,
                              const char *reason)
{
    LLVMValueRef is_null;
    LLVMBasicBlockRef fail_bb;
    LLVMBasicBlockRef cont_bb;

    if (ctx == NULL || ptr == NULL || panic_fn == NULL)
        return false;
    if (ctx->current_function == NULL) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM spawn expression null guard requires an active function");
        return false;
    }

    is_null = LLVMBuildICmp(ctx->builder, LLVMIntEQ, ptr,
        LLVMConstNull(ctx->type_i8ptr), llvm_tmp_name(ctx));
    fail_bb = LLVMAppendBasicBlockInContext(ctx->context,
        ctx->current_function, "pgy.spawn.fail");
    cont_bb = LLVMAppendBasicBlockInContext(ctx->context,
        ctx->current_function, "pgy.spawn.cont");
    LLVMBuildCondBr(ctx->builder, is_null, fail_bb, cont_bb);

    LLVMPositionBuilderAtEnd(ctx->builder, fail_bb);
    if (cleanup_ptr != NULL && free_fn != NULL) {
        LLVMValueRef free_args[] = { cleanup_ptr };
        LLVMBuildCall2(ctx->builder, free_fn->fn_type, free_fn->fn,
            free_args, 1, "");
    }
    {
        LLVMValueRef reason_arg = LLVMBuildGlobalStringPtr(ctx->builder,
            reason != NULL ? reason : "LLVM spawn expression failed",
            llvm_tmp_name(ctx));
        LLVMBuildCall2(ctx->builder, panic_fn->fn_type, panic_fn->fn,
            &reason_arg, 1, "");
    }
    LLVMBuildUnreachable(ctx->builder);

    LLVMPositionBuilderAtEnd(ctx->builder, cont_bb);
    return true;
}

LLVMValueRef
llvm_emit_spawn_expr(ASTNode *node, LLVMGenCtx *ctx)
{
    ASTNode *target = ast_spawn_function(node);
    ASTNode *call = NULL;
    ASTNode *callee = NULL;
    const char *callee_name = NULL;
    size_t argc = 0;
    LLVMValueRef *args = NULL;
    ASTNode *callee_decl = NULL;
    LLVMFuncEntry *callee_entry = NULL;
    const MIRRoutine *callee_routine = NULL;
    bool callee_has_mir_signature = false;
    bool callee_is_generic_func = false;
    bool callee_is_extern_func = false;
    bool allow_ast_compat = false;
    LLVMValueRef callee_fn = NULL;
    LLVMTypeRef callee_fn_type = NULL;
    LLVMTypeRef callee_ret_type = NULL;
    LLVMFuncEntry *spawn_fn = NULL;
    LLVMFuncEntry *malloc_fn = NULL;
    LLVMFuncEntry *free_fn = NULL;
    LLVMFuncEntry *panic_fn = NULL;
    PgyExecutionLane spawn_lane = PGY_LANE_REJECT;
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
    callee_decl = llvm_find_function_decl(ctx, callee_name);
    if (callee_decl != NULL && callee_decl->type == AST_FUNC_DECL) {
        callee_is_extern_func = llvm_decl_is_extern_function(ctx, callee_decl);
        if (!callee_is_extern_func && llvm_active_has_mir(ctx)) {
            callee_routine =
                llvm_active_function_routine_by_name(ctx, callee_name);
        }
        callee_is_generic_func =
            llvm_mir_or_ast_function_is_generic(callee_routine, callee_decl);
        if (!callee_is_generic_func && !callee_is_extern_func
            && llvm_active_has_mir(ctx)) {
            if (callee_routine == NULL) {
                llvm_set_mir_inventory_missing(ctx,
                    "MIR-only LLVM path missing spawn routine for '%s'",
                    callee_name != NULL ? callee_name : "<function>");
                return NULL;
            }
            if (!llvm_mir_routine_signature_metadata_complete_for(ctx,
                    callee_routine, callee_decl,
                    LLVM_MIR_SIGNATURE_REQUIRE_PARAM_TYPE_NAMES,
                    "MIR-only LLVM path missing spawn signature metadata for '%s'",
                    NULL,
                    "MIR-only LLVM path missing spawn parameter type-name metadata for '%s'")) {
                return NULL;
            }
            callee_has_mir_signature = true;
        }
    }
    allow_ast_compat = callee_decl != NULL
        && callee_decl->type == AST_FUNC_DECL
        && (callee_is_generic_func
            || callee_is_extern_func);

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
            ASTNode *arg = ast_call_argument(call, i);
            FuncParam *param = NULL;
            const char *param_type_name = NULL;
            if (callee_has_mir_signature) {
                if (i < llvm_mir_routine_param_count(callee_routine)) {
                    param = llvm_mir_routine_param(callee_routine, i);
                    param_type_name =
                        llvm_mir_routine_param_type_name(callee_routine, i);
                }
            } else if (allow_ast_compat) {
                param = ast_func_param(callee_decl, i);
            }
            if (llvm_spawn_reject_worker_storage_param(ctx, node, param,
                    param_type_name, i, callee_name)) {
                return NULL;
            }
            if (llvm_spawn_reject_worker_storage_arg(ctx, node, arg, i,
                    callee_name)) {
                return NULL;
            }
            args[i] = llvm_emit_expression(arg, ctx);
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
    /* SEA: LLVM spawn lowering consumes the verified ExecutionLane fact
       carried from AIR (the spawn-lane plan); the backend does not recover
       the lane from source spelling. The runtime facade owns the concrete
       executor mapping. A spawn site the plan does not cover is fail-closed. */
    if (!pgy_verified_spawn_lane_plan_lookup(ctx->spawn_lane_plan, ast_node_stable_id(node),
            &spawn_lane)) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM spawn expression has no verified execution-lane fact in the AIR spawn-lane plan");
        return NULL;
    }
    const char *spawn_export_name = "pgy_lane_spawn_dispatch_export";
    spawn_fn = llvm_lookup_function(ctx, spawn_export_name);
    malloc_fn = llvm_lookup_function(ctx, "malloc");
    free_fn = llvm_lookup_function(ctx, "free");
    panic_fn = llvm_spawn_required_panic_fn(ctx, node);
    if (spawn_fn == NULL || malloc_fn == NULL || free_fn == NULL
        || panic_fn == NULL) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM spawn expression requires registered runtime functions '%s', 'malloc', 'free', and panic",
            spawn_export_name);
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
    callee_fn = callee_entry->fn;
    callee_fn_type = callee_entry->fn_type;
    callee_ret_type = callee_entry->ret_type;

    {
        LLVMValueRef saved_fn = ctx->current_function;
        LLVMTypeRef saved_ret = ctx->current_ret_type;
        LLVMTypeRef saved_function_ret = ctx->current_function_ret_type;
        const char *saved_return_type_name = ctx->current_return_type_name;
        ASTNode *saved_return_callable_type = ctx->current_return_callable_type;
        LLVMBasicBlockRef saved_bb = LLVMGetInsertBlock(ctx->builder);
        LLVMLexicalRegistrySnapshot lexical_snapshot =
            llvm_lexical_registry_snapshot(ctx);
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
        LLVMValueRef spawn_args[3];
        LLVMValueRef handle;
        LLVMBasicBlockRef entry;

        if (!llvm_spawn_wrapper_name(ctx, node, wrapper_name,
                sizeof(wrapper_name), wrapper_id))
            return NULL;

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

        wrapper_type = LLVMFunctionType(ctx->type_i8ptr, wrapper_params, 1, 0);
        wrapper_fn = LLVMAddFunction(ctx->module, wrapper_name, wrapper_type);

        ctx->current_function = wrapper_fn;
        ctx->current_ret_type = ctx->type_i8ptr;
        ctx->current_function_ret_type = ctx->type_i8ptr;
        ctx->current_return_type_name = NULL;
        ctx->current_return_callable_type = NULL;
        entry = LLVMAppendBasicBlockInContext(ctx->context, wrapper_fn, "entry");
        LLVMPositionBuilderAtEnd(ctx->builder, entry);
        llvm_scope_push(ctx);
        if (ctx->has_error) {
            llvm_lexical_registry_restore(ctx, lexical_snapshot);
            ctx->current_function = saved_fn;
            ctx->current_ret_type = saved_ret;
            ctx->current_function_ret_type = saved_function_ret;
            ctx->current_return_type_name = saved_return_type_name;
            ctx->current_return_callable_type = saved_return_callable_type;
            if (saved_bb != NULL)
                LLVMPositionBuilderAtEnd(ctx->builder, saved_bb);
            return NULL;
        }

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

        if (callee_ret_type == ctx->type_void) {
            LLVMBuildCall2(ctx->builder, callee_fn_type, callee_fn,
                loaded_args, (unsigned)argc, "");
            LLVMBuildRet(ctx->builder, LLVMConstNull(ctx->type_i8ptr));
        } else {
            LLVMTargetDataRef td;
            uint64_t size;
            LLVMValueRef malloc_args[1];
            LLVMValueRef raw_result;
            LLVMValueRef typed_result;

            call_result = LLVMBuildCall2(ctx->builder, callee_fn_type,
                callee_fn, loaded_args, (unsigned)argc,
                llvm_tmp_name(ctx));
            td = LLVMGetModuleDataLayout(ctx->module);
            size = LLVMABISizeOfType(td, callee_ret_type);
            malloc_args[0] = LLVMConstInt(ctx->type_i64, size, 0);
            raw_result = LLVMBuildCall2(ctx->builder, malloc_fn->fn_type,
                malloc_fn->fn, malloc_args, 1, llvm_tmp_name(ctx));
            typed_result = LLVMBuildBitCast(ctx->builder, raw_result,
                LLVMPointerType(callee_ret_type, 0), llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, call_result, typed_result);
            LLVMBuildRet(ctx->builder, raw_result);
        }

        llvm_scope_pop(ctx);
        llvm_lexical_registry_restore(ctx, lexical_snapshot);
        ctx->current_function = saved_fn;
        ctx->current_ret_type = saved_ret;
        ctx->current_function_ret_type = saved_function_ret;
        ctx->current_return_type_name = saved_return_type_name;
        ctx->current_return_callable_type = saved_return_callable_type;
        if (saved_bb != NULL)
            LLVMPositionBuilderAtEnd(ctx->builder, saved_bb);

        if (argc > 0) {
            LLVMTargetDataRef td = LLVMGetModuleDataLayout(ctx->module);
            uint64_t size = LLVMABISizeOfType(td, arg_struct_type);
            LLVMValueRef malloc_args[] = { LLVMConstInt(ctx->type_i64, size, 0) };
            LLVMValueRef typed_arg;

            raw_spawn_arg = LLVMBuildCall2(ctx->builder, malloc_fn->fn_type,
                malloc_fn->fn, malloc_args, 1, llvm_tmp_name(ctx));
            if (!llvm_spawn_emit_nonnull_guard(ctx, node, raw_spawn_arg,
                    panic_fn, NULL, NULL,
                    "LLVM spawn argument allocation failed")) {
                return NULL;
            }
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
        spawn_args[0] = LLVMConstInt(ctx->type_i32,
            (unsigned long long)spawn_lane, 0);
        spawn_args[1] = fn_ptr;
        spawn_args[2] = raw_spawn_arg;
        handle = LLVMBuildCall2(ctx->builder, spawn_fn->fn_type, spawn_fn->fn,
            spawn_args, 3, llvm_tmp_name(ctx));
        {
            LLVMValueRef task = LLVMBuildExtractValue(ctx->builder, handle, 0,
                llvm_tmp_name(ctx));
            if (!llvm_spawn_emit_nonnull_guard(ctx, node, task,
                    panic_fn, free_fn, raw_spawn_arg,
                    "LLVM spawn task creation failed")) {
                return NULL;
            }
        }

        return handle;
    }
}

#endif
