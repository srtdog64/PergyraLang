/*
 * LLVM MIR local and parameter alloca emission.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_mir_local_emit.h"
#include "llvm_mir_host_field.h"

#include <string.h>

#include "llvm_internal_api.h"
#include "llvm_mir_async_fact.h"
#include "llvm_mir_local_expected_type.h"
#include "llvm_mir_local_element_type.h"
#include "llvm_mir_local_type_lookup.h"
#include "llvm_mir_slice_fact.h"
#include "llvm_mir_type_helpers.h"
#include "codegen_slot_type_policy.h"
#include "parser/ast_api.h"
#include "../common/string_compat.h"

static const MIRSourceLocalType *
llvm_mir_local_source_fact(const MIRRoutine *routine, const char *name)
{
    const MIRSourceLocalType *fact;
    char base_name[128];

    if (routine == NULL || name == NULL)
        return NULL;
    fact = mir_routine_source_local_type_fact(routine, name);
    if (fact == NULL
        && llvm_mir_base_name_from_versioned(name, base_name,
            sizeof(base_name))
        && strcmp(base_name, name) != 0) {
        fact = mir_routine_source_local_type_fact(routine, base_name);
    }
    return fact;
}

static LLVMTypeRef
llvm_mir_local_type_from_source_fact_entry(LLVMGenCtx *ctx,
                                           const MIRSourceLocalType *fact)
{
    LLVMTypeRef *param_types = NULL;
    LLVMTypeRef ret_type;

    if (ctx == NULL || fact == NULL)
        return NULL;
    if (!fact->is_callable)
        return fact->type_name != NULL
            ? pergyra_type_to_llvm(ctx, fact->type_name)
            : NULL;
    ret_type = fact->callable_return_type_name != NULL
        ? pergyra_type_to_llvm(ctx, fact->callable_return_type_name)
        : ctx->type_void;
    if (ctx->has_error || ret_type == NULL)
        return NULL;
    if (fact->callable_param_count > 0) {
        param_types = pgy_arena_calloc(&ctx->scratch,
            fact->callable_param_count * sizeof(LLVMTypeRef));
        if (param_types == NULL) {
            llvm_set_mir_inventory_missing(ctx,
                "LLVM MIR callable source-local type parameter allocation failed");
            return NULL;
        }
        for (size_t i = 0; i < fact->callable_param_count; i++) {
            param_types[i] = pergyra_type_to_llvm(ctx,
                fact->callable_param_type_names[i]);
            if (ctx->has_error || param_types[i] == NULL)
                return NULL;
        }
    }
    return LLVMPointerType(
        LLVMFunctionType(ret_type, param_types,
            (unsigned)fact->callable_param_count, 0),
        0);
}

static LLVMTypeRef
llvm_mir_local_type_from_source_fact(const MIRRoutine *routine,
                                     LLVMGenCtx *ctx,
                                     const char *name)
{
    const MIRSourceLocalType *fact;

    if (routine == NULL || ctx == NULL || name == NULL)
        return NULL;
    fact = llvm_mir_local_source_fact(routine, name);
    return llvm_mir_local_type_from_source_fact_entry(ctx, fact);
}

static void
llvm_mir_bind_source_local_base(const MIRRoutine *routine,
                                LLVMGenCtx *ctx,
                                const MIRInstruction *inst,
                                const char *name,
                                LLVMValueRef alloca,
                                LLVMTypeRef type)
{
    const char *type_name;
    char *owned_name;

    if (routine == NULL || ctx == NULL || inst == NULL || name == NULL
        || alloca == NULL || type == NULL)
        return;
    type_name = mir_routine_source_local_type_name(routine, name);
    if (type_name == NULL && !mir_instruction_uses_source_local_decl_emit(inst))
        return;
    owned_name = pgy_arena_strdup(&ctx->persistent, name);
    if (owned_name == NULL) {
        llvm_set_mir_topology_invalid(ctx,
            "LLVM MIR local source fact scope binding out of memory");
        return;
    }
    llvm_scope_declare(ctx, owned_name, alloca, type);
    if (type_name != NULL) {
        llvm_register_typed_var_abi_binding(ctx, owned_name, alloca,
            type_name);
        if (llvm_lookup_class(ctx, type_name) != NULL)
            llvm_register_var_class(ctx, owned_name, type_name);
    } else {
        LLVMClassTypeEntry *class_entry =
            llvm_lookup_class_by_struct_type(ctx, type);
        if (class_entry != NULL && class_entry->class_name != NULL)
            llvm_register_var_class(ctx, owned_name,
                class_entry->class_name);
    }
}

static LLVMTypeRef
llvm_mir_local_type_from_assignment_target(const MIRRoutine *routine,
                                           LLVMGenCtx *ctx,
                                           ASTNode *expr,
                                           LLVMMirVar *vars,
                                           size_t var_count)
{
    ASTNode *target;
    const char *name;
    LLVMClassTypeEntry *host_cls;
    int field_idx;

    if (ctx == NULL || expr == NULL || expr->type != AST_ASSIGNMENT)
        return NULL;
    target = ast_assignment_target(expr);
    if (target == NULL || target->type != AST_IDENTIFIER)
        return NULL;
    name = ast_identifier_name(target);
    if (name == NULL)
        return NULL;

    LLVMTypeRef local_type =
        llvm_mir_local_type_from_vars(vars, var_count, name);
    if (local_type != NULL)
        return local_type;

    local_type = llvm_mir_local_type_from_source_fact(routine, ctx, name);
    if (ctx->has_error || local_type != NULL)
        return local_type;

    const char *host_name = llvm_current_host_class_name(ctx);
    if (host_name == NULL)
        host_name = mir_routine_owner_name(routine);
    host_cls = host_name != NULL ? llvm_lookup_class(ctx, host_name) : NULL;
    field_idx = host_cls != NULL ? llvm_class_field_index(host_cls, name) : -1;
    if (field_idx < 0)
        return NULL;
    return llvm_class_field_type_at_index(host_cls, field_idx);
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
    array_type = llvm_mir_slice_fact_array_type_from_slice_type(ctx,
        slice_type);
    if (array_type != NULL || slice_type == NULL || ctx == NULL
        || ctx->has_error) {
        return array_type;
    }

    llvm_set_mir_inventory_missing(ctx,
        "LLVM MIR SliceCopy result '%s' requires Slice<T> operand metadata",
        inst->result_name != NULL ? inst->result_name : "(anonymous-local)");
    return NULL;
}

static LLVMTypeRef
llvm_mir_local_type_from_slot_read_fact(const MIRRoutine *routine,
                                        LLVMGenCtx *ctx,
                                        const MIRInstruction *inst)
{
    char receiver_name[128];
    char inner_name[256];
    const char *receiver_type;

    if (routine == NULL || ctx == NULL || inst == NULL
        || inst->arg1 == NULL || strcmp(inst->arg1, "Read") != 0
        || inst->use_count == 0 || inst->uses == NULL
        || inst->uses[0] == NULL) {
        return NULL;
    }
    if (!llvm_mir_base_name_from_versioned(inst->uses[0],
            receiver_name, sizeof(receiver_name))) {
        size_t receiver_len = strlen(inst->uses[0]);
        if (receiver_len >= sizeof(receiver_name))
            return NULL;
        memcpy(receiver_name, inst->uses[0], receiver_len + 1);
    }
    receiver_type = mir_routine_source_local_type_name(routine,
        receiver_name);
    if (receiver_type == NULL
        || (!pgy_codegen_type_name_is_slot_family(receiver_type)
            && strncmp(receiver_type, "PinnedSlotView<", 15) != 0
            && strncmp(receiver_type, "PinnedSecureSlotView<", 21) != 0)) {
        return NULL;
    }
    if (!llvm_constructed_arg_name_copy(receiver_type, 0,
            inner_name, sizeof(inner_name))
        || inner_name[0] == '\0'
        || strcmp(inner_name, "Unknown") == 0) {
        return NULL;
    }
    return pergyra_type_to_llvm(ctx, inner_name);
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

    if (inst->requires_source_local_decl_emit && inst->expr1 != NULL) {
        type = llvm_mir_local_type_from_source_fact(routine, ctx,
            inst->result_name);
        if (ctx->has_error || type != NULL)
            return type;
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing source-local type metadata for '%s'",
            inst->result_name != NULL
                ? inst->result_name
                : "(anonymous-local)");
        return NULL;
    }

    type = llvm_mir_type_from_abi_layout(ctx, inst->type_layout);
    if (type != NULL)
        return type;

    type = llvm_mir_local_type_from_assignment_target(routine, ctx,
        inst->expr0, vars, var_count);
    if (type != NULL)
        return type;

    type = llvm_mir_local_type_from_slice_copy_fact(routine, ctx, inst,
        vars, var_count, depth);
    if (ctx->has_error || type != NULL)
        return type;

    type = llvm_mir_local_type_from_slot_read_fact(routine, ctx, inst);
    if (ctx->has_error || type != NULL)
        return type;

    type = llvm_mir_async_fact_type_from_channel_recv(routine, ctx, inst);
    if (ctx->has_error || type != NULL)
        return type;

    type = llvm_mir_async_fact_type_from_await(routine, ctx, inst);
    if (ctx->has_error || type != NULL)
        return type;

    type = llvm_mir_slice_fact_type_from_call(ctx, inst->expr0,
        vars, var_count);
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
        ASTNode *value_expr = llvm_mir_local_initializer_expr(inst->expr0);
        type = llvm_mir_local_infer_expr_type(routine, ctx, inst, NULL,
            value_expr);
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
                ASTNode *value_expr =
                    llvm_mir_local_initializer_expr(inst->expr0);
                ASTNode *type_expr = inst->requires_source_local_decl_emit
                    ? inst->expr1
                    : NULL;
                const MIRSourceLocalType *source_local_fact = NULL;
                const char *source_local_type_name = NULL;
                char base_name[128];
                bool has_base_name = llvm_mir_base_name_from_versioned(
                    inst->result_name, base_name, sizeof(base_name));

                if (inst->requires_source_local_decl_emit && type_expr != NULL) {
                    source_local_fact = llvm_mir_local_source_fact(
                        routine, has_base_name ? base_name : inst->result_name);
                    source_local_type_name = source_local_fact != NULL
                        ? source_local_fact->type_name
                        : NULL;
                    if (source_local_fact == NULL) {
                        llvm_set_mir_inventory_missing(ctx,
                            "MIR-only LLVM path missing source-local type metadata for '%s'",
                            inst->result_name != NULL
                                ? inst->result_name
                                : "(anonymous-local)");
                        return;
                    }
                }

                if (layout_type != NULL) {
                    alloca_type = layout_type;
                } else if (type_expr != NULL) {
                    alloca_type = llvm_mir_local_type_from_source_fact_entry(
                        ctx, source_local_fact);
                    if (ctx->has_error || alloca_type == NULL) {
                        llvm_set_mir_inventory_missing(ctx,
                            "MIR-only LLVM path cannot lower source-local type metadata for '%s'",
                            inst->result_name != NULL
                                ? inst->result_name
                                : "(anonymous-local)");
                        return;
                    }
                } else if (value_expr != NULL) {
                    if (inst->arg0 != NULL) {
                        alloca_type = llvm_mir_local_type_from_source_fact(
                            routine, ctx, inst->arg0);
                        if (ctx->has_error)
                            return;
                    }
                    if (alloca_type == NULL && has_base_name) {
                        alloca_type = llvm_mir_local_type_from_source_fact(
                            routine, ctx, base_name);
                        if (ctx->has_error)
                            return;
                    }
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
                    if (alloca_type == NULL) {
                        alloca_type = llvm_mir_local_infer_expr_type(routine,
                            ctx, inst, has_base_name ? base_name : NULL,
                            value_expr);
                    }
                    if (ctx->has_error || alloca_type == NULL)
                        return;
                } else if (has_base_name) {
                    alloca_type = llvm_mir_local_type_from_vars(
                        vars, var_count, base_name);
                    if (alloca_type == NULL) {
                        alloca_type = llvm_mir_local_type_from_source_fact(
                            routine, ctx, base_name);
                        if (ctx->has_error)
                            return;
                    }
                }
                if (alloca_type == NULL) {
                    alloca_type = llvm_mir_local_type_from_instruction_fact(
                        routine, ctx, inst, vars, var_count, 0);
                    if (ctx->has_error)
                        return;
                }
                if (alloca_type == NULL) {
                    alloca_type = llvm_mir_local_type_from_source_fact(
                        routine, ctx, inst->result_name);
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
                {
                    const char *expected_type_name =
                        llvm_mir_local_expected_type_name(routine, inst,
                            has_base_name ? base_name : NULL);
                    if (expected_type_name != NULL
                        && pgy_classify_type(expected_type_name)
                            == PGY_TK_CHANNEL) {
                        alloca_type = LLVMArrayType(
                            LLVMInt8TypeInContext(ctx->context), 256);
                    }
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
                /* Closure #70: pre-initialize host-field-aliased SSA locals
                 * from the host field so branch reads do not observe uninit
                 * stack before a prior SSA-DEF. */
                if (has_base_name && vars[var_count].alloca != NULL
                    && llvm_current_host_class_name(ctx) != NULL
                    && llvm_scope_contains(ctx, "self")) {
                    LLVMClassTypeEntry *_host_cls = llvm_lookup_class(ctx,
                        llvm_current_host_class_name(ctx));
                    if (_host_cls != NULL
                        && llvm_class_field_index(_host_cls, base_name) >= 0) {
                        llvm_mir_copy_host_field_to_versioned_local(ctx,
                            base_name, &vars[var_count]);
                    }
                }
                if (has_base_name && source_local_type_name != NULL) {
                    char *owned_base =
                        pgy_arena_strdup(&ctx->persistent, base_name);
                    if (owned_base == NULL) {
                        llvm_set_mir_topology_invalid(ctx,
                            "LLVM MIR typed local scope binding out of memory");
                        return;
                    }
                    llvm_scope_declare(ctx, owned_base,
                        vars[var_count].alloca, vars[var_count].type);
                    if (ctx->has_error)
                        return;
                    if (source_local_fact != NULL
                        && source_local_fact->is_callable) {
                        llvm_register_callable_signature_names(ctx, owned_base,
                            source_local_fact->callable_param_count,
                            (const char *const *)
                                source_local_fact->callable_param_type_names,
                            source_local_fact->callable_return_type_name);
                    } else {
                        llvm_register_typed_var_abi_binding(ctx, owned_base,
                            vars[var_count].alloca, source_local_type_name);
                    }
                    if (source_local_type_name != NULL
                        && llvm_lookup_class(ctx, source_local_type_name) != NULL)
                        llvm_register_var_class(ctx, owned_base,
                            source_local_type_name);
                } else if (has_base_name
                           && inst->abi_type_name != NULL) {
                    llvm_register_typed_var_abi_binding(ctx, base_name,
                        vars[var_count].alloca,
                        inst->abi_type_name);
                } else if (has_base_name
                           && mir_instruction_uses_source_local_decl_emit(inst)) {
                    llvm_mir_bind_source_local_base(routine, ctx, inst,
                        base_name, vars[var_count].alloca,
                        vars[var_count].type);
                    if (ctx->has_error)
                        return;
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
                        const char *source_type_name =
                            mir_routine_source_local_type_name(routine,
                                                               base_name);
                        if (source_type_name != NULL) {
                            elem_type =
                                llvm_mir_local_elem_type_from_type_name(
                                    ctx, source_type_name);
                        }
                        if (elem_type == NULL && type_expr != NULL) {
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
                    LLVMTypeRef elem_type =
                        llvm_mir_slice_fact_elem_type_from_receiver(ctx,
                            receiver, vars, var_count);
                    if (elem_type == NULL && type_expr != NULL)
                        elem_type = llvm_mir_local_elem_type_from_type_ast(
                            ctx, type_expr);
                    if (elem_type == NULL)
                        elem_type = llvm_mir_local_elem_type_from_layout(
                            ctx, inst->type_layout);
                    if (!llvm_mir_local_require_elem_type(ctx, value_expr,
                            elem_type, base_name))
                        return;
                    llvm_register_array_var_binding(ctx, base_name,
                        vars[var_count].alloca, elem_type, -1);
                }
                if (has_base_name) {
                    LLVMClassTypeEntry *value_cls = llvm_stmt_lookup_class_by_type(ctx, vars[var_count].type);
                    if (value_cls != NULL) {
                        llvm_register_var_class(ctx, base_name, value_cls->class_name);
                    }
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
