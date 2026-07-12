#ifdef PGY_LLVM_ENABLED
/*
 * LLVM emission for the join-form parallel block (docs/181 SS1, rungs 0+1):
 *   parallel (x in xs)     [join with all] { body }   element mode
 *   parallel (i in lo..hi) [join with all] { body }   index mode (R1)
 *
 * One wrapper function, N runtime tasks: the call site computes the
 * fan-out length (collection length, or hi-lo clamped at zero),
 * stack-allocates N capture contexts and N task handles, fans out
 * through the worker pool in a runtime loop, and joins on every handle.
 * Captures travel by pointer exactly like the arms form; the element or
 * index travels by value in a per-context field. Array captures are
 * admitted from the checker-sealed index-disjointness fact list only and
 * re-register their registry binding inside the wrapper (the registry is
 * keyed by the scope alloca, which differs across the boundary).
 * Induction variables live in allocas so the guard's inserted blocks
 * never disturb SSA form.
 */

#include "llvm_internal.h"
#include "llvm_stmt_parallel_names.h"
#include "../common/execution_lane_kind.h"
#include "../parser/ast_analysis.h"
#include "../parser/ast_api.h"

#include <string.h>

/* Rung-0 capture cap (mirrors the C emitter's MAX_SLOT_VARS scale). */
#define PJOIN_MAX_CAPTURES 64

typedef struct {
    const char *name;
    LLVMValueRef alloca;
    LLVMTypeRef type;
    const char *channel_inner;
    const char *future_inner;
    const char *slot_inner;
    bool future_is_remote;
    bool slot_is_secure;
    /* Fact-admitted array (docs/181 R1 index-disjointness or R5
     * snapshot-read): element metadata stashed by value so the wrapper
     * can re-register the binding. */
    bool is_admitted_array;
    LLVMTypeRef array_elem_type;
    const char *array_elem_name;
} JoinCapturedVar;

static void
llvm_join_set_error(LLVMGenCtx *ctx, ASTNode *node, const char *fmt,
                    const char *arg)
{
    llvm_set_error_at_with_hints(ctx, node,
        PGY_CODE_LLVM_TYPE_UNSUPPORTED,
        PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
        PGY_FIX_INSPECT_MIR_INVENTORY,
        fmt, arg != NULL ? arg : "<binding>");
}

/* Shared emission for both forms; expression mode (result_out != NULL)
 * adds a per-task result slot to the context struct, wires `give` to it
 * through ctx->pjoin_give_ptr, and materializes an Array<R> value in
 * index order after the join. */
