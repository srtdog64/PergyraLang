/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_mir_await_emit.h"

#include <string.h>

#include "llvm_expr_spawn_call_helpers.h"
#include "llvm_internal_api.h"
#include "llvm_mir_async_fact.h"
#include "llvm_mir_scope_bind.h"
#include "../parser/ast_api.h"

static const MIRInstruction *
llvm_mir_find_await_resource_op(const MIRBasicBlock *mir_block,
                                const char *future_name)
{
    if (mir_block == NULL || future_name == NULL)
        return NULL;

    for (size_t i = 0; i < mir_block->instruction_count; i++) {
        const MIRInstruction *candidate = &mir_block->instructions[i];
        if (candidate->kind != MIR_INST_RESOURCE_OP
            || candidate->name == NULL
            || (strcmp(candidate->name, "AwaitLocal") != 0
                && strcmp(candidate->name, "AwaitRemote") != 0)) {
            continue;
        }
        if (candidate->arg0 != NULL && strcmp(candidate->arg0, future_name) == 0)
            return candidate;
        for (size_t u = 0; u < candidate->use_count; u++) {
            char base_name[128];
            if (llvm_mir_base_name_from_versioned(candidate->uses[u],
                    base_name, sizeof(base_name))
                && strcmp(base_name, future_name) == 0) {
                return candidate;
            }
        }
    }
    return NULL;
}

bool
llvm_mir_try_emit_await_local_def(const MIRInstruction *inst,
                                  const MIRBasicBlock *mir_block,
                                  const MIRRoutine *routine,
                                  LLVMGenCtx *ctx,
                                  LLVMMirVar *vars,
                                  size_t var_count,
                                  bool *handled)
{
    char base_name[128];
    ASTNode *stmt;
    ASTNode *init;
    ASTNode *operand;
    const char *future_name;
    const char *inner;
    const char *resource_name;
    char inner_from_mir[256];
    bool is_remote;
    LLVMMirVar *target;
    LLVMValueRef task;
    LLVMValueRef value;
    ASTNode *type_ann;

    if (handled != NULL)
        *handled = false;
    if (inst == NULL || ctx == NULL || handled == NULL
        || !mir_instruction_uses_source_local_decl_emit(inst)
        || mir_instruction_source_payload(inst) == NULL
        || !llvm_mir_base_name_from_versioned(inst->result_name, base_name,
            sizeof(base_name))) {
        return true;
    }

    stmt = mir_instruction_source_payload(inst);
    if (stmt == NULL || stmt->type != AST_LET_DECL)
        return true;
    init = ast_let_initializer(stmt);
    if (init == NULL || init->type != AST_AWAIT_EXPR)
        return true;
    operand = ast_await_expression(init);
    if (operand == NULL) {
        llvm_set_error_at_with_hints(ctx, init,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM MIR AwaitLocal def requires a Future<T> operand");
        return false;
    }
    future_name = operand->type == AST_IDENTIFIER
        ? ast_identifier_name(operand) : NULL;
    resource_name = operand->type == AST_SPAWN_EXPR ? "spawn" : future_name;
    if (resource_name == NULL || resource_name[0] == '\0') {
        llvm_set_error_at_with_hints(ctx, init,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM MIR AwaitLocal def has unnamed Future<T> operand");
        return false;
    }
    if (llvm_mir_find_await_resource_op(mir_block, resource_name) == NULL) {
        llvm_set_error_at_with_hints(ctx, init,
            PGY_CODE_MIR_TOPOLOGY_INVALID,
            PGY_CAUSE_MIR_TOPOLOGY_INVALID,
            PGY_FIX_INSPECT_HIR_TO_MIR_LOWERING,
            "LLVM MIR await def requires matching AwaitLocal/AwaitRemote resource fact");
        return false;
    }

    target = llvm_mir_get_var_entry(vars, var_count, inst->result_name);
    if (target == NULL || target->alloca == NULL || target->type == NULL) {
        llvm_set_error_at_with_hints(ctx, init,
            PGY_CODE_MIR_TOPOLOGY_INVALID,
            PGY_CAUSE_MIR_TOPOLOGY_INVALID,
            PGY_FIX_INSPECT_HIR_TO_MIR_LOWERING,
            "LLVM MIR AwaitLocal def target is missing SSA storage");
        return false;
    }

    if (operand->type == AST_SPAWN_EXPR) {
        inner = llvm_infer_spawn_future_inner(ctx, operand);
        is_remote = false;
    } else if (operand->type == AST_IDENTIFIER) {
        inner = llvm_lookup_future_inner(ctx, future_name);
        is_remote = llvm_lookup_future_is_remote(ctx, future_name);
    } else {
        llvm_set_error_at_with_hints(ctx, init,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM MIR AwaitLocal def requires a named or inline-spawn Future<T> operand");
        return false;
    }
    if ((inner == NULL || inner[0] == '\0')
        && future_name != NULL
        && llvm_mir_async_fact_future_inner_from_source_local(
            routine, future_name, inner_from_mir, sizeof(inner_from_mir),
            &is_remote)) {
        inner = inner_from_mir;
    }
    if (inner == NULL || inner[0] == '\0') {
        llvm_set_error_at_with_hints(ctx, init,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM MIR AwaitLocal def requires registered Future<T> metadata");
        return false;
    }
    task = llvm_emit_expression(operand, ctx);
    if (task == NULL) {
        if (!ctx->has_error) {
            llvm_set_error_at_with_hints(ctx, operand,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_INSPECT_MIR_INVENTORY,
                "LLVM MIR AwaitLocal def could not lower its Future<T> operand");
        }
        return false;
    }
    value = llvm_await_task_handle(ctx, init, task, inner, is_remote);
    if (value == NULL) {
        if (!ctx->has_error) {
            llvm_set_error_at_with_hints(ctx, init,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_INSPECT_MIR_INVENTORY,
                "LLVM MIR AwaitLocal def did not produce a value");
        }
        return false;
    }
    LLVMBuildStore(ctx->builder, value, target->alloca);
    llvm_mir_bind_base_local_scope(ctx, base_name, target->alloca,
        target->type, inst->arg1);
    type_ann = ast_let_type(stmt);
    if (type_ann != NULL)
        llvm_register_typed_var_binding(ctx, base_name, target->alloca,
            type_ann);
    else if (inst->abi_type_name != NULL)
        llvm_register_typed_var_abi_binding(ctx, base_name, target->alloca,
            inst->abi_type_name);
    *handled = true;
    return !ctx->has_error;
}

#endif
