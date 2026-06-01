/*
 * LLVM MIR local and parameter alloca emission.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_mir_local_emit.h"

#include <string.h>

#include "llvm_internal_api.h"
#include "llvm_mir_type_helpers.h"
#include "parser/ast_api.h"
#include "../common/string_compat.h"

static LLVMTypeRef
llvm_mir_local_elem_type_from_layout(LLVMGenCtx *ctx,
                                     const MIRTypeLayout *layout)
{
    char inner_name[256];

    if (ctx == NULL || layout == NULL || layout->abi_type_name == NULL)
        return NULL;
    switch (pgy_classify_type(layout->abi_type_name)) {
    case PGY_TK_ARRAY:
    case PGY_TK_SLICE:
        break;
    default:
        return NULL;
    }
    if (!llvm_constructed_arg_name_copy(layout->abi_type_name, 0,
            inner_name, sizeof(inner_name))) {
        return NULL;
    }
    if (inner_name[0] == '\0' || strcmp(inner_name, "Unknown") == 0)
        return NULL;
    return pergyra_type_to_llvm(ctx, inner_name);
}

static LLVMTypeRef
llvm_mir_local_elem_type_from_type_ast(LLVMGenCtx *ctx, ASTNode *type_node)
{
    const char *type_name;
    GenericParams *args;
    GenericParam *first_arg;
    const char *inner_name;

    if (ctx == NULL || type_node == NULL || type_node->type != AST_TYPE)
        return NULL;
    type_name = ast_type_name(type_node);
    if (type_name == NULL)
        return NULL;
    if (strcmp(type_name, "Array") != 0
        && strcmp(type_name, "Slice") != 0
        && strncmp(type_name, "Array<", 6) != 0
        && strncmp(type_name, "Slice<", 6) != 0) {
        return NULL;
    }
    args = ast_type_generic_args(type_node);
    if (args == NULL || ast_generic_param_count(args) == 0)
        return NULL;
    first_arg = ast_generic_param_at(args, 0);
    inner_name = ast_generic_param_name(first_arg);
    if (inner_name == NULL || inner_name[0] == '\0')
        return NULL;
    return pergyra_type_to_llvm(ctx, inner_name);
}

static bool
llvm_mir_local_require_elem_type(LLVMGenCtx *ctx, ASTNode *site,
                                 LLVMTypeRef elem_type,
                                 const char *surface)
{
    if (ctx != NULL && elem_type != NULL && !ctx->has_error)
        return true;
    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx,
            site,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM MIR local '%s' requires concrete Array<T>/Slice<T> element metadata",
            surface != NULL ? surface : "<local>");
    }
    return false;
}

static LLVMTypeRef
llvm_mir_local_type_from_vars(LLVMMirVar *vars, size_t var_count,
                              const char *name)
{
    LLVMMirVar *entry;
    char base_name[128];

    if (vars == NULL || name == NULL)
        return NULL;

    entry = llvm_mir_get_var_entry(vars, var_count, name);
    if (entry != NULL)
        return entry->type;

    for (size_t i = var_count; i > 0; i--) {
        const char *mir_name = vars[i - 1].mir_name;
        if (mir_name == NULL)
            continue;
        if (!llvm_mir_base_name_from_versioned(mir_name, base_name,
                sizeof(base_name)))
            continue;
        if (strcmp(base_name, name) == 0)
            return vars[i - 1].type;
    }

    return NULL;
}

static bool
llvm_mir_value_expr_is_method_call(ASTNode *expr)
{
    ASTNode *callee;

    if (expr == NULL)
        return false;
    if (expr->type == AST_ARRAY_ACCESS)
        return true;
    if (expr->type != AST_CALL)
        return false;
    callee = ast_call_callee(expr);
    if (callee == NULL)
        return false;
    return callee->type == AST_MEMBER_ACCESS
        || callee->type == AST_IDENTIFIER;
}

static LLVMTypeRef
llvm_mir_local_type_from_value_fact(const MIRInstruction *inst,
                                    LLVMMirVar *vars,
                                    size_t var_count)
{
    ASTNode *value_expr;

    if (inst == NULL)
        return NULL;
    value_expr = inst->expr0;
    if (llvm_mir_value_expr_is_method_call(value_expr))
        return NULL;
    if (inst->use_count > 0 && inst->uses != NULL) {
        LLVMTypeRef use_type =
            llvm_mir_local_type_from_vars(vars, var_count, inst->uses[0]);
        if (use_type != NULL)
            return use_type;
    }

    if (value_expr != NULL
        && value_expr->type == AST_IDENTIFIER) {
        const char *value_name = ast_identifier_name(value_expr);
        if (value_name != NULL) {
            return llvm_mir_local_type_from_vars(vars, var_count, value_name);
        }
    }

    return NULL;
}

static const MIRInstruction *
llvm_mir_find_result_instruction(const MIRRoutine *routine,
                                 const char *result_name)
{
    if (routine == NULL || result_name == NULL)
        return NULL;
    for (size_t b = 0; b < routine->block_count; b++) {
        const MIRBasicBlock *block = &routine->blocks[b];
        for (size_t i = 0; i < block->instruction_count; i++) {
            const MIRInstruction *inst = &block->instructions[i];
            if (inst->result_name != NULL
                && strcmp(inst->result_name, result_name) == 0) {
                return inst;
            }
        }
    }
    return NULL;
}

static LLVMTypeRef
llvm_mir_local_type_from_instruction_fact(const MIRRoutine *routine,
                                          LLVMGenCtx *ctx,
                                          const MIRInstruction *inst,
                                          LLVMMirVar *vars,
                                          size_t var_count,
                                          unsigned depth);

static LLVMTypeRef
llvm_mir_local_type_from_named_result(const MIRRoutine *routine,
                                      LLVMGenCtx *ctx,
                                      const char *result_name,
                                      LLVMMirVar *vars,
                                      size_t var_count,
                                      unsigned depth)
{
    const MIRInstruction *source;
    LLVMTypeRef type;

    if (result_name == NULL || depth > 16)
        return NULL;

    type = llvm_mir_local_type_from_vars(vars, var_count, result_name);
    if (type != NULL)
        return type;

    source = llvm_mir_find_result_instruction(routine, result_name);
    if (source == NULL)
        return NULL;
    return llvm_mir_local_type_from_instruction_fact(routine, ctx, source,
        vars, var_count, depth + 1);
}

static LLVMTypeRef
llvm_mir_array_type_from_slice_type(LLVMGenCtx *ctx, LLVMTypeRef slice_type)
{
    if (ctx == NULL || slice_type == NULL)
        return NULL;
    if (slice_type == ctx->slice_type_Int)
        return ctx->array_type_Int;
    if (slice_type == ctx->slice_type_Long)
        return ctx->array_type_Long;
    if (slice_type == ctx->slice_type_Float)
        return ctx->array_type_Float;
    if (slice_type == ctx->slice_type_Double)
        return ctx->array_type_Double;
    if (slice_type == ctx->slice_type_Bool)
        return ctx->array_type_Bool;
    if (slice_type == ctx->slice_type_String)
        return ctx->array_type_String;
    return NULL;
}

static LLVMTypeRef
llvm_mir_local_type_from_slice_copy_fact(const MIRRoutine *routine,
                                         LLVMGenCtx *ctx,
                                         const MIRInstruction *inst,
                                         LLVMMirVar *vars,
                                         size_t var_count,
                                         unsigned depth)
{
    LLVMTypeRef slice_type;
    LLVMTypeRef array_type;

    if (inst == NULL || inst->arg1 == NULL
        || strcmp(inst->arg1, "SliceCopy") != 0
        || inst->use_count == 0 || inst->uses == NULL) {
        return NULL;
    }

    slice_type = llvm_mir_local_type_from_named_result(routine, ctx,
        inst->uses[0], vars, var_count, depth + 1);
    array_type = llvm_mir_array_type_from_slice_type(ctx, slice_type);
    if (array_type != NULL || slice_type == NULL || ctx == NULL
        || ctx->has_error) {
        return array_type;
    }

    llvm_set_mir_inventory_missing(ctx,
        "LLVM MIR SliceCopy result '%s' requires Slice<T> operand metadata",
        inst->result_name != NULL ? inst->result_name : "(anonymous-local)");
    return NULL;
}

static ASTNode *
llvm_mir_local_initializer_expr(ASTNode *expr)
{
    if (expr != NULL && expr->type == AST_LET_DECL)
        return ast_let_initializer(expr);
    return expr;
}

static ASTNode *
llvm_mir_local_recv_expr(ASTNode *expr)
{
    ASTNode *value = llvm_mir_local_initializer_expr(expr);

    if (value == NULL)
        return NULL;
    if (value->type == AST_CHANNEL_RECV)
        return value;
    if (value->type == AST_ASSIGNMENT
        && ast_assignment_value(value) != NULL
        && ast_assignment_value(value)->type == AST_CHANNEL_RECV) {
        return ast_assignment_value(value);
    }
    return NULL;
}

static ASTNode *
llvm_mir_local_await_expr(ASTNode *expr)
{
    ASTNode *value = llvm_mir_local_initializer_expr(expr);

    return value != NULL && value->type == AST_AWAIT_EXPR ? value : NULL;
}

static const MIRInstruction *
llvm_mir_local_find_base_def(const MIRRoutine *routine, const char *base_name)
{
    char result_base[128];

    if (routine == NULL || base_name == NULL)
        return NULL;
    for (size_t b = 0; b < routine->block_count; b++) {
        const MIRBasicBlock *block = &routine->blocks[b];
        for (size_t i = 0; i < block->instruction_count; i++) {
            const MIRInstruction *candidate = &block->instructions[i];
            if (candidate->kind != MIR_INST_DEF)
                continue;
            if (candidate->arg0 != NULL
                && strcmp(candidate->arg0, base_name) == 0)
                return candidate;
            if (candidate->result_name == NULL)
                continue;
            if (!llvm_mir_base_name_from_versioned(candidate->result_name,
                    result_base, sizeof(result_base)))
                continue;
            if (strcmp(result_base, base_name) == 0)
                return candidate;
        }
    }
    return NULL;
}

static const char *
llvm_mir_local_first_type_arg_scratch(LLVMGenCtx *ctx, ASTNode *type_node,
                                      const char *container_name)
{
    GenericParams *args;
    GenericParam *arg0;

    if (ctx == NULL || type_node == NULL || type_node->type != AST_TYPE)
        return NULL;
    if (ast_type_name(type_node) == NULL
        || strcmp(ast_type_name(type_node), container_name) != 0)
        return NULL;
    args = ast_type_generic_args(type_node);
    arg0 = ast_generic_param_at(args, 0);
    if (arg0 == NULL)
        return NULL;
    return llvm_stmt_render_type_arg_scratch(arg0, &ctx->scratch);
}

static LLVMTypeRef
llvm_mir_local_type_from_channel_recv_fact(const MIRRoutine *routine,
                                           LLVMGenCtx *ctx,
                                           const MIRInstruction *inst)
{
    ASTNode *recv;
    ASTNode *channel;
    const MIRInstruction *channel_def;
    ASTNode *source;
    ASTNode *type_ann;
    const char *inner;

    if (routine == NULL || ctx == NULL || inst == NULL)
        return NULL;
    recv = llvm_mir_local_recv_expr(inst->expr0);
    if (recv == NULL)
        return NULL;
    channel = ast_channel_recv_channel(recv);
    if (channel == NULL || channel->type != AST_IDENTIFIER
        || ast_identifier_name(channel) == NULL)
        return NULL;

    channel_def = llvm_mir_local_find_base_def(routine,
        ast_identifier_name(channel));
    source = mir_instruction_source_payload(channel_def);
    type_ann = source != NULL && source->type == AST_LET_DECL
        ? ast_let_type(source) : NULL;
    inner = llvm_mir_local_first_type_arg_scratch(ctx, type_ann, "Channel");
    if (inner == NULL || inner[0] == '\0')
        return NULL;
    return pergyra_type_to_llvm(ctx, inner);
}

static LLVMTypeRef
llvm_mir_local_type_from_await_fact(const MIRRoutine *routine,
                                    LLVMGenCtx *ctx,
                                    const MIRInstruction *inst)
{
    ASTNode *await_expr;
    ASTNode *operand;
    const MIRInstruction *future_def;
    ASTNode *source;
    ASTNode *type_ann;
    ASTNode *init;
    const char *inner;

    if (routine == NULL || ctx == NULL || inst == NULL)
        return NULL;
    await_expr = llvm_mir_local_await_expr(inst->expr0);
    if (await_expr == NULL)
        return NULL;
    operand = ast_await_expression(await_expr);
    if (operand == NULL || operand->type != AST_IDENTIFIER
        || ast_identifier_name(operand) == NULL)
        return NULL;

    future_def = llvm_mir_local_find_base_def(routine,
        ast_identifier_name(operand));
    source = mir_instruction_source_payload(future_def);
    type_ann = source != NULL && source->type == AST_LET_DECL
        ? ast_let_type(source) : NULL;
    inner = llvm_mir_local_first_type_arg_scratch(ctx, type_ann, "Future");
    if (inner == NULL || inner[0] == '\0') {
        init = source != NULL && source->type == AST_LET_DECL
            ? ast_let_initializer(source) : NULL;
        if (init != NULL && init->type == AST_SPAWN_EXPR)
            inner = llvm_infer_spawn_future_inner(ctx, init);
    }
    if (inner == NULL || inner[0] == '\0')
        return NULL;
    return pergyra_type_to_llvm(ctx, inner);
}

static LLVMTypeRef
llvm_mir_local_type_from_instruction_fact(const MIRRoutine *routine,
                                          LLVMGenCtx *ctx,
                                          const MIRInstruction *inst,
                                          LLVMMirVar *vars,
                                          size_t var_count,
                                          unsigned depth)
{
    LLVMTypeRef type;

    if (inst == NULL || ctx == NULL || depth > 16)
        return NULL;

    type = llvm_mir_type_from_abi_layout(ctx, inst->type_layout);
    if (type != NULL)
        return type;

    if (inst->expr1 != NULL) {
        type = llvm_mir_type_from_ast(ctx, inst->expr1);
        if (ctx->has_error || type != NULL)
            return type;
    }

    type = llvm_mir_local_type_from_slice_copy_fact(routine, ctx, inst,
        vars, var_count, depth);
    if (ctx->has_error || type != NULL)
        return type;

    type = llvm_mir_local_type_from_channel_recv_fact(routine, ctx, inst);
    if (ctx->has_error || type != NULL)
        return type;

    type = llvm_mir_local_type_from_await_fact(routine, ctx, inst);
    if (ctx->has_error || type != NULL)
        return type;

    type = llvm_mir_local_type_from_value_fact(inst, vars, var_count);
    if (type != NULL)
        return type;

    if (inst->kind == MIR_INST_PHI) {
        for (size_t i = 0; i < inst->phi_incoming_count; i++) {
            type = llvm_mir_local_type_from_named_result(routine, ctx,
                inst->phi_incomings[i].value_name, vars, var_count,
                depth + 1);
            if (ctx->has_error || type != NULL)
                return type;
        }
        for (size_t i = 0; i < inst->use_count; i++) {
            type = llvm_mir_local_type_from_named_result(routine, ctx,
                inst->uses[i], vars, var_count, depth + 1);
            if (ctx->has_error || type != NULL)
                return type;
        }
        return NULL;
    }

    if (inst->expr0 != NULL) {
        type = llvm_stmt_infer_expr_type(ctx, inst->expr0);
        if (ctx->has_error || type != NULL)
            return type;
    }

    return NULL;
}

void
llvm_emit_mir_local_allocas(const MIRRoutine *routine, LLVMGenCtx *ctx,
                            LLVMMirVar **vars_ptr, size_t *var_capacity_ptr,
                            size_t *var_count_ptr)
{
    LLVMMirVar *vars = vars_ptr != NULL ? *vars_ptr : NULL;
    size_t var_capacity = var_capacity_ptr != NULL ? *var_capacity_ptr : 0;
    size_t var_count = var_count_ptr != NULL ? *var_count_ptr : 0;

    if (routine == NULL || ctx == NULL || vars_ptr == NULL
        || var_capacity_ptr == NULL || var_count_ptr == NULL)
        return;

    for (size_t b = 0; b < routine->block_count; b++) {
        const MIRBasicBlock *mir_block = &routine->blocks[b];
        for (size_t j = 0; j < mir_block->instruction_count; j++) {
            const MIRInstruction *inst = &mir_block->instructions[j];
            if ((inst->kind == MIR_INST_DEF || inst->kind == MIR_INST_PHI)
                && inst->result_name != NULL) {
                LLVMTypeRef alloca_type = NULL;
                LLVMTypeRef layout_type = llvm_mir_type_from_abi_layout(
                    ctx, inst->type_layout);
                ASTNode *value_expr = inst->expr0;
                ASTNode *type_expr = inst->expr1;
                char base_name[128];
                bool has_base_name = llvm_mir_base_name_from_versioned(
                    inst->result_name, base_name, sizeof(base_name));

                if (layout_type != NULL) {
                    alloca_type = layout_type;
                } else if (type_expr != NULL) {
                    alloca_type = llvm_mir_type_from_ast(ctx, type_expr);
                    if (ctx->has_error || alloca_type == NULL)
                        return;
                    if (has_base_name) {
                        llvm_mir_register_nominal_class(ctx, base_name,
                                                        type_expr);
                    }
                } else if (value_expr != NULL) {
                    if (inst->arg1 != NULL
                        && strcmp(inst->arg1, "SliceCopy") == 0) {
                        alloca_type =
                            llvm_mir_local_type_from_instruction_fact(
                                routine, ctx, inst, vars, var_count, 0);
                    }
                    if (alloca_type == NULL) {
                        alloca_type = llvm_mir_local_type_from_value_fact(
                            inst, vars, var_count);
                    }
                    if (alloca_type == NULL && has_base_name) {
                        alloca_type = llvm_mir_local_type_from_vars(
                            vars, var_count, base_name);
                    }
                    if (alloca_type == NULL) {
                        alloca_type = llvm_mir_local_type_from_instruction_fact(
                            routine, ctx, inst, vars, var_count, 0);
                        if (ctx->has_error)
                            return;
                    }
                    if (alloca_type == NULL)
                        alloca_type = llvm_stmt_infer_expr_type(ctx, value_expr);
                    if (ctx->has_error || alloca_type == NULL)
                        return;
                } else if (has_base_name) {
                    alloca_type = llvm_mir_local_type_from_vars(
                        vars, var_count, base_name);
                }
                if (alloca_type == NULL) {
                    alloca_type = llvm_mir_local_type_from_instruction_fact(
                        routine, ctx, inst, vars, var_count, 0);
                    if (ctx->has_error)
                        return;
                }
                if (alloca_type == NULL) {
                    llvm_set_mir_inventory_missing(ctx,
                        "MIR-only LLVM path missing local type metadata for '%s'",
                        inst->result_name != NULL
                            ? inst->result_name
                            : "(anonymous-local)");
                    return;
                }
                if (var_count >= var_capacity) {
                    size_t new_capacity = var_capacity > 0 ? var_capacity * 2 : 64;
                    LLVMMirVar *grown = pgy_arena_calloc(&ctx->scratch,
                        new_capacity * sizeof(LLVMMirVar));
                    if (grown == NULL)
                        return;
                    if (vars != NULL && var_count > 0)
                        memcpy(grown, vars, var_count * sizeof(LLVMMirVar));
                    vars = grown;
                    var_capacity = new_capacity;
                }
                vars[var_count].mir_name = inst->result_name;
                vars[var_count].type = alloca_type;
                vars[var_count].alloca = llvm_create_entry_alloca(
                    ctx, alloca_type, inst->result_name);
                if (has_base_name && type_expr != NULL) {
                    llvm_register_typed_var_binding(ctx, base_name,
                        vars[var_count].alloca, type_expr);
                } else if (has_base_name
                           && inst->type_layout != NULL
                           && inst->type_layout->abi_type_name != NULL) {
                    llvm_register_typed_var_abi_binding(ctx, base_name,
                        vars[var_count].alloca,
                        inst->type_layout->abi_type_name);
                }
                if (has_base_name
                    && value_expr != NULL
                    && value_expr->type == AST_ARRAY_LITERAL) {
                    LLVMTypeRef elem_type = NULL;
                    if (ast_array_literal_count(value_expr) > 0
                        && ast_array_literal_element(value_expr, 0) != NULL) {
                        elem_type = llvm_stmt_infer_expr_type(ctx,
                            ast_array_literal_element(value_expr, 0));
                    } else {
                        if (type_expr != NULL) {
                            elem_type = llvm_mir_local_elem_type_from_type_ast(
                                ctx, type_expr);
                        }
                        if (elem_type == NULL) {
                            elem_type = llvm_mir_local_elem_type_from_layout(
                                ctx, inst->type_layout);
                        }
                    }
                    if (!llvm_mir_local_require_elem_type(ctx, value_expr,
                            elem_type, base_name))
                        return;
                    llvm_register_array_var_binding(ctx, base_name,
                        vars[var_count].alloca, elem_type,
                        (int64_t)ast_array_literal_count(value_expr));
                } else if (has_base_name
                    && value_expr != NULL
                    && value_expr->type == AST_CALL
                    && ast_call_callee(value_expr) != NULL
                    && ast_call_callee(value_expr)->type == AST_MEMBER_ACCESS
                    && ast_member_name(ast_call_callee(value_expr)) != NULL
                    && strcmp(ast_member_name(ast_call_callee(value_expr)),
                        "Slice") == 0) {
                    ASTNode *receiver =
                        ast_member_object(ast_call_callee(value_expr));
                    LLVMTypeRef elem_type = NULL;
                    const char *receiver_name = ast_identifier_name(receiver);
                    if (receiver_name != NULL) {
                        LLVMArrayVarEntry *entry = llvm_lookup_array_var(
                            ctx, receiver_name);
                        if (entry != NULL && entry->elem_type != NULL)
                            elem_type = entry->elem_type;
                    } else if (receiver != NULL
                        && receiver->type == AST_CALL
                        && ast_call_callee(receiver) != NULL
                        && ast_call_callee(receiver)->type == AST_IDENTIFIER) {
                        const char *callee_name =
                            ast_identifier_name(ast_call_callee(receiver));
                        if (callee_name != NULL) {
                            ASTNode *decl = llvm_stmt_find_function_decl_by_name(
                                ctx, callee_name);
                            ASTNode *return_type = ast_func_return_type(decl);
                            GenericParams *return_generic_args =
                                ast_type_generic_args(return_type);
                            if (return_type != NULL
                                && return_type->type == AST_TYPE
                                && ast_generic_param_at(return_generic_args, 0) != NULL) {
                                char *elem_name =
                                    llvm_stmt_render_type_arg_scratch(
                                        ast_generic_param_at(return_generic_args, 0),
                                        &ctx->scratch);
                                if (elem_name == NULL || elem_name[0] == '\0') {
                                    if (!ctx->has_error) {
                                        llvm_set_error_at_with_hints(ctx,
                                            value_expr,
                                            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                                            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                                            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                                            "LLVM MIR Slice receiver '%s' requires concrete element type metadata",
                                            callee_name);
                                    }
                                } else {
                                    elem_type = pergyra_type_to_llvm(
                                        ctx, elem_name);
                                }
                            }
                        }
                    }
                    if (elem_type == NULL)
                        elem_type = llvm_mir_local_elem_type_from_layout(
                            ctx, inst->type_layout);
                    if (!llvm_mir_local_require_elem_type(ctx, value_expr,
                            elem_type, base_name))
                        return;
                    llvm_register_array_var_binding(ctx, base_name,
                        vars[var_count].alloca, elem_type, -1);
                }
                var_count++;
            }
        }
    }

    *vars_ptr = vars;
    *var_capacity_ptr = var_capacity;
    *var_count_ptr = var_count;
}

#endif