static void
llvm_emit_parallel_join_common(ASTNode *node, LLVMGenCtx *ctx,
                               LLVMValueRef *result_out)
{
    const char *elem_name = ast_parallel_join_element(node);
    ASTNode *coll = ast_parallel_join_collection(node);
    ASTNode *range_end = ast_parallel_join_range_end(node);
    bool index_mode = range_end != NULL;
    bool expr_mode = result_out != NULL;
    const char *give_name = ast_parallel_join_give_type(node);
    LLVMTypeRef give_type = NULL;
    ASTNode *body = ast_parallel_task(node, 0);

    if (expr_mode) {
        *result_out = NULL;
        if (give_name == NULL || give_name[0] == '\0') {
            llvm_join_set_error(ctx, node,
                "LLVM parallel join expression lacks the checker-sealed give-result fact%s",
                "");
            return;
        }
        give_type = pgy_kind_to_llvm(ctx, pgy_classify_type(give_name));
        if (give_type == NULL) {
            llvm_join_set_error(ctx, node,
                "LLVM parallel join give result '%s' has no primitive lowering",
                give_name);
            return;
        }
    }
    const char *coll_name = NULL;
    LLVMVarEntry coll_var = {0};
    LLVMArrayVarEntry *coll_entry = NULL;
    LLVMTypeRef elem_type;

    if (!ast_parallel_dispositions_sealed(node)) {
        llvm_join_set_error(ctx, node,
            "LLVM parallel join block reached the emitter without checker-sealed capture dispositions%s",
            "");
        return;
    }
    if (elem_name == NULL || body == NULL || coll == NULL
        || (!index_mode && coll->type != AST_IDENTIFIER)) {
        llvm_join_set_error(ctx, node,
            "LLVM parallel join block reached the emitter in a non-admitted shape%s",
            "");
        return;
    }
    if (index_mode) {
        /* The binding is the Int index (i32; parity with the C int32_t). */
        elem_type = ctx->type_i32;
    } else {
        coll_name = ast_identifier_name(coll);
        if (!llvm_scope_lookup_snapshot(ctx, coll_name, &coll_var)
            || coll_var.alloca == NULL || coll_var.type == NULL) {
            llvm_join_set_error(ctx, node,
                "LLVM parallel join collection '%s' requires a storage-backed binding",
                coll_name);
            return;
        }
        coll_entry = llvm_lookup_array_var(ctx, coll_name);
        if (coll_entry == NULL || coll_entry->elem_type == NULL) {
            llvm_join_set_error(ctx, node,
                "LLVM parallel join collection '%s' requires concrete Array<T> element metadata",
                coll_name);
            return;
        }
        elem_type = coll_entry->elem_type;
    }

    LLVMFuncEntry *spawn_fn = llvm_lookup_function(ctx,
        "pgy_lane_spawn_dispatch_export");
    LLVMFuncEntry *await_fn = llvm_lookup_function(ctx, "pgy_await_export");
    char get_fn_name[64] = "";
    LLVMFuncEntry *get_fn = NULL;
    if (!index_mode) {
        const char *elem_suffix = llvm_type_to_suffix(ctx, elem_type);
        if (elem_suffix == NULL || strcmp(elem_suffix, "Unknown") == 0) {
            llvm_join_set_error(ctx, node,
                "LLVM parallel join rung 0 requires a primitive element lowering for '%s'",
                coll_name);
            return;
        }
        if (snprintf(get_fn_name, sizeof(get_fn_name), "pgy_array_get_%s",
                     elem_suffix) < (int)sizeof(get_fn_name))
            get_fn = llvm_lookup_function(ctx, get_fn_name);
    }
    if (spawn_fn == NULL || await_fn == NULL
        || (!index_mode && get_fn == NULL)) {
        llvm_join_set_error(ctx, node,
            "LLVM parallel join requires registered runtime functions (spawn/await/%s); sequential fallback is disabled",
            get_fn_name);
        return;
    }

    /* ---------------------------------------------------------------
     * Captures (arms discipline; element binding is NOT a capture).
     * --------------------------------------------------------------- */
    JoinCapturedVar captured[PJOIN_MAX_CAPTURES];
    size_t n_captured = 0;

    for (int i = 0; i < ctx->scope_depth; i++) {
        LLVMScopeFrame *frame = &ctx->scopes[i];
        for (int j = 0; j < frame->count; j++) {
            if (!llvm_capture_entry_is_required(ctx, body, frame, j))
                continue;
            if (coll_name != NULL && frame->entries[j].name != NULL
                && strcmp(frame->entries[j].name, coll_name) == 0)
                continue; /* fan-out source is read at the call site only */
            if (n_captured >= PJOIN_MAX_CAPTURES) {
                llvm_join_set_error(ctx, node,
                    "LLVM parallel join capture registry overflow%s", "");
                return;
            }
            if (frame->entries[j].alloca == NULL) {
                llvm_join_set_error(ctx, node,
                    "LLVM parallel join capture requires storage-backed binding '%s'",
                    frame->entries[j].name);
                return;
            }
            /* Fact-admitted arrays cross the boundary: R1
             * index-disjointness rows and R5 snapshot-read rows. Every
             * other collection keeps the shared-mutable reject. The
             * element metadata is stashed by value for the wrapper-side
             * registry re-bind. */
            LLVMArrayVarEntry *arr_entry =
                llvm_lookup_array_var(ctx, frame->entries[j].name);
            bool index_admitted = arr_entry != NULL
                && ast_parallel_join_index_array_admitted(node,
                       frame->entries[j].name);
            bool readonly_admitted = arr_entry != NULL
                && ast_parallel_join_readonly_array_admitted(node,
                       frame->entries[j].name);
            bool array_admitted = index_admitted || readonly_admitted;
            if (!array_admitted
                && llvm_capture_reject_shared_collection(ctx, node,
                    "parallel join", frame->entries[j].name, true)) {
                return;
            }
            captured[n_captured] = (JoinCapturedVar){
                frame->entries[j].name,
                frame->entries[j].alloca,
                frame->entries[j].type,
                llvm_lookup_channel_inner(ctx, frame->entries[j].name),
                llvm_lookup_future_inner(ctx, frame->entries[j].name),
                llvm_lookup_slot_inner(ctx, frame->entries[j].name),
                llvm_lookup_future_is_remote(ctx, frame->entries[j].name),
                llvm_lookup_slot_is_secure(ctx, frame->entries[j].name),
                array_admitted,
                array_admitted ? arr_entry->elem_type : NULL,
                array_admitted ? arr_entry->elem_name : NULL
            };
            if (captured[n_captured].slot_inner != NULL) {
                /* Parity with the C emitter's rung-0 edge. */
                llvm_join_set_error(ctx, node,
                    "parallel join rung 0 does not carry slot captures yet ('%s'); a later rung will (docs/181 SS1.4)",
                    frame->entries[j].name);
                return;
            }
            n_captured++;
        }
    }

    /* ---------------------------------------------------------------
     * Per-element context struct: [captures as i8*..., element value],
     * plus a give slot (R2) or the shared any-join cell pointers (R3).
     * --------------------------------------------------------------- */
    bool is_any = ast_parallel_join_is_any(node);
    size_t n_fields = n_captured + 1
        + (expr_mode ? (is_any ? 2 : 1) : 0);
    LLVMTypeRef ctx_fields[PJOIN_MAX_CAPTURES + 3];
    for (size_t i = 0; i < n_captured; i++)
        ctx_fields[i] = ctx->type_i8ptr;
    ctx_fields[n_captured] = elem_type;
    if (expr_mode && is_any) {
        ctx_fields[n_captured + 1] = ctx->type_i8ptr; /* i32* state  */
        ctx_fields[n_captured + 2] = ctx->type_i8ptr; /* give T* res */
    } else if (expr_mode) {
        ctx_fields[n_captured + 1] = give_type;
    }

    char ctx_name[64];
    if (!llvm_parallel_counter_name(ctx, ctx_name, sizeof(ctx_name),
            "_pgy_pjoin_ctx_", ctx->parallel_counter))
        return;
    LLVMTypeRef ctx_struct_type = LLVMStructCreateNamed(ctx->context, ctx_name);
    LLVMStructSetBody(ctx_struct_type, ctx_fields, (unsigned)n_fields, 0);

    /* ---------------------------------------------------------------
     * The one replicated wrapper.
     * --------------------------------------------------------------- */
    LLVMValueRef saved_fn = ctx->current_function;
    LLVMTypeRef saved_ret = ctx->current_ret_type;
    LLVMTypeRef saved_function_ret = ctx->current_function_ret_type;
    const char *saved_return_type_name = ctx->current_return_type_name;
    ASTNode *saved_return_callable_type = ctx->current_return_callable_type;
    LLVMBasicBlockRef saved_bb = LLVMGetInsertBlock(ctx->builder);

    LLVMTypeRef wrapper_params[] = { ctx->type_i8ptr };
    LLVMTypeRef wrapper_type = LLVMFunctionType(ctx->type_i8ptr,
                                                wrapper_params, 1, 0);
    char fn_name[64];
    if (!llvm_parallel_counter_name(ctx, fn_name, sizeof(fn_name),
            "_pgy_pjoin_", ctx->parallel_counter))
        return;
    LLVMValueRef wrapper_fn = LLVMAddFunction(ctx->module, fn_name,
                                              wrapper_type);
    {
        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(
            ctx->context, wrapper_fn, "entry");
        LLVMPositionBuilderAtEnd(ctx->builder, entry);

        ctx->current_function = wrapper_fn;
        ctx->current_ret_type = ctx->type_i8ptr;
        ctx->current_function_ret_type = ctx->type_i8ptr;
        ctx->current_return_type_name = NULL;
        ctx->current_return_callable_type = NULL;

        LLVMLexicalRegistrySnapshot lexical_snapshot =
            llvm_lexical_registry_snapshot(ctx);
        llvm_scope_push(ctx);

        LLVMValueRef arg0 = LLVMGetParam(wrapper_fn, 0);
        LLVMValueRef ctx_ptr = LLVMBuildBitCast(ctx->builder, arg0,
            LLVMPointerType(ctx_struct_type, 0), "_pctx");

        for (size_t c = 0; c < n_captured; c++) {
            LLVMValueRef field_ptr = LLVMBuildStructGEP2(
                ctx->builder, ctx_struct_type, ctx_ptr, (unsigned)c,
                llvm_tmp_name(ctx));
            LLVMValueRef var_ptr = LLVMBuildLoad2(
                ctx->builder, ctx->type_i8ptr, field_ptr,
                llvm_tmp_name(ctx));
            llvm_scope_declare(ctx, captured[c].name, var_ptr,
                               captured[c].type);
            if (captured[c].channel_inner != NULL)
                llvm_register_channel_var_binding(ctx, captured[c].name,
                    var_ptr, captured[c].channel_inner);
            if (captured[c].future_inner != NULL)
                llvm_register_future_var_binding(ctx, captured[c].name,
                    var_ptr, captured[c].future_inner,
                    captured[c].future_is_remote);
            if (captured[c].is_admitted_array)
                /* The array registry is keyed by the scope alloca, which
                 * is the loaded pointer inside the wrapper -- re-register
                 * so indexed get/set resolve element metadata. */
                llvm_register_array_var_binding(ctx, captured[c].name,
                    var_ptr, captured[c].array_elem_type,
                    captured[c].array_elem_name, -1);
        }

        /* The element binding: a wrapper-local fed from the context. */
        {
            LLVMValueRef elem_gep = LLVMBuildStructGEP2(
                ctx->builder, ctx_struct_type, ctx_ptr,
                (unsigned)n_captured, llvm_tmp_name(ctx));
            LLVMValueRef elem_val = LLVMBuildLoad2(
                ctx->builder, elem_type, elem_gep,
                llvm_tmp_name(ctx));
            LLVMValueRef elem_local = LLVMBuildAlloca(
                ctx->builder, elem_type, elem_name);
            LLVMBuildStore(ctx->builder, elem_val, elem_local);
            llvm_scope_declare(ctx, elem_name, elem_local, elem_type);
        }

        {
            LLVMValueRef saved_give_ptr = ctx->pjoin_give_ptr;
            LLVMTypeRef saved_give_type = ctx->pjoin_give_type;
            LLVMValueRef saved_any_state = ctx->pjoin_any_state_ptr;
            LLVMValueRef saved_any_res = ctx->pjoin_any_res_ptr;

            if (expr_mode && is_any) {
                /* R3: the shared decision/result cells travel as i8*
                 * fields; give redirects through them. The entry safe
                 * point (docs/181 SS2.4) retires a task that starts
                 * after the decision. */
                LLVMValueRef st_gep = LLVMBuildStructGEP2(
                    ctx->builder, ctx_struct_type, ctx_ptr,
                    (unsigned)(n_captured + 1), llvm_tmp_name(ctx));
                LLVMValueRef st_raw = LLVMBuildLoad2(ctx->builder,
                    ctx->type_i8ptr, st_gep, llvm_tmp_name(ctx));
                LLVMValueRef st_ptr = LLVMBuildBitCast(ctx->builder,
                    st_raw, LLVMPointerType(ctx->type_i32, 0),
                    llvm_tmp_name(ctx));
                LLVMValueRef rs_gep = LLVMBuildStructGEP2(
                    ctx->builder, ctx_struct_type, ctx_ptr,
                    (unsigned)(n_captured + 2), llvm_tmp_name(ctx));
                LLVMValueRef rs_raw = LLVMBuildLoad2(ctx->builder,
                    ctx->type_i8ptr, rs_gep, llvm_tmp_name(ctx));
                LLVMValueRef rs_ptr = LLVMBuildBitCast(ctx->builder,
                    rs_raw, LLVMPointerType(give_type, 0),
                    llvm_tmp_name(ctx));

                LLVMValueRef st_val = LLVMBuildLoad2(ctx->builder,
                    ctx->type_i32, st_ptr, llvm_tmp_name(ctx));
                LLVMSetOrdering(st_val, LLVMAtomicOrderingAcquire);
                LLVMValueRef decided = LLVMBuildICmp(ctx->builder,
                    LLVMIntNE, st_val, LLVMConstInt(ctx->type_i32, 0, 0),
                    llvm_tmp_name(ctx));
                LLVMBasicBlockRef retire_bb =
                    LLVMAppendBasicBlockInContext(ctx->context,
                        wrapper_fn, "pj.any.retire");
                LLVMBasicBlockRef body_bb =
                    LLVMAppendBasicBlockInContext(ctx->context,
                        wrapper_fn, "pj.any.body");
                LLVMBuildCondBr(ctx->builder, decided, retire_bb, body_bb);
                LLVMPositionBuilderAtEnd(ctx->builder, retire_bb);
                LLVMBuildRet(ctx->builder, LLVMConstNull(ctx->type_i8ptr));
                LLVMPositionBuilderAtEnd(ctx->builder, body_bb);

                ctx->pjoin_any_state_ptr = st_ptr;
                ctx->pjoin_any_res_ptr = rs_ptr;
                ctx->pjoin_give_ptr = NULL;
                ctx->pjoin_give_type = give_type;
            } else if (expr_mode) {
                ctx->pjoin_give_ptr = LLVMBuildStructGEP2(
                    ctx->builder, ctx_struct_type, ctx_ptr,
                    (unsigned)(n_captured + 1), "_pj_give");
                ctx->pjoin_give_type = give_type;
                ctx->pjoin_any_state_ptr = NULL;
                ctx->pjoin_any_res_ptr = NULL;
            } else {
                ctx->pjoin_give_ptr = NULL;
                ctx->pjoin_give_type = NULL;
                ctx->pjoin_any_state_ptr = NULL;
                ctx->pjoin_any_res_ptr = NULL;
            }
            llvm_emit_statement(body, ctx);
            ctx->pjoin_give_ptr = saved_give_ptr;
            ctx->pjoin_give_type = saved_give_type;
            ctx->pjoin_any_state_ptr = saved_any_state;
            ctx->pjoin_any_res_ptr = saved_any_res;
        }

        if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder))
                == NULL)
            LLVMBuildRet(ctx->builder, LLVMConstNull(ctx->type_i8ptr));

        llvm_scope_pop(ctx);
        llvm_lexical_registry_restore(ctx, lexical_snapshot);
    }

    ctx->parallel_counter++;
    ctx->current_function = saved_fn;
    ctx->current_ret_type = saved_ret;
    ctx->current_function_ret_type = saved_function_ret;
    ctx->current_return_type_name = saved_return_type_name;
    ctx->current_return_callable_type = saved_return_callable_type;
    if (saved_bb != NULL)
        LLVMPositionBuilderAtEnd(ctx->builder, saved_bb);
    if (ctx->has_error)
        return;

    /* R5 alias fail-close (docs/181): a snapshot-read capture whose
     * backing IS an index-written array would let free-index reads race
     * the per-index writes (`let b = a;` copies the handle, not the
     * buffer). One pointer compare per (written, read) pair at fan-out
     * entry; the C twin emits the same check into the same panic export
     * pair, so class and reason match across backends. */
    for (size_t wi = 0; wi < n_captured; wi++) {
        if (!captured[wi].is_admitted_array
            || !ast_parallel_join_index_array_admitted(node,
                   captured[wi].name))
            continue;
        for (size_t ri = 0; ri < n_captured; ri++) {
            if (!captured[ri].is_admitted_array
                || !ast_parallel_join_readonly_array_admitted(node,
                       captured[ri].name))
                continue;
            LLVMFuncEntry *panic_fn = llvm_lookup_function(ctx,
                "pgy_runtime_panic_authority_mismatch_export");
            if (panic_fn == NULL) {
                llvm_join_set_error(ctx, node,
                    "parallel join alias check requires the registered panic runtime%s",
                    "");
                return;
            }
            LLVMTypeRef w_data_ty = LLVMStructGetTypeAtIndex(
                captured[wi].type, 0);
            LLVMTypeRef r_data_ty = LLVMStructGetTypeAtIndex(
                captured[ri].type, 0);
            LLVMValueRef w_gep = LLVMBuildStructGEP2(ctx->builder,
                captured[wi].type, captured[wi].alloca, 0,
                llvm_tmp_name(ctx));
            LLVMValueRef w_data = LLVMBuildLoad2(ctx->builder, w_data_ty,
                w_gep, llvm_tmp_name(ctx));
            LLVMValueRef r_gep = LLVMBuildStructGEP2(ctx->builder,
                captured[ri].type, captured[ri].alloca, 0,
                llvm_tmp_name(ctx));
            LLVMValueRef r_data = LLVMBuildLoad2(ctx->builder, r_data_ty,
                r_gep, llvm_tmp_name(ctx));
            LLVMValueRef w8 = LLVMBuildBitCast(ctx->builder, w_data,
                ctx->type_i8ptr, llvm_tmp_name(ctx));
            LLVMValueRef r8 = LLVMBuildBitCast(ctx->builder, r_data,
                ctx->type_i8ptr, llvm_tmp_name(ctx));
            LLVMValueRef same = LLVMBuildICmp(ctx->builder, LLVMIntEQ,
                w8, r8, llvm_tmp_name(ctx));
            LLVMValueRef nonnull = LLVMBuildICmp(ctx->builder, LLVMIntNE,
                w8, LLVMConstPointerNull(ctx->type_i8ptr),
                llvm_tmp_name(ctx));
            LLVMValueRef aliased = LLVMBuildAnd(ctx->builder, same,
                nonnull, llvm_tmp_name(ctx));
            LLVMBasicBlockRef alias_bb = LLVMAppendBasicBlockInContext(
                ctx->context, ctx->current_function, "pj.alias.panic");
            LLVMBasicBlockRef cont_bb = LLVMAppendBasicBlockInContext(
                ctx->context, ctx->current_function, "pj.alias.cont");
            LLVMBuildCondBr(ctx->builder, aliased, alias_bb, cont_bb);
            LLVMPositionBuilderAtEnd(ctx->builder, alias_bb);
            {
                LLVMValueRef panic_args[] = {
                    LLVMBuildGlobalStringPtr(ctx->builder,
                        "parallel join read-only capture aliases an index-written array",
                        llvm_tmp_name(ctx))
                };
                LLVMBuildCall2(ctx->builder, panic_fn->fn_type,
                    panic_fn->fn, panic_args, 1, "");
                LLVMBuildUnreachable(ctx->builder);
            }
            LLVMPositionBuilderAtEnd(ctx->builder, cont_bb);
        }
    }

    /* ---------------------------------------------------------------
     * Call site: fan-out length, fan-out loop, all-join loop.
     * --------------------------------------------------------------- */
    LLVMTypeRef handle_type = LLVMGetReturnType(spawn_fn->fn_type);
    LLVMValueRef one = LLVMConstInt(ctx->type_i64, 1, 0);
    LLVMValueRef zero = LLVMConstInt(ctx->type_i64, 0, 0);

    LLVMValueRef n_val;
    LLVMValueRef range_lo64 = NULL;
    if (index_mode) {
        /* n = hi > lo ? hi - lo : 0, in i64 (endpoints are Int/i32). */
        LLVMValueRef lo = llvm_emit_expression(coll, ctx);
        LLVMValueRef hi = lo != NULL
            ? llvm_emit_expression(range_end, ctx)
            : NULL;
        if (lo == NULL || hi == NULL) {
            llvm_join_set_error(ctx, node,
                "LLVM parallel join could not lower a range endpoint expression%s",
                "");
            return;
        }
        if (LLVMTypeOf(lo) != ctx->type_i64)
            lo = LLVMBuildSExt(ctx->builder, lo, ctx->type_i64,
                               llvm_tmp_name(ctx));
        if (LLVMTypeOf(hi) != ctx->type_i64)
            hi = LLVMBuildSExt(ctx->builder, hi, ctx->type_i64,
                               llvm_tmp_name(ctx));
        range_lo64 = lo;
        LLVMValueRef diff = LLVMBuildSub(ctx->builder, hi, lo,
                                         llvm_tmp_name(ctx));
        LLVMValueRef nonempty = LLVMBuildICmp(ctx->builder, LLVMIntSGT,
            hi, lo, llvm_tmp_name(ctx));
        n_val = LLVMBuildSelect(ctx->builder, nonempty, diff, zero,
                                llvm_tmp_name(ctx));
    } else {
        LLVMValueRef aggregate = LLVMBuildLoad2(ctx->builder, coll_var.type,
            coll_var.alloca, llvm_tmp_name(ctx));
        n_val = llvm_array_length_i64(ctx, aggregate);
        if (n_val == NULL) {
            llvm_join_set_error(ctx, node,
                "LLVM parallel join could not read the collection length%s",
                "");
            return;
        }
    }
    LLVMValueRef n_is_zero = LLVMBuildICmp(ctx->builder, LLVMIntEQ, n_val,
        zero, llvm_tmp_name(ctx));
    LLVMValueRef n_or_one = LLVMBuildSelect(ctx->builder, n_is_zero, one,
        n_val, llvm_tmp_name(ctx));

    LLVMValueRef ctxs = LLVMBuildArrayAlloca(ctx->builder, ctx_struct_type,
        n_or_one, "_pj_ctxs");
    LLVMValueRef handles = LLVMBuildArrayAlloca(ctx->builder, handle_type,
        n_or_one, "_pj_hs");
    LLVMValueRef i_slot = LLVMBuildAlloca(ctx->builder, ctx->type_i64,
        "_pj_i");
    LLVMBuildStore(ctx->builder, zero, i_slot);

    /* R3 shared cells: decision (i32, 0 = undecided) and winner result
     * (zero-initialized only to quiet uninitialized-value passes; it is
     * read only after a decision, and no decision panics). */
    LLVMValueRef any_state = NULL;
    LLVMValueRef any_res = NULL;
    if (expr_mode && is_any) {
        any_state = LLVMBuildAlloca(ctx->builder, ctx->type_i32,
            "_pj_any");
        LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i32, 0, 0),
            any_state);
        any_res = LLVMBuildAlloca(ctx->builder, give_type, "_pj_any_res");
        LLVMBuildStore(ctx->builder, LLVMConstNull(give_type), any_res);
    }

    LLVMBasicBlockRef spawn_cond = LLVMAppendBasicBlockInContext(
        ctx->context, ctx->current_function, "pj.spawn.cond");
    LLVMBasicBlockRef spawn_body = LLVMAppendBasicBlockInContext(
        ctx->context, ctx->current_function, "pj.spawn.body");
    LLVMBasicBlockRef spawn_done = LLVMAppendBasicBlockInContext(
        ctx->context, ctx->current_function, "pj.spawn.done");
    LLVMBuildBr(ctx->builder, spawn_cond);

    LLVMPositionBuilderAtEnd(ctx->builder, spawn_cond);
    {
        LLVMValueRef i_val = LLVMBuildLoad2(ctx->builder, ctx->type_i64,
            i_slot, llvm_tmp_name(ctx));
        LLVMValueRef cont = LLVMBuildICmp(ctx->builder, LLVMIntULT, i_val,
            n_val, llvm_tmp_name(ctx));
        LLVMBuildCondBr(ctx->builder, cont, spawn_body, spawn_done);
    }

    LLVMPositionBuilderAtEnd(ctx->builder, spawn_body);
    {
        LLVMValueRef i_val = LLVMBuildLoad2(ctx->builder, ctx->type_i64,
            i_slot, llvm_tmp_name(ctx));
        LLVMValueRef ctx_i = LLVMBuildGEP2(ctx->builder, ctx_struct_type,
            ctxs, &i_val, 1, llvm_tmp_name(ctx));
        for (size_t c = 0; c < n_captured; c++) {
            LLVMValueRef field_ptr = LLVMBuildStructGEP2(ctx->builder,
                ctx_struct_type, ctx_i, (unsigned)c, llvm_tmp_name(ctx));
            LLVMValueRef addr = LLVMBuildBitCast(ctx->builder,
                captured[c].alloca, ctx->type_i8ptr, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, addr, field_ptr);
        }
        {
            LLVMValueRef elem_val;
            if (index_mode) {
                /* Task index = lo + i, truncated back to the Int/i32
                 * binding width (index mode pins elem_type to i32). */
                LLVMValueRef idx64 = LLVMBuildAdd(ctx->builder, range_lo64,
                    i_val, llvm_tmp_name(ctx));
                elem_val = LLVMBuildTrunc(ctx->builder, idx64, elem_type,
                    llvm_tmp_name(ctx));
            } else {
                LLVMValueRef get_args[] = { coll_var.alloca, i_val };
                elem_val = LLVMBuildCall2(ctx->builder,
                    get_fn->fn_type, get_fn->fn, get_args, 2,
                    llvm_tmp_name(ctx));
            }
            LLVMValueRef elem_ptr = LLVMBuildStructGEP2(ctx->builder,
                ctx_struct_type, ctx_i, (unsigned)n_captured,
                llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, elem_val, elem_ptr);
        }
        if (expr_mode && is_any) {
            LLVMValueRef st_field = LLVMBuildStructGEP2(ctx->builder,
                ctx_struct_type, ctx_i, (unsigned)(n_captured + 1),
                llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder,
                LLVMBuildBitCast(ctx->builder, any_state, ctx->type_i8ptr,
                    llvm_tmp_name(ctx)), st_field);
            LLVMValueRef rs_field = LLVMBuildStructGEP2(ctx->builder,
                ctx_struct_type, ctx_i, (unsigned)(n_captured + 2),
                llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder,
                LLVMBuildBitCast(ctx->builder, any_res, ctx->type_i8ptr,
                    llvm_tmp_name(ctx)), rs_field);
        }
        {
            LLVMValueRef fn_ptr = LLVMBuildBitCast(ctx->builder, wrapper_fn,
                ctx->type_i8ptr, llvm_tmp_name(ctx));
            LLVMValueRef ctx_i8 = LLVMBuildBitCast(ctx->builder, ctx_i,
                ctx->type_i8ptr, llvm_tmp_name(ctx));
            LLVMValueRef spawn_args[] = {
                LLVMConstInt(ctx->type_i32, PGY_LANE_WORKER_POOL, 0),
                fn_ptr,
                ctx_i8
            };
            LLVMValueRef handle = LLVMBuildCall2(ctx->builder,
                spawn_fn->fn_type, spawn_fn->fn, spawn_args, 3,
                llvm_tmp_name(ctx));
            if (!llvm_emit_task_handle_nonnull_guard(ctx, node, handle,
                    "LLVM parallel join task spawn failed"))
                return;
            LLVMValueRef h_ptr = LLVMBuildGEP2(ctx->builder, handle_type,
                handles, &i_val, 1, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, handle, h_ptr);
        }
        {
            LLVMValueRef i_val2 = LLVMBuildLoad2(ctx->builder,
                ctx->type_i64, i_slot, llvm_tmp_name(ctx));
            LLVMValueRef next = LLVMBuildAdd(ctx->builder, i_val2, one,
                llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, next, i_slot);
            LLVMBuildBr(ctx->builder, spawn_cond);
        }
    }

    LLVMPositionBuilderAtEnd(ctx->builder, spawn_done);
    LLVMBuildStore(ctx->builder, zero, i_slot);

    LLVMBasicBlockRef join_cond = LLVMAppendBasicBlockInContext(
        ctx->context, ctx->current_function, "pj.join.cond");
    LLVMBasicBlockRef join_body = LLVMAppendBasicBlockInContext(
        ctx->context, ctx->current_function, "pj.join.body");
    LLVMBasicBlockRef join_done = LLVMAppendBasicBlockInContext(
        ctx->context, ctx->current_function, "pj.join.done");
    LLVMBuildBr(ctx->builder, join_cond);

    LLVMPositionBuilderAtEnd(ctx->builder, join_cond);
    {
        LLVMValueRef i_val = LLVMBuildLoad2(ctx->builder, ctx->type_i64,
            i_slot, llvm_tmp_name(ctx));
        LLVMValueRef cont = LLVMBuildICmp(ctx->builder, LLVMIntULT, i_val,
            n_val, llvm_tmp_name(ctx));
        LLVMBuildCondBr(ctx->builder, cont, join_body, join_done);
    }

    LLVMPositionBuilderAtEnd(ctx->builder, join_body);
    {
        LLVMValueRef i_val = LLVMBuildLoad2(ctx->builder, ctx->type_i64,
            i_slot, llvm_tmp_name(ctx));
        LLVMValueRef h_ptr = LLVMBuildGEP2(ctx->builder, handle_type,
            handles, &i_val, 1, llvm_tmp_name(ctx));
        LLVMValueRef handle = LLVMBuildLoad2(ctx->builder, handle_type,
            h_ptr, llvm_tmp_name(ctx));
        if (expr_mode && is_any) {
            /* R3 cancel hint: once decided, still-queued tasks are
             * skipped by the pool's pre-run safe point. Every handle is
             * still awaited exactly once (context lifetimes stay
             * join-bounded); first-give-wins is decided by the CAS,
             * never by this index-order await walk. */
            LLVMFuncEntry *cancel_fn = llvm_lookup_function(ctx,
                "pgy_task_cancel_export");
            if (cancel_fn == NULL) {
                llvm_join_set_error(ctx, node,
                    "LLVM any-join requires the registered task-cancel runtime%s",
                    "");
                return;
            }
            LLVMValueRef st_val = LLVMBuildLoad2(ctx->builder,
                ctx->type_i32, any_state, llvm_tmp_name(ctx));
            LLVMSetOrdering(st_val, LLVMAtomicOrderingAcquire);
            LLVMValueRef decided = LLVMBuildICmp(ctx->builder, LLVMIntNE,
                st_val, LLVMConstInt(ctx->type_i32, 0, 0),
                llvm_tmp_name(ctx));
            LLVMBasicBlockRef hint_bb = LLVMAppendBasicBlockInContext(
                ctx->context, ctx->current_function, "pj.any.hint");
            LLVMBasicBlockRef await_bb = LLVMAppendBasicBlockInContext(
                ctx->context, ctx->current_function, "pj.any.await");
            LLVMBuildCondBr(ctx->builder, decided, hint_bb, await_bb);
            LLVMPositionBuilderAtEnd(ctx->builder, hint_bb);
            {
                LLVMValueRef cancel_args[] = { handle };
                LLVMBuildCall2(ctx->builder, cancel_fn->fn_type,
                    cancel_fn->fn, cancel_args, 1, "");
                LLVMBuildBr(ctx->builder, await_bb);
            }
            LLVMPositionBuilderAtEnd(ctx->builder, await_bb);
        }
        LLVMValueRef await_args[] = { handle };
        LLVMBuildCall2(ctx->builder, await_fn->fn_type, await_fn->fn,
            await_args, 1, "");
        LLVMValueRef next = LLVMBuildAdd(ctx->builder, i_val, one,
            llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, next, i_slot);
        LLVMBuildBr(ctx->builder, join_cond);
    }

    LLVMPositionBuilderAtEnd(ctx->builder, join_done);

    /* Expression mode: materialize the result from the per-task give
     * slots (llvm_stmt_parallel_join_result.c) -- Array<R> for the
     * all-join, one folded scalar for the R4 reduce combinators, the
     * winner's value for the R3 any-join. */
    if (expr_mode && is_any) {
        LLVMFuncEntry *panic_fn = llvm_lookup_function(ctx,
            "pgy_runtime_panic_out_of_bounds_export");
        if (panic_fn == NULL) {
            llvm_join_set_error(ctx, node,
                "LLVM any-join requires the registered panic runtime%s",
                "");
            return;
        }
        LLVMValueRef st_val = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
            any_state, llvm_tmp_name(ctx));
        LLVMSetOrdering(st_val, LLVMAtomicOrderingAcquire);
        LLVMValueRef undecided = LLVMBuildICmp(ctx->builder, LLVMIntEQ,
            st_val, LLVMConstInt(ctx->type_i32, 0, 0),
            llvm_tmp_name(ctx));
        LLVMBasicBlockRef empty_bb = LLVMAppendBasicBlockInContext(
            ctx->context, ctx->current_function, "pj.any.empty");
        LLVMBasicBlockRef done_bb = LLVMAppendBasicBlockInContext(
            ctx->context, ctx->current_function, "pj.any.done");
        LLVMBuildCondBr(ctx->builder, undecided, empty_bb, done_bb);
        LLVMPositionBuilderAtEnd(ctx->builder, empty_bb);
        {
            /* "First of nothing" fails closed, same rationale as empty
             * min/max (docs/181 R4); shared reason with the C twin. */
            LLVMValueRef panic_args[] = {
                LLVMBuildGlobalStringPtr(ctx->builder,
                    "any-join over an empty parallel fan-out",
                    llvm_tmp_name(ctx))
            };
            LLVMBuildCall2(ctx->builder, panic_fn->fn_type, panic_fn->fn,
                panic_args, 1, "");
            LLVMBuildUnreachable(ctx->builder);
        }
        LLVMPositionBuilderAtEnd(ctx->builder, done_bb);
        *result_out = LLVMBuildLoad2(ctx->builder, give_type, any_res,
            "_pj_any_val");
    } else if (expr_mode) {
        if (ast_parallel_join_reduce_op(node) != NULL)
            llvm_pjoin_materialize_reduce(ctx, node, give_name, give_type,
                ctx_struct_type, ctxs, n_captured, n_val, i_slot,
                result_out);
        else
            llvm_pjoin_materialize_result(ctx, node, give_name, give_type,
                ctx_struct_type, ctxs, n_captured, n_val, i_slot,
                result_out);
    }
}

void
llvm_emit_parallel_join_block(ASTNode *node, LLVMGenCtx *ctx)
{
    llvm_emit_parallel_join_common(node, ctx, NULL);
}

LLVMValueRef
llvm_emit_parallel_join_expr(ASTNode *node, LLVMGenCtx *ctx)
{
    LLVMValueRef result = NULL;

    llvm_emit_parallel_join_common(node, ctx, &result);
    return result;
}

#endif /* PGY_LLVM_ENABLED */
