/*
 * Copyright (c) 2026 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM MIR for-in lowering. MIR owns the loop index, condition, body binding,
 * and backedge increment; this file keeps iterable loop details out of the
 * generic CFG control owner.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"
#include "parser/ast_api.h"

#include <stdlib.h>
#include <string.h>

static void
llvm_mir_for_in_index_name(const MIRInstruction *inst,
                           const char *variable,
                           char *buf,
                           size_t buf_size)
{
    uint32_t stable_id;

    if (buf == NULL || buf_size == 0)
        return;
    stable_id = mir_instruction_source_stable_id(inst);
    if (stable_id == 0) {
        snprintf(buf, buf_size, "__pgy_idx_%s",
                 variable != NULL ? variable : "it");
        return;
    }
    snprintf(buf, buf_size, "__pgy_idx_%s_%u",
             variable != NULL ? variable : "it", stable_id);
}

static void
llvm_mir_for_in_binding_name(const MIRInstruction *inst,
                             const char *variable,
                             char *buf,
                             size_t buf_size)
{
    uint32_t stable_id;

    if (buf == NULL || buf_size == 0)
        return;
    stable_id = mir_instruction_source_stable_id(inst);
    if (stable_id == 0) {
        snprintf(buf, buf_size, "%s.mir.forin",
                 variable != NULL ? variable : "it");
        return;
    }
    snprintf(buf, buf_size, "%s.mir.forin.%u",
             variable != NULL ? variable : "it", stable_id);
}

static LLVMFuncEntry *
llvm_mir_for_in_required_runtime(LLVMGenCtx *ctx,
                                 const MIRInstruction *inst,
                                 const char *function_name)
{
    LLVMFuncEntry *fn = function_name != NULL
        ? llvm_lookup_function(ctx, function_name)
        : NULL;

    if (fn == NULL && ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, mir_instruction_source_payload(inst),
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM MIR for-in lowering requires registered runtime function '%s'",
            function_name != NULL ? function_name : "<missing>");
    }
    return fn;
}

static void
llvm_mir_for_in_set_error(LLVMGenCtx *ctx,
                          const MIRInstruction *inst,
                          const char *message)
{
    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, mir_instruction_source_payload(inst),
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "%s", message != NULL ? message : "LLVM MIR for-in lowering failed");
    }
}

static LLVMTypeRef
llvm_mir_for_in_scope_collection_elem_type(LLVMGenCtx *ctx, const char *name)
{
    LLVMVarEntry var;
    const char *struct_name;
    const char *suffix = NULL;

    if (ctx == NULL || name == NULL)
        return NULL;
    if (!llvm_scope_lookup_snapshot(ctx, name, &var) || var.type == NULL)
        return NULL;
    if (LLVMGetTypeKind(var.type) != LLVMStructTypeKind)
        return NULL;

    struct_name = LLVMGetStructName(var.type);
    if (struct_name == NULL)
        return NULL;
    if (strncmp(struct_name, "PgyArray_", 9) == 0) {
        suffix = struct_name + 9;
    } else if (strncmp(struct_name, "PgySlice_", 9) == 0) {
        suffix = struct_name + 9;
    }
    if (suffix == NULL || suffix[0] == '\0'
        || strcmp(suffix, "Unknown") == 0) {
        return NULL;
    }
    return pergyra_type_to_llvm(ctx, suffix);
}

bool
llvm_mir_emit_for_in_loop_init(const MIRInstruction *inst, LLVMGenCtx *ctx)
{
    const char *variable;
    LLVMValueRef idx_alloca;
    char idx_name[256];

    if (inst == NULL || ctx == NULL)
        return true;
    if (inst->branch_shape != MIR_BRANCH_FOR_IN) {
        return true;
    }
    variable = inst->arg0;
    if (variable == NULL)
        return true;

    llvm_mir_for_in_index_name(inst, variable, idx_name, sizeof(idx_name));
    if (llvm_scope_contains(ctx, idx_name))
        return true;
    idx_alloca = llvm_create_entry_alloca(ctx, ctx->type_i32, idx_name);
    LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i32, 0, 0), idx_alloca);
    llvm_scope_declare(ctx, pergyra_strdup(idx_name), idx_alloca, ctx->type_i32);
    return true;
}

LLVMValueRef
llvm_mir_emit_for_in_loop_condition(const MIRInstruction *inst, LLVMGenCtx *ctx)
{
    ASTNode *iterable;
    const char *variable;
    LLVMVarEntry idx_snapshot;
    LLVMVarEntry iter_snapshot;
    LLVMValueRef current;
    LLVMValueRef size_call;
    char idx_name[256];

    if (inst == NULL || ctx == NULL)
        return NULL;
    if (inst->branch_shape != MIR_BRANCH_FOR_IN) {
        return NULL;
    }
    variable = inst->arg0;
    if (variable == NULL)
        return NULL;

    llvm_mir_for_in_index_name(inst, variable, idx_name, sizeof(idx_name));
    if (!llvm_scope_lookup_snapshot(ctx, idx_name, &idx_snapshot)
        || idx_snapshot.alloca == NULL) {
        LLVMValueRef idx_alloca = llvm_create_entry_alloca(ctx, ctx->type_i32,
                                                           idx_name);
        LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i32, 0, 0),
                       idx_alloca);
        llvm_scope_declare(ctx, pergyra_strdup(idx_name), idx_alloca,
                           ctx->type_i32);
        (void)llvm_scope_lookup_snapshot(ctx, idx_name, &idx_snapshot);
    }
    if (idx_snapshot.alloca == NULL)
        return NULL;

    iterable = inst->expr0;
    if (iterable == NULL || iterable->type != AST_IDENTIFIER) {
        llvm_mir_for_in_set_error(ctx, inst,
            "LLVM MIR for-in lowering requires identifier iterable metadata");
        return NULL;
    }

    const char *iter_name = ast_identifier_name(iterable);
    LLVMArrayVarEntry *array_entry = llvm_lookup_array_var(ctx, iter_name);
    LLVMTypeRef scope_elem =
        llvm_mir_for_in_scope_collection_elem_type(ctx, iter_name);
    if (llvm_scope_lookup_snapshot(ctx, iter_name, &iter_snapshot)
        && (array_entry != NULL || scope_elem != NULL)) {
        LLVMValueRef aggregate = LLVMBuildLoad2(ctx->builder,
            iter_snapshot.type, iter_snapshot.alloca, llvm_tmp_name(ctx));
        LLVMValueRef length64 = llvm_array_length_i64(ctx, aggregate);
        current = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
                                 idx_snapshot.alloca, llvm_tmp_name(ctx));
        current = LLVMBuildSExt(ctx->builder, current, ctx->type_i64,
                                llvm_tmp_name(ctx));
        return LLVMBuildICmp(ctx->builder, LLVMIntSLT, current, length64,
                             llvm_tmp_name(ctx));
    }
    if (iter_snapshot.alloca != NULL
        && llvm_lookup_list_inner(ctx, iter_name) != NULL) {
        LLVMFuncEntry *size_fn = llvm_mir_for_in_required_runtime(ctx, inst,
            "pgy_list_size_raw_export");
        if (size_fn == NULL)
            return NULL;
        LLVMValueRef args[] = {
            LLVMBuildBitCast(ctx->builder, iter_snapshot.alloca,
                             ctx->type_i8ptr, llvm_tmp_name(ctx))
        };
        size_call = LLVMBuildCall2(ctx->builder, size_fn->fn_type,
                                   size_fn->fn, args, 1, llvm_tmp_name(ctx));
        current = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
                                 idx_snapshot.alloca, llvm_tmp_name(ctx));
        return LLVMBuildICmp(ctx->builder, LLVMIntSLT, current, size_call,
                             llvm_tmp_name(ctx));
    }

    llvm_mir_for_in_set_error(ctx, inst,
        "LLVM MIR for-in lowering requires Array<T>, Slice<T>, or List<T> iterable metadata");
    return NULL;
}

bool
llvm_mir_emit_for_in_loop_increment(const MIRInstruction *inst, LLVMGenCtx *ctx)
{
    const char *variable;
    LLVMVarEntry idx_snapshot;
    LLVMValueRef current;
    LLVMValueRef next;
    char idx_name[256];

    if (inst == NULL || ctx == NULL)
        return true;
    if (inst->branch_shape != MIR_BRANCH_FOR_IN) {
        return true;
    }
    variable = inst->arg0;
    if (variable == NULL)
        return true;

    llvm_mir_for_in_index_name(inst, variable, idx_name, sizeof(idx_name));
    if (!llvm_scope_lookup_snapshot(ctx, idx_name, &idx_snapshot)
        || idx_snapshot.alloca == NULL)
        return true;

    current = LLVMBuildLoad2(ctx->builder, ctx->type_i32, idx_snapshot.alloca,
                             llvm_tmp_name(ctx));
    next = LLVMBuildAdd(ctx->builder, current,
                        LLVMConstInt(ctx->type_i32, 1, 0),
                        llvm_tmp_name(ctx));
    LLVMBuildStore(ctx->builder, next, idx_snapshot.alloca);
    return true;
}

static const MIRInstruction *
llvm_mir_find_for_in_branch_inst(const MIRBasicBlock *block)
{
    if (block == NULL)
        return NULL;
    for (size_t i = 0; i < block->instruction_count; i++) {
        const MIRInstruction *inst = &block->instructions[i];
        if (inst->kind == MIR_INST_BRANCH
            && inst->branch_shape == MIR_BRANCH_FOR_IN) {
            return inst;
        }
    }
    return NULL;
}

static bool
llvm_mir_for_in_block_reaches(const MIRRoutine *routine,
                              size_t from_id,
                              size_t target_id,
                              bool *visited)
{
    const MIRBasicBlock *block;

    if (routine == NULL || visited == NULL)
        return false;
    if (from_id >= routine->block_count || target_id >= routine->block_count)
        return false;
    if (from_id == target_id)
        return true;
    if (visited[from_id])
        return false;
    visited[from_id] = true;

    block = &routine->blocks[from_id];
    if (block->has_succ_true
        && llvm_mir_for_in_block_reaches(routine, block->succ_true,
                                         target_id, visited)) {
        return true;
    }
    if (block->has_succ_false
        && llvm_mir_for_in_block_reaches(routine, block->succ_false,
                                         target_id, visited)) {
        return true;
    }
    return false;
}

static bool
llvm_mir_for_in_block_reaches_avoiding(const MIRRoutine *routine,
                                       size_t from_id,
                                       size_t target_id,
                                       size_t stop_id,
                                       bool *visited)
{
    const MIRBasicBlock *block;

    if (routine == NULL || visited == NULL)
        return false;
    if (from_id >= routine->block_count || target_id >= routine->block_count)
        return false;
    if (from_id == target_id)
        return true;
    if (from_id == stop_id)
        return false;
    if (visited[from_id])
        return false;
    visited[from_id] = true;

    block = &routine->blocks[from_id];
    if (block->has_succ_true
        && llvm_mir_for_in_block_reaches_avoiding(routine, block->succ_true,
                                                  target_id, stop_id,
                                                  visited)) {
        return true;
    }
    if (block->has_succ_false
        && llvm_mir_for_in_block_reaches_avoiding(routine, block->succ_false,
                                                  target_id, stop_id,
                                                  visited)) {
        return true;
    }
    return false;
}

static bool
llvm_mir_for_in_body_region_contains(const MIRRoutine *routine,
                                     const MIRBasicBlock *loop_block,
                                     size_t target_id)
{
    bool *visited;
    bool reaches_body;
    bool reaches_exit;

    if (routine == NULL || loop_block == NULL)
        return false;
    if (!loop_block->has_succ_true || !loop_block->has_succ_false)
        return false;
    if (target_id >= routine->block_count || target_id == loop_block->id)
        return false;
    if (llvm_mir_find_for_in_branch_inst(loop_block) == NULL)
        return false;

    visited = calloc(routine->block_count, sizeof(bool));
    if (visited == NULL)
        return false;
    reaches_body = llvm_mir_for_in_block_reaches(routine,
        loop_block->succ_true, target_id, visited);
    memset(visited, 0, routine->block_count * sizeof(bool));
    reaches_exit = llvm_mir_for_in_block_reaches_avoiding(routine,
        loop_block->succ_false, target_id, loop_block->id, visited);
    free(visited);
    return reaches_body && !reaches_exit;
}

static bool
llvm_mir_emit_for_in_binding_for_inst(const MIRInstruction *branch_inst,
                                      LLVMGenCtx *ctx)
{
    ASTNode *iterable;
    const char *variable;
    const char *iter_name;
    const char *list_inner;
    LLVMArrayVarEntry *array_entry;
    LLVMTypeRef scope_elem_ty;
    LLVMVarEntry list_snapshot;
    LLVMVarEntry loop_snapshot;
    LLVMVarEntry idx_snapshot;
    LLVMValueRef list_alloca;
    LLVMTypeRef list_type;
    LLVMValueRef loop_alloca;
    LLVMTypeRef loop_type;
    LLVMValueRef idx_alloca;
    LLVMFuncEntry *get_fn;
    LLVMTypeRef elem_ty;
    LLVMValueRef idx;
    char idx_name[256];
    char binding_name[256];

    if (branch_inst == NULL || ctx == NULL)
        return true;
    iterable = branch_inst->expr0;
    variable = branch_inst->arg0;
    if (iterable == NULL || iterable->type != AST_IDENTIFIER || variable == NULL)
        return true;

    iter_name = ast_identifier_name(iterable);
    list_inner = llvm_lookup_list_inner(ctx, iter_name);
    array_entry = llvm_lookup_array_var(ctx, iter_name);
    scope_elem_ty = llvm_mir_for_in_scope_collection_elem_type(ctx, iter_name);
    if ((list_inner == NULL && array_entry == NULL && scope_elem_ty == NULL)
        || !llvm_scope_lookup_snapshot(ctx, iter_name, &list_snapshot)) {
        return true;
    }
    list_alloca = list_snapshot.alloca;
    list_type = list_snapshot.type;
    if (list_alloca == NULL || list_type == NULL)
        return true;

    elem_ty = array_entry != NULL ? array_entry->elem_type
                                  : scope_elem_ty;
    if (elem_ty == NULL && list_inner != NULL)
        elem_ty = pergyra_type_to_llvm(ctx, list_inner);
    if (ctx->has_error || elem_ty == NULL)
        return false;
    llvm_mir_for_in_binding_name(branch_inst, variable, binding_name,
                                 sizeof(binding_name));
    if (!llvm_scope_lookup_snapshot(ctx, binding_name, &loop_snapshot)) {
        LLVMValueRef loop_alloca = llvm_create_entry_alloca(ctx, elem_ty,
                                                            binding_name);
        llvm_scope_declare(ctx, pergyra_strdup(binding_name), loop_alloca,
                           elem_ty);
        {
            LLVMClassTypeEntry *cls = llvm_stmt_lookup_class_by_type(ctx, elem_ty);
            if (cls != NULL)
                llvm_register_var_class(ctx, pergyra_strdup(variable),
                                        cls->class_name);
        }
        (void)llvm_scope_lookup_snapshot(ctx, binding_name, &loop_snapshot);
    }
    if (loop_snapshot.alloca == NULL || loop_snapshot.type == NULL)
        return true;
    loop_alloca = loop_snapshot.alloca;
    loop_type = loop_snapshot.type;
    if (loop_alloca != NULL) {
        llvm_scope_declare(ctx, pergyra_strdup(variable), loop_alloca,
                           loop_type);
    }
    llvm_mir_for_in_index_name(branch_inst, variable,
                               idx_name, sizeof(idx_name));
    if (!llvm_scope_lookup_snapshot(ctx, idx_name, &idx_snapshot)
        || idx_snapshot.alloca == NULL) {
        return true;
    }
    idx_alloca = idx_snapshot.alloca;

    idx = LLVMBuildLoad2(ctx->builder, ctx->type_i32, idx_alloca,
                         llvm_tmp_name(ctx));
    if (array_entry != NULL || scope_elem_ty != NULL) {
        LLVMValueRef aggregate = LLVMBuildLoad2(ctx->builder, list_type,
            list_alloca, llvm_tmp_name(ctx));
        LLVMValueRef data_ptr = llvm_array_data_ptr(ctx, aggregate);
        LLVMValueRef idx64 = LLVMBuildSExt(ctx->builder, idx, ctx->type_i64,
                                           llvm_tmp_name(ctx));
        /*
         * The for-in loop condition guarantees idx is in [0, length), so the
         * element address lies within the array buffer. Emit an inbounds GEP
         * (matching the higher-order map path) so the optimizer can reason
         * about the access and hoist the invariant base out of the loop.
         */
        LLVMValueRef elem_ptr = LLVMBuildInBoundsGEP2(ctx->builder, elem_ty,
            data_ptr, &idx64, 1, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder,
            LLVMBuildLoad2(ctx->builder, elem_ty, elem_ptr, llvm_tmp_name(ctx)),
            loop_alloca);
        return true;
    }

    get_fn = llvm_mir_for_in_required_runtime(ctx, branch_inst,
        "pgy_list_get_raw_export");
    if (get_fn == NULL)
        return true;
    {
        LLVMValueRef args[] = {
            LLVMBuildBitCast(ctx->builder, list_alloca, ctx->type_i8ptr,
                             llvm_tmp_name(ctx)),
            idx,
            LLVMBuildBitCast(ctx->builder, loop_alloca, ctx->type_i8ptr,
                             llvm_tmp_name(ctx)),
            llvm_sizeof_type_i64(ctx, elem_ty)
        };
        LLVMBuildCall2(ctx->builder, get_fn->fn_type, get_fn->fn,
                       args, 4, "");
    }
    return true;
}

bool
llvm_mir_emit_for_in_body_binding(const MIRRoutine *routine,
                                  const MIRBasicBlock *block,
                                  LLVMGenCtx *ctx)
{
    if (routine == NULL || block == NULL || ctx == NULL)
        return true;

    for (size_t i = 0; i < routine->block_count; i++) {
        const MIRBasicBlock *candidate = &routine->blocks[i];
        const MIRInstruction *branch_inst;
        if (!llvm_mir_for_in_body_region_contains(routine, candidate,
                                                  block->id)) {
            continue;
        }
        branch_inst = llvm_mir_find_for_in_branch_inst(candidate);
        if (!llvm_mir_emit_for_in_binding_for_inst(branch_inst, ctx))
            return false;
        if (ctx->has_error)
            return false;
    }
    return true;
}

#endif /* PGY_LLVM_ENABLED */
