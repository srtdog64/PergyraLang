/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_domain_event.h"

static LLVMTypeRef
llvm_domain_event_required_param_type(LLVMGenCtx *ctx,
                                      ASTNode *event_decl,
                                      ASTNode *type_node,
                                      const char *event_name)
{
    if (ctx == NULL)
        return NULL;
    if (type_node != NULL) {
        LLVMTypeRef type = ast_type_to_llvm(ctx, type_node);
        if (ctx->has_error || type == NULL)
            return NULL;
        return type;
    }

    llvm_set_error_at_with_hints(ctx, event_decl,
        PGY_CODE_LLVM_TYPE_UNSUPPORTED,
        PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
        PGY_FIX_ANNOTATE_CONCRETE_TYPE,
        "LLVM event '%s' parameter requires explicit type metadata; silent i32 fallback is not allowed",
        event_name != NULL ? event_name : "<anonymous>");
    return NULL;
}

static bool
llvm_domain_event_struct_name(char *out,
    size_t out_size,
    const char *event_name)
{
    int written;

    if (out == NULL || out_size == 0 || event_name == NULL)
        return false;

    written = snprintf(out, out_size, "PgyEvent_%s", event_name);
    return written >= 0 && (size_t)written < out_size;
}

static bool
llvm_domain_event_helper_name(char *out,
    size_t out_size,
    const char *event_name,
    const char *suffix)
{
    int written;

    if (out == NULL || out_size == 0 || event_name == NULL || suffix == NULL)
        return false;

    written = snprintf(out, out_size, "%s_%s", event_name, suffix);
    return written >= 0 && (size_t)written < out_size;
}

