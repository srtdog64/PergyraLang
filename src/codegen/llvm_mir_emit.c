/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM backend MIR function emission split from llvm_backend.c.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

typedef struct {
    const char *mir_name;
    LLVMValueRef alloca;
    LLVMTypeRef type;
} LLVMMirVar;

static LLVMValueRef
llvm_mir_get_var(LLVMMirVar *vars, size_t count, const char *name)
{
    for (size_t i = 0; i < count; i++) {
        if (vars[i].mir_name && strcmp(vars[i].mir_name, name) == 0)
            return vars[i].alloca;
    }
    return NULL;
}

static LLVMTypeRef
llvm_mir_type_from_ast(LLVMGenCtx *ctx, ASTNode *type_node)
{
    if (ctx == NULL || type_node == NULL)
        return ctx->type_i32;
    LLVMTypeRef type = ast_type_to_llvm(ctx, type_node);
    return type != NULL ? type : ctx->type_i32;
}

static void
llvm_emit_mir_block_with_exprs(const MIRBasicBlock *mir_block, const MIRRoutine *routine,
                               LLVMGenCtx *ctx, LLVMBasicBlockRef *llvm_blocks,
                               LLVMMirVar *vars, size_t var_count, ASTNode *func_decl)
{
    (void)routine;
    (void)func_decl;
    LLVMBasicBlockRef llvm_block = llvm_blocks[mir_block->id];
    LLVMPositionBuilderAtEnd(ctx->builder, llvm_block);
    bool emitted_terminator = false;

    for (size_t i = 0; i < mir_block->instruction_count; i++) {
        const MIRInstruction *inst = &mir_block->instructions[i];
        switch (inst->kind) {
        case MIR_INST_RESOURCE_OP:
            break;
        case MIR_INST_DEF:
            if (inst->ast != NULL && inst->result_name != NULL) {
                if (inst->ast->type == AST_LET_DECL || inst->ast->type == AST_ASSIGNMENT) {
                    llvm_emit_statement(inst->ast, ctx);
                } else {
                    LLVMValueRef alloca = llvm_mir_get_var(vars, var_count, inst->result_name);
                    if (alloca != NULL) {
                        LLVMValueRef val = llvm_emit_expression(inst->ast, ctx);
                        if (val != NULL)
                            LLVMBuildStore(ctx->builder, val, alloca);
                    }
                }
            }
            break;
        case MIR_INST_PHI:
            break;
        case MIR_INST_BRANCH:
            if (inst->ast != NULL && mir_block->has_succ_true && mir_block->has_succ_false) {
                LLVMValueRef cond = llvm_emit_expression(inst->ast, ctx);
                if (cond != NULL) {
                    LLVMBasicBlockRef true_bb = llvm_blocks[mir_block->succ_true];
                    LLVMBasicBlockRef false_bb = llvm_blocks[mir_block->succ_false];
                    LLVMBuildCondBr(ctx->builder, cond, true_bb, false_bb);
                    emitted_terminator = true;
                }
            } else if (mir_block->has_succ_true) {
                LLVMBuildBr(ctx->builder, llvm_blocks[mir_block->succ_true]);
                emitted_terminator = true;
            }
            break;
        case MIR_INST_RETURN:
            llvm_emit_defers_from(ctx, 0);
            if (inst->ast != NULL) {
                LLVMValueRef val = llvm_emit_expression(inst->ast, ctx);
                if (val != NULL) {
                    LLVMBuildRet(ctx->builder, val);
                    emitted_terminator = true;
                } else {
                    LLVMBuildRetVoid(ctx->builder);
                }
            } else {
                LLVMBuildRetVoid(ctx->builder);
            }
            emitted_terminator = true;
            break;
        case MIR_INST_CLEANUP_EDGE:
            break;
        case MIR_INST_STMT:
            if (inst->ast != NULL)
                llvm_emit_statement(inst->ast, ctx);
            break;
        default:
            break;
        }
    }

    if (!emitted_terminator) {
        if (mir_block->has_succ_true && mir_block->has_succ_false) {
            LLVMBuildCondBr(ctx->builder,
                            LLVMConstInt(LLVMInt1TypeInContext(
                                LLVMGetModuleContext(ctx->module)), 1, false),
                            llvm_blocks[mir_block->succ_true],
                            llvm_blocks[mir_block->succ_false]);
        } else if (mir_block->has_succ_true) {
            LLVMBuildBr(ctx->builder, llvm_blocks[mir_block->succ_true]);
        } else {
            llvm_emit_defers_from(ctx, 0);
            if (ctx->current_ret_type == ctx->type_void) {
                LLVMBuildRetVoid(ctx->builder);
            } else {
                LLVMBuildRet(ctx->builder, LLVMConstNull(ctx->current_ret_type));
            }
        }
    }
}