void
llvm_emit_domain_event_helpers(LLVMGenCtx *ctx,
    ASTNode **events,
    size_t event_count)
{
    LLVMBasicBlockRef saved_bb;

    if (ctx == NULL || events == NULL)
        return;

    saved_bb = LLVMGetInsertBlock(ctx->builder);

    /* Register event types and generate helper functions. */
    for (size_t i = 0; i < event_count; i++) {
        ASTNode *stmt = events[i];
        if (stmt == NULL || stmt->type != AST_EVENT_DECL)
            continue;

        const char *ename = ast_event_name(stmt);
        int pc = (int)ast_event_param_count(stmt);

        /* Event struct: { [16 x ptr], i64 } handlers + count */
        LLVMTypeRef handler_arr = LLVMArrayType(ctx->type_i8ptr,
                                                 PGY_EVENT_MAX_HANDLERS);
        LLVMTypeRef sfields[] = { handler_arr, ctx->type_i64 };
        char sname[256];
        if (!llvm_domain_event_struct_name(sname, sizeof(sname), ename)) {
            llvm_set_error(ctx, "event struct name is too long");
            goto restore_state;
        }
        LLVMTypeRef evt_struct = LLVMStructCreateNamed(ctx->context, sname);
        LLVMStructSetBody(evt_struct, sfields, 2, 0);

        /*
         * Handler parameter types must match the full event arity.  The
         * previous stack array only initialized the first 8 entries, which
         * made wider events read uninitialized LLVMTypeRef values.
         */
        size_t ptype_count = (pc > 0) ? (size_t)pc : 1;
        LLVMTypeRef *ptypes = pgy_arena_calloc(&ctx->scratch,
            ptype_count * sizeof(LLVMTypeRef));
        if (ptypes == NULL) {
            llvm_set_error(ctx, "event parameter type allocation failed");
            goto restore_state;
        }
        for (int j = 0; j < pc; j++) {
            ASTNode *p = ast_event_param(stmt, (size_t)j);
            ASTNode *param_type = (p != NULL) ? ast_let_type(p) : NULL;
            ptypes[j] = llvm_domain_event_required_param_type(
                ctx, stmt, param_type, ename);
            if (ctx->has_error || ptypes[j] == NULL)
                goto restore_state;
        }
        llvm_register_event(ctx, ename, evt_struct, pc, ptypes);

        /* Handler function type: void(param_types...) */
        LLVMTypeRef handler_ft = LLVMFunctionType(ctx->type_void,
            ptypes, (unsigned)pc, 0);
        LLVMTypeRef handler_ptr_t = LLVMPointerTypeInContext(ctx->context, 0);
        (void)handler_ptr_t;

        /* --- Generate EventName_INIT(ptr) -> void --- */
        {
            char fname[256];
            if (!llvm_domain_event_helper_name(fname, sizeof(fname),
                    ename, "INIT")) {
                llvm_set_error(ctx, "event init helper name is too long");
                goto restore_state;
            }
            LLVMTypeRef init_params[] = { ctx->type_i8ptr };
            LLVMTypeRef init_ft = LLVMFunctionType(ctx->type_void,
                init_params, 1, 0);
            LLVMValueRef init_fn = LLVMAddFunction(ctx->module, fname, init_ft);
            llvm_register_function(ctx, LLVMGetValueName(init_fn),
                init_fn, init_ft, ctx->type_void);

            LLVMBasicBlockRef bb = LLVMAppendBasicBlockInContext(
                ctx->context, init_fn, "entry");
            LLVMPositionBuilderAtEnd(ctx->builder, bb);

            /* memset(e, 0, sizeof(struct)) */
            LLVMValueRef e_ptr = LLVMGetParam(init_fn, 0);
            LLVMValueRef sz = LLVMSizeOf(evt_struct);
            LLVMBuildMemSet(ctx->builder, e_ptr,
                LLVMConstInt(LLVMInt8TypeInContext(ctx->context), 0, 0),
                sz, 0);
            LLVMBuildRetVoid(ctx->builder);
        }

        /* --- Generate EventName_SUBSCRIBE(ptr, handler_ptr) -> void --- */
        {
            char fname[256];
            if (!llvm_domain_event_helper_name(fname, sizeof(fname),
                    ename, "SUBSCRIBE")) {
                llvm_set_error(ctx, "event subscribe helper name is too long");
                goto restore_state;
            }
            LLVMTypeRef sub_params[] = { ctx->type_i8ptr, ctx->type_i8ptr };
            LLVMTypeRef sub_ft = LLVMFunctionType(ctx->type_void,
                sub_params, 2, 0);
            LLVMValueRef sub_fn = LLVMAddFunction(ctx->module, fname, sub_ft);
            llvm_register_function(ctx, LLVMGetValueName(sub_fn),
                sub_fn, sub_ft, ctx->type_void);

            LLVMBasicBlockRef bb = LLVMAppendBasicBlockInContext(
                ctx->context, sub_fn, "entry");
            LLVMPositionBuilderAtEnd(ctx->builder, bb);

            LLVMValueRef e_ptr = LLVMGetParam(sub_fn, 0);
            LLVMValueRef h_ptr = LLVMGetParam(sub_fn, 1);
            LLVMFuncEntry *panic_fn = llvm_lookup_function(ctx,
                "pgy_runtime_panic_internal_invariant_export");
            if (panic_fn == NULL) {
                llvm_set_error(ctx,
                    "event subscribe overflow requires registered panic runtime");
                goto restore_state;
            }

            /* count_ptr = GEP(e, 0, 1), the i64 count field */
            LLVMValueRef count_ptr = LLVMBuildStructGEP2(ctx->builder,
                evt_struct, e_ptr, 1, "count_ptr");
            LLVMValueRef count = LLVMBuildLoad2(ctx->builder,
                ctx->type_i64, count_ptr, "count");

            /* if (count < 16) { handlers[count] = h; count++; } */
            LLVMValueRef max_h = LLVMConstInt(ctx->type_i64,
                PGY_EVENT_MAX_HANDLERS, 0);
            LLVMValueRef cmp = LLVMBuildICmp(ctx->builder,
                LLVMIntULT, count, max_h, "cmp");

            LLVMBasicBlockRef then_bb = LLVMAppendBasicBlockInContext(
                ctx->context, sub_fn, "then");
            LLVMBasicBlockRef fail_bb = LLVMAppendBasicBlockInContext(
                ctx->context, sub_fn, "event.subscribe.overflow");
            LLVMBasicBlockRef end_bb = LLVMAppendBasicBlockInContext(
                ctx->context, sub_fn, "end");
            LLVMBuildCondBr(ctx->builder, cmp, then_bb, fail_bb);

            LLVMPositionBuilderAtEnd(ctx->builder, then_bb);
            /* handlers_ptr = GEP(e, 0, 0, count) */
            LLVMValueRef idx[] = {
                LLVMConstInt(ctx->type_i32, 0, 0),
                LLVMConstInt(ctx->type_i32, 0, 0),
                count
            };
            LLVMValueRef slot = LLVMBuildGEP2(ctx->builder,
                evt_struct, e_ptr, idx, 3, "slot");
            LLVMBuildStore(ctx->builder, h_ptr, slot);

            /* count++ */
            LLVMValueRef new_count = LLVMBuildAdd(ctx->builder,
                count, LLVMConstInt(ctx->type_i64, 1, 0), "new_count");
            LLVMBuildStore(ctx->builder, new_count, count_ptr);
            LLVMBuildBr(ctx->builder, end_bb);

            LLVMPositionBuilderAtEnd(ctx->builder, fail_bb);
            {
                LLVMValueRef reason = LLVMBuildGlobalStringPtr(ctx->builder,
                    "event handler capacity exceeded", "event.overflow.reason");
                LLVMBuildCall2(ctx->builder, panic_fn->fn_type, panic_fn->fn,
                    &reason, 1, "");
                LLVMBuildUnreachable(ctx->builder);
            }

            LLVMPositionBuilderAtEnd(ctx->builder, end_bb);
            LLVMBuildRetVoid(ctx->builder);
        }

        /* --- Generate EventName_UNSUBSCRIBE(ptr, handler_ptr) -> void --- */
        {
            char fname[256];
            if (!llvm_domain_event_helper_name(fname, sizeof(fname),
                    ename, "UNSUBSCRIBE")) {
                llvm_set_error(ctx,
                    "event unsubscribe helper name is too long");
                goto restore_state;
            }
            LLVMTypeRef unsub_params[] = { ctx->type_i8ptr, ctx->type_i8ptr };
            LLVMTypeRef unsub_ft = LLVMFunctionType(ctx->type_void,
                unsub_params, 2, 0);
            LLVMValueRef unsub_fn = LLVMAddFunction(ctx->module, fname, unsub_ft);
            llvm_register_function(ctx, LLVMGetValueName(unsub_fn),
                unsub_fn, unsub_ft, ctx->type_void);

            LLVMBasicBlockRef entry_bb = LLVMAppendBasicBlockInContext(
                ctx->context, unsub_fn, "entry");
            LLVMPositionBuilderAtEnd(ctx->builder, entry_bb);

            LLVMValueRef e_ptr = LLVMGetParam(unsub_fn, 0);
            LLVMValueRef h_ptr = LLVMGetParam(unsub_fn, 1);

            LLVMValueRef count_ptr = LLVMBuildStructGEP2(ctx->builder,
                evt_struct, e_ptr, 1, "count_ptr");
            LLVMValueRef count = LLVMBuildLoad2(ctx->builder,
                ctx->type_i64, count_ptr, "count");

            /* Loop: for (i = 0; i < count; i++) */
            LLVMValueRef i_alloca = LLVMBuildAlloca(ctx->builder,
                ctx->type_i64, "i");
            LLVMBuildStore(ctx->builder,
                LLVMConstInt(ctx->type_i64, 0, 0), i_alloca);

            LLVMBasicBlockRef loop_bb = LLVMAppendBasicBlockInContext(
                ctx->context, unsub_fn, "loop");
            LLVMBasicBlockRef found_bb = LLVMAppendBasicBlockInContext(
                ctx->context, unsub_fn, "found");
            LLVMBasicBlockRef next_bb = LLVMAppendBasicBlockInContext(
                ctx->context, unsub_fn, "next");
            LLVMBasicBlockRef done_bb = LLVMAppendBasicBlockInContext(
                ctx->context, unsub_fn, "done");

            LLVMBuildBr(ctx->builder, loop_bb);
            LLVMPositionBuilderAtEnd(ctx->builder, loop_bb);

            LLVMValueRef iv = LLVMBuildLoad2(ctx->builder,
                ctx->type_i64, i_alloca, "iv");
            LLVMValueRef cmp = LLVMBuildICmp(ctx->builder,
                LLVMIntULT, iv, count, "cmp");
            LLVMBasicBlockRef body_bb = LLVMAppendBasicBlockInContext(
                ctx->context, unsub_fn, "body");
            LLVMBuildCondBr(ctx->builder, cmp, body_bb, done_bb);

            LLVMPositionBuilderAtEnd(ctx->builder, body_bb);
            LLVMValueRef idx[] = {
                LLVMConstInt(ctx->type_i32, 0, 0),
                LLVMConstInt(ctx->type_i32, 0, 0),
                iv
            };
            LLVMValueRef slot = LLVMBuildGEP2(ctx->builder,
                evt_struct, e_ptr, idx, 3, "slot");
            LLVMValueRef val = LLVMBuildLoad2(ctx->builder,
                ctx->type_i8ptr, slot, "hval");
            LLVMValueRef eq = LLVMBuildICmp(ctx->builder,
                LLVMIntEQ, val, h_ptr, "eq");
            LLVMBuildCondBr(ctx->builder, eq, found_bb, next_bb);

            /* found: replace handlers[i] with handlers[count - 1], count-- */
            LLVMPositionBuilderAtEnd(ctx->builder, found_bb);
            LLVMValueRef last_idx_val = LLVMBuildSub(ctx->builder,
                count, LLVMConstInt(ctx->type_i64, 1, 0), "last");
            LLVMValueRef last_gep_idx[] = {
                LLVMConstInt(ctx->type_i32, 0, 0),
                LLVMConstInt(ctx->type_i32, 0, 0),
                last_idx_val
            };
            LLVMValueRef last_slot = LLVMBuildGEP2(ctx->builder,
                evt_struct, e_ptr, last_gep_idx, 3, "last_slot");
            LLVMValueRef last_val = LLVMBuildLoad2(ctx->builder,
                ctx->type_i8ptr, last_slot, "last_val");
            LLVMBuildStore(ctx->builder, last_val, slot);
            LLVMBuildStore(ctx->builder, last_idx_val, count_ptr);
            LLVMBuildBr(ctx->builder, done_bb);

            /* next: i++ */
            LLVMPositionBuilderAtEnd(ctx->builder, next_bb);
            LLVMValueRef inc = LLVMBuildAdd(ctx->builder,
                iv, LLVMConstInt(ctx->type_i64, 1, 0), "inc");
            LLVMBuildStore(ctx->builder, inc, i_alloca);
            LLVMBuildBr(ctx->builder, loop_bb);

            LLVMPositionBuilderAtEnd(ctx->builder, done_bb);
            LLVMBuildRetVoid(ctx->builder);
        }

        /* --- Generate EventName_INVOKE(ptr, params...) -> void --- */
        {
            char fname[256];
            if (!llvm_domain_event_helper_name(fname, sizeof(fname),
                    ename, "INVOKE")) {
                llvm_set_error(ctx, "event invoke helper name is too long");
                goto restore_state;
            }
            /*
             * params: ptr (event), then handler params.  Consumed by
             * LLVMFunctionType (which copies the type array) and never
             * retained beyond this block.
             */
            LLVMTypeRef *inv_params = pgy_arena_calloc(&ctx->scratch,
                (size_t)(pc + 1) * sizeof(LLVMTypeRef));
            if (inv_params == NULL) {
                llvm_set_error(ctx, "event invoke parameter allocation failed");
                goto restore_state;
            }
            inv_params[0] = ctx->type_i8ptr;
            for (int j = 0; j < pc; j++)
                inv_params[j + 1] = ptypes[j];

            LLVMTypeRef inv_ft = LLVMFunctionType(ctx->type_void,
                inv_params, (unsigned)(pc + 1), 0);
            LLVMValueRef inv_fn = LLVMAddFunction(ctx->module, fname, inv_ft);
            llvm_register_function(ctx, LLVMGetValueName(inv_fn),
                inv_fn, inv_ft, ctx->type_void);

            LLVMBasicBlockRef entry_bb = LLVMAppendBasicBlockInContext(
                ctx->context, inv_fn, "entry");
            LLVMPositionBuilderAtEnd(ctx->builder, entry_bb);

            LLVMValueRef e_ptr = LLVMGetParam(inv_fn, 0);
            LLVMValueRef count_ptr = LLVMBuildStructGEP2(ctx->builder,
                evt_struct, e_ptr, 1, "count_ptr");
            LLVMValueRef count = LLVMBuildLoad2(ctx->builder,
                ctx->type_i64, count_ptr, "count");

            LLVMValueRef i_alloca = LLVMBuildAlloca(ctx->builder,
                ctx->type_i64, "i");
            LLVMBuildStore(ctx->builder,
                LLVMConstInt(ctx->type_i64, 0, 0), i_alloca);

            LLVMBasicBlockRef loop_bb = LLVMAppendBasicBlockInContext(
                ctx->context, inv_fn, "loop");
            LLVMBasicBlockRef call_bb = LLVMAppendBasicBlockInContext(
                ctx->context, inv_fn, "call");
            LLVMBasicBlockRef done_bb = LLVMAppendBasicBlockInContext(
                ctx->context, inv_fn, "done");

            LLVMBuildBr(ctx->builder, loop_bb);
            LLVMPositionBuilderAtEnd(ctx->builder, loop_bb);

            LLVMValueRef iv = LLVMBuildLoad2(ctx->builder,
                ctx->type_i64, i_alloca, "iv");
            LLVMValueRef cmp = LLVMBuildICmp(ctx->builder,
                LLVMIntULT, iv, count, "cmp");
            LLVMBuildCondBr(ctx->builder, cmp, call_bb, done_bb);

            LLVMPositionBuilderAtEnd(ctx->builder, call_bb);
            /* Load handler pointer */
            LLVMValueRef idx[] = {
                LLVMConstInt(ctx->type_i32, 0, 0),
                LLVMConstInt(ctx->type_i32, 0, 0),
                iv
            };
            LLVMValueRef slot = LLVMBuildGEP2(ctx->builder,
                evt_struct, e_ptr, idx, 3, "slot");
            LLVMValueRef hval = LLVMBuildLoad2(ctx->builder,
                ctx->type_i8ptr, slot, "hval");

            /*
             * Call handler(params...) via indirect call.  Arg buffer is
             * consumed by LLVMBuildCall2 and never retained.
             */
            LLVMValueRef *call_args = pgy_arena_calloc(&ctx->scratch,
                (size_t)pc * sizeof(LLVMValueRef));
            if (call_args == NULL && pc > 0) {
                llvm_set_error(ctx, "event invoke call argument allocation failed");
                goto restore_state;
            }
            for (int j = 0; j < pc; j++)
                call_args[j] = LLVMGetParam(inv_fn, (unsigned)(j + 1));
            LLVMBuildCall2(ctx->builder, handler_ft, hval,
                call_args, (unsigned)pc, "");

            /* i++ */
            LLVMValueRef inc = LLVMBuildAdd(ctx->builder,
                iv, LLVMConstInt(ctx->type_i64, 1, 0), "inc");
            LLVMBuildStore(ctx->builder, inc, i_alloca);
            LLVMBuildBr(ctx->builder, loop_bb);

            LLVMPositionBuilderAtEnd(ctx->builder, done_bb);
            LLVMBuildRetVoid(ctx->builder);
        }

        /* Create global variable for this event */
        LLVMValueRef gv = LLVMAddGlobal(ctx->module, evt_struct, ename);
        LLVMSetInitializer(gv, LLVMConstNull(evt_struct));
        LLVMSetLinkage(gv, LLVMInternalLinkage);
    }

restore_state:
    if (saved_bb != NULL)
        LLVMPositionBuilderAtEnd(ctx->builder, saved_bb);
}

#endif /* PGY_LLVM_ENABLED */