LLVMValueRef
llvm_emit_func_from_mir(const MIRRoutine *routine, LLVMGenCtx *ctx)
{
    size_t param_count = 0;
    bool is_intent = false;
    bool is_method = false;
    const char *owner_name = NULL;
    LLVMClassTypeEntry *owner_cls = NULL;
    const char *fn_name = NULL;
    char qualified_name[256];
    if (routine == NULL || ctx == NULL || routine->hir_routine == NULL)
        return NULL;

    ASTNode *func_decl = routine->hir_routine->ast;
    if (func_decl == NULL
        || (func_decl->type != AST_FUNC_DECL && func_decl->type != AST_INTENT_DECL))
        return NULL;

    is_intent = (func_decl->type == AST_INTENT_DECL);
    is_method = (!is_intent && routine->kind == MIR_SCOPE_METHOD);
    owner_name = routine->owner_name;
    owner_cls = (is_method && owner_name != NULL)
        ? llvm_lookup_class(ctx, owner_name)
        : NULL;
    param_count = is_intent
        ? func_decl->data.intent_decl.involve_count
        : (is_method ? 1 : 0);
    if (!is_intent) {
        for (size_t i = 0; i < func_decl->data.func_decl.param_count; i++) {
            FuncParam *p = func_decl->data.func_decl.params[i];
            if (is_method && p != NULL && p->type == NULL
                && p->name != NULL && strcmp(p->name, "self") == 0) {
                continue;
            }
            param_count++;
        }
    }
    LLVMTypeRef *param_types = calloc(param_count > 0 ? param_count : 1, sizeof(LLVMTypeRef));
    for (size_t i = 0; i < param_count; i++) {
        if (is_intent) {
            ASTNode *involves = func_decl->data.intent_decl.involves[i];
            if (involves != NULL && involves->data.intent_involves.subject_type != NULL) {
                param_types[i] = llvm_mir_type_from_ast(
                    ctx, involves->data.intent_involves.subject_type);
                if (llvm_intent_involves_uses_pointer_self(ctx, involves))
                    param_types[i] = LLVMPointerType(param_types[i], 0);
            } else {
                param_types[i] = ctx->type_i32;
            }
        } else if (is_method && i == 0) {
            if (owner_cls != NULL) {
                param_types[i] = owner_cls->is_pointer_self_host
                    ? LLVMPointerType(owner_cls->struct_type, 0)
                    : owner_cls->struct_type;
            } else {
                param_types[i] = ctx->type_i8ptr;
            }
        } else {
            size_t param_index = i;
            if (is_method)
                param_index--;
            FuncParam *p = func_decl->data.func_decl.params[param_index];
            while (is_method && p != NULL && p->type == NULL
                   && p->name != NULL && strcmp(p->name, "self") == 0) {
                param_index++;
                p = (param_index < func_decl->data.func_decl.param_count)
                    ? func_decl->data.func_decl.params[param_index]
                    : NULL;
            }
            if (p != NULL && p->type != NULL)
                param_types[i] = llvm_mir_type_from_ast(ctx, p->type);
            else
                param_types[i] = ctx->type_i32;
        }
    }
    LLVMTypeRef ret_type = is_intent ? ctx->type_i1 : ctx->type_i32;
    if (!is_intent && func_decl->data.func_decl.return_type != NULL)
        ret_type = llvm_mir_type_from_ast(ctx, func_decl->data.func_decl.return_type);

    LLVMTypeRef func_type = LLVMFunctionType(ret_type, param_types, (unsigned)param_count, 0);
    fn_name = routine->name;
    if (is_method && owner_name != NULL && routine->name != NULL) {
        snprintf(qualified_name, sizeof(qualified_name), "%s_%s", owner_name, routine->name);
        fn_name = qualified_name;
    }
    LLVMFuncEntry *entry = llvm_lookup_or_create_function(ctx, fn_name, func_type, ret_type);
    LLVMValueRef fn = entry != NULL ? entry->fn : NULL;
    if (fn == NULL)
        return NULL;
    free(param_types);

    size_t var_capacity = 64;
    LLVMMirVar *vars = calloc(var_capacity, sizeof(LLVMMirVar));
    size_t var_count = 0;

    LLVMBasicBlockRef *llvm_blocks = calloc(routine->block_count, sizeof(LLVMBasicBlockRef));
    for (size_t i = 0; i < routine->block_count; i++) {
        char bb_name[64];
        snprintf(bb_name, sizeof(bb_name), "bb_%zu", i);
        llvm_blocks[i] = LLVMAppendBasicBlockInContext(ctx->context, fn, bb_name);
    }

    LLVMPositionBuilderAtEnd(ctx->builder, llvm_blocks[routine->entry_block]);
    for (size_t b = 0; b < routine->block_count; b++) {
        const MIRBasicBlock *mir_block = &routine->blocks[b];
        for (size_t j = 0; j < mir_block->instruction_count; j++) {
            const MIRInstruction *inst = &mir_block->instructions[j];
            if ((inst->kind == MIR_INST_DEF || inst->kind == MIR_INST_PHI)
                && inst->result_name != NULL) {
                LLVMTypeRef alloca_type = ctx->type_i32;
                if (var_count >= var_capacity) {
                    var_capacity *= 2;
                    vars = realloc(vars, var_capacity * sizeof(LLVMMirVar));
                }
                vars[var_count].mir_name = inst->result_name;
                vars[var_count].type = alloca_type;
                vars[var_count].alloca = LLVMBuildAlloca(ctx->builder, alloca_type,
                                                         inst->result_name);
                var_count++;
            }
        }
    }

    LLVMValueRef saved_fn = ctx->current_function;
    LLVMTypeRef saved_ret = ctx->current_ret_type;
    int saved_slot_var_count = ctx->slot_var_count;
    int saved_view_var_count = ctx->view_var_count;
    int saved_device_slot_var_count = ctx->device_slot_var_count;
    int saved_future_var_count = ctx->future_var_count;
    int saved_channel_var_count = ctx->channel_var_count;
    int saved_var_class_count = ctx->var_class_count;
    int saved_projection_borrow_count = ctx->projection_borrow_count;
    int saved_array_var_count = ctx->array_var_count;
    int saved_list_var_count = ctx->list_var_count;
    int saved_set_var_count = ctx->set_var_count;
    int saved_queue_var_count = ctx->queue_var_count;
    int saved_map_var_count = ctx->map_var_count;
    int saved_callable_var_count = ctx->callable_var_count;
    const char *saved_class_name = ctx->current_class_name;
    ctx->current_function = fn;
    ctx->current_ret_type = ret_type;
    if (is_method)
        ctx->current_class_name = owner_name;

    llvm_scope_push(ctx);
    llvm_defer_scope_push(ctx);

    LLVMPositionBuilderAtEnd(ctx->builder, llvm_blocks[routine->entry_block]);
    for (size_t i = 0; i < param_count; i++) {
        if (is_intent) {
            ASTNode *involves = func_decl->data.intent_decl.involves[i];
            const char *alias = involves != NULL ? involves->data.intent_involves.alias : NULL;
            ASTNode *subject_type = involves != NULL ? involves->data.intent_involves.subject_type : NULL;
            const char *type_name = (subject_type != NULL && subject_type->type == AST_TYPE)
                ? subject_type->data.type.name : NULL;
            LLVMTypeRef pt = (subject_type != NULL) ? llvm_mir_type_from_ast(ctx, subject_type)
                                                    : ctx->type_i32;
            if (involves != NULL && llvm_intent_involves_uses_pointer_self(ctx, involves))
                pt = LLVMPointerType(pt, 0);
            LLVMValueRef alloca = LLVMBuildAlloca(ctx->builder, pt,
                alias != NULL ? alias : "actor");
            LLVMBuildStore(ctx->builder, LLVMGetParam(fn, (unsigned)i), alloca);
            llvm_scope_declare(ctx, alias != NULL ? alias : "actor", alloca, pt);
            if (subject_type != NULL)
                llvm_register_typed_var(ctx, alias, subject_type);
            if (type_name != NULL)
                llvm_register_var_class(ctx, alias, type_name);
        } else {
            if (is_method && i == 0) {
                LLVMTypeRef self_type = owner_cls != NULL
                    ? (owner_cls->is_pointer_self_host
                        ? LLVMPointerType(owner_cls->struct_type, 0)
                        : owner_cls->struct_type)
                    : ctx->type_i8ptr;
                LLVMValueRef alloca = LLVMBuildAlloca(ctx->builder, self_type,
                    (owner_cls != NULL && owner_cls->is_pointer_self_host) ? "self.addr" : "self");
                LLVMBuildStore(ctx->builder, LLVMGetParam(fn, 0), alloca);
                llvm_scope_declare(ctx, "self", alloca, self_type);
                if (owner_name != NULL)
                    llvm_register_var_class(ctx, "self", owner_name);
            } else {
                size_t param_index = is_method ? (i - 1) : i;
                FuncParam *p = func_decl->data.func_decl.params[param_index];
                while (is_method && p != NULL && p->type == NULL
                       && p->name != NULL && strcmp(p->name, "self") == 0) {
                    param_index++;
                    p = (param_index < func_decl->data.func_decl.param_count)
                        ? func_decl->data.func_decl.params[param_index]
                        : NULL;
                }
                if (p == NULL)
                    continue;
                LLVMTypeRef pt = (p->type != NULL) ? llvm_mir_type_from_ast(ctx, p->type)
                                                   : ctx->type_i32;
                LLVMValueRef alloca = LLVMBuildAlloca(ctx->builder, pt, p->name);
                LLVMBuildStore(ctx->builder, LLVMGetParam(fn, (unsigned)i), alloca);
                llvm_scope_declare(ctx, p->name, alloca, pt);
                llvm_register_typed_var(ctx, p->name, p->type);
            }
        }
    }

    if (routine->entry_block < routine->block_count) {
        llvm_emit_mir_block_with_exprs(&routine->blocks[routine->entry_block], routine, ctx,
                                       llvm_blocks, vars, var_count, func_decl);
    }
    for (size_t i = 0; i < routine->block_count; i++) {
        if (i == routine->entry_block)
            continue;
        const MIRBasicBlock *mir_block = &routine->blocks[i];
        if (mir_block->is_reachable && !mir_block->is_cleanup) {
            llvm_emit_mir_block_with_exprs(mir_block, routine, ctx, llvm_blocks,
                                           vars, var_count, func_decl);
        } else if (!mir_block->is_cleanup) {
            LLVMPositionBuilderAtEnd(ctx->builder, llvm_blocks[i]);
            LLVMBuildUnreachable(ctx->builder);
        }
    }

    if (routine->has_cleanup_block) {
        for (size_t i = 0; i < routine->block_count; i++) {
            const MIRBasicBlock *mir_block = &routine->blocks[i];
            if (mir_block->is_cleanup && mir_block->is_reachable) {
                llvm_emit_mir_block_with_exprs(mir_block, routine, ctx, llvm_blocks,
                                               vars, var_count, func_decl);
            }
        }
    }

    llvm_defer_scope_pop(ctx);
    llvm_scope_pop(ctx);
    ctx->slot_var_count = saved_slot_var_count;
    ctx->view_var_count = saved_view_var_count;
    ctx->device_slot_var_count = saved_device_slot_var_count;
    ctx->future_var_count = saved_future_var_count;
    ctx->channel_var_count = saved_channel_var_count;
    ctx->var_class_count = saved_var_class_count;
    ctx->projection_borrow_count = saved_projection_borrow_count;
    ctx->array_var_count = saved_array_var_count;
    ctx->list_var_count = saved_list_var_count;
    ctx->set_var_count = saved_set_var_count;
    ctx->queue_var_count = saved_queue_var_count;
    ctx->map_var_count = saved_map_var_count;
    ctx->callable_var_count = saved_callable_var_count;
    ctx->current_function = saved_fn;
    ctx->current_ret_type = saved_ret;
    ctx->current_class_name = saved_class_name;
    free(vars);
    free(llvm_blocks);
    return fn;
}

#endif
