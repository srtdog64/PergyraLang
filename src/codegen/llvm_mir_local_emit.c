/*
 * LLVM MIR local and parameter alloca emission.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_mir_local_emit.h"

#include <stdio.h>
#include <string.h>

#include "llvm_internal_api.h"
#include "llvm_mir_type_helpers.h"
#include "parser/ast_api.h"
#include "../common/string_compat.h"

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

static LLVMTypeRef
llvm_mir_local_type_from_value_fact(const MIRInstruction *inst,
                                    LLVMMirVar *vars,
                                    size_t var_count)
{
    ASTNode *value_expr;

    if (inst == NULL)
        return NULL;
    if (inst->use_count > 0 && inst->uses != NULL) {
        LLVMTypeRef use_type =
            llvm_mir_local_type_from_vars(vars, var_count, inst->uses[0]);
        if (use_type != NULL)
            return use_type;
    }

    value_expr = inst->expr0;
    if (value_expr != NULL
        && value_expr->type == AST_IDENTIFIER) {
        const char *value_name = ast_identifier_name(value_expr);
        if (value_name != NULL) {
            return llvm_mir_local_type_from_vars(vars, var_count, value_name);
        }
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
                LLVMTypeRef alloca_type = ctx->type_i32;
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
                    if (has_base_name)
                        llvm_register_typed_var(ctx, base_name, type_expr);
                } else if (value_expr != NULL) {
                    alloca_type = llvm_mir_local_type_from_value_fact(
                        inst, vars, var_count);
                    if (alloca_type == NULL)
                        alloca_type = llvm_stmt_infer_expr_type(ctx, value_expr);
                    if (ctx->has_error || alloca_type == NULL)
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
                if (has_base_name
                    && value_expr != NULL
                    && value_expr->type == AST_ARRAY_LITERAL) {
                    LLVMTypeRef elem_type = ctx->type_i32;
                    if (ast_array_literal_count(value_expr) > 0
                        && ast_array_literal_element(value_expr, 0) != NULL) {
                        elem_type = llvm_stmt_infer_expr_type(ctx,
                            ast_array_literal_element(value_expr, 0));
                    }
                    llvm_register_array_var(ctx, base_name,
                        elem_type, (int64_t)ast_array_literal_count(value_expr));
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
                    LLVMTypeRef elem_type = ctx->type_i32;
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
                    llvm_register_array_var(ctx, base_name,
                        elem_type, -1);
                }
                var_count++;
            }
        }
    }

    *vars_ptr = vars;
    *var_capacity_ptr = var_capacity;
    *var_count_ptr = var_count;
}

void
llvm_emit_mir_param_allocas(const MIRRoutine *routine, ASTNode *func_decl,
                            LLVMValueRef fn, LLVMGenCtx *ctx, bool is_intent,
                            bool is_method, LLVMClassTypeEntry *owner_cls,
                            const char *owner_name, size_t param_count)
{
    (void)routine;
    if (!is_intent) {
        size_t emitted_index = 0;
        if (is_method) {
            LLVMTypeRef self_type = owner_cls != NULL
                ? (owner_cls->is_pointer_self_host
                    ? LLVMPointerType(owner_cls->struct_type, 0)
                    : owner_cls->struct_type)
                : ctx->type_i8ptr;
            LLVMValueRef alloca = LLVMBuildAlloca(ctx->builder, self_type,
                (owner_cls != NULL && owner_cls->is_pointer_self_host)
                    ? "self.addr" : "self");
            LLVMBuildStore(ctx->builder, LLVMGetParam(fn, 0), alloca);
            llvm_scope_declare(ctx, "self", alloca, self_type);
            if (owner_name != NULL)
                llvm_register_var_class(ctx, "self", owner_name);
            emitted_index = 1;
        }

        size_t func_param_count = ast_func_param_count(func_decl);
        for (size_t param_index = 0; param_index < func_param_count;
             param_index++) {
            FuncParam *p = ast_func_param(func_decl, param_index);
            bool is_secure_slot = false;
            const char *slot_inner;
            LLVMTypeRef pt;
            LLVMValueRef alloca;

            if (p == NULL || (is_method && p->type == NULL && p->name != NULL
                && strcmp(p->name, "self") == 0)) {
                continue;
            }

            slot_inner = llvm_mir_boundary_slot_inner_name(ctx, p,
                &is_secure_slot);
            if (p->name == NULL) {
                emitted_index += (slot_inner != NULL && is_secure_slot) ? 2 : 1;
                continue;
            }

            pt = llvm_mir_required_type_from_ast(ctx, func_decl, p->type,
                "function parameter");
            if (ctx->has_error || pt == NULL)
                return;
            if (slot_inner != NULL) {
                LLVMTypeRef slot_ptr_ty = LLVMPointerType(pt, 0);
                alloca = LLVMBuildAlloca(ctx->builder, slot_ptr_ty, p->name);
                LLVMBuildStore(ctx->builder,
                    LLVMGetParam(fn, (unsigned)emitted_index++), alloca);
                llvm_scope_declare(ctx, p->name, alloca, slot_ptr_ty);
                llvm_register_typed_var(ctx, p->name, p->type);
                llvm_register_slot_var(ctx, p->name, slot_inner, is_secure_slot);
                if (is_secure_slot) {
                    char token_name[256];
                    LLVMTypeRef token_ty = llvm_secure_token_type(ctx, slot_inner);
                    LLVMValueRef token_alloca;
                    snprintf(token_name, sizeof(token_name), "%s_token", p->name);
                    token_alloca = LLVMBuildAlloca(ctx->builder, token_ty,
                        token_name);
                    LLVMBuildStore(ctx->builder,
                        LLVMGetParam(fn, (unsigned)emitted_index++),
                        token_alloca);
                    llvm_scope_declare(ctx, pergyra_strdup(token_name),
                        token_alloca, token_ty);
                }
                continue;
            }

            if (p->type != NULL && llvm_mir_param_uses_pointer_self(ctx, p->type))
                pt = LLVMPointerType(pt, 0);
            alloca = LLVMBuildAlloca(ctx->builder, pt, p->name);
            LLVMBuildStore(ctx->builder,
                LLVMGetParam(fn, (unsigned)emitted_index++), alloca);
            llvm_scope_declare(ctx, p->name, alloca, pt);
            llvm_register_typed_var(ctx, p->name, p->type);
        }
        return;
    }

    for (size_t i = 0; i < param_count; i++) {
        if (is_intent) {
            size_t binding_count = 0;
            size_t involve_count = 0;
            size_t value_count = 0;
            ASTNode **bindings = ast_intent_decl_bindings(func_decl, &binding_count);
            ASTNode **involves = ast_intent_decl_involves(func_decl, &involve_count);
            ASTNode **values = ast_intent_decl_values(func_decl, &value_count);
            ASTNode *binding = binding_count > 0
                ? (i < binding_count ? bindings[i] : NULL)
                : (i < involve_count
                    ? involves[i]
                    : (i - involve_count < value_count
                        ? values[i - involve_count]
                        : NULL));
            const char *alias = NULL;
            ASTNode *type_node = NULL;
            const char *type_name = NULL;
            LLVMTypeRef pt = ctx->type_i32;
            bool pointer_param = false;
            if (binding != NULL && binding->type == AST_INTENT_INVOLVES) {
                alias = ast_intent_involves_alias(binding);
                type_node = ast_intent_involves_subject_type(binding);
                pointer_param = llvm_intent_involves_uses_pointer_self(ctx,
                    binding);
            } else if (binding != NULL && binding->type == AST_INTENT_VALUE) {
                alias = ast_intent_value_alias(binding);
                type_node = ast_intent_value_type(binding);
            }
            if (type_node != NULL && type_node->type == AST_TYPE)
                type_name = ast_type_name(type_node);
            pt = llvm_mir_required_type_from_ast(ctx, binding, type_node,
                "intent binding");
            if (ctx->has_error || pt == NULL)
                return;
            if (pointer_param)
                pt = LLVMPointerType(pt, 0);
            LLVMValueRef alloca = LLVMBuildAlloca(ctx->builder, pt,
                alias != NULL ? alias : "binding");
            LLVMBuildStore(ctx->builder, LLVMGetParam(fn, (unsigned)i), alloca);
            llvm_scope_declare(ctx, alias != NULL ? alias : "binding", alloca,
                pt);
            if (type_node != NULL)
                llvm_register_typed_var(ctx, alias, type_node);
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
                    (owner_cls != NULL && owner_cls->is_pointer_self_host)
                        ? "self.addr" : "self");
                LLVMBuildStore(ctx->builder, LLVMGetParam(fn, 0), alloca);
                llvm_scope_declare(ctx, "self", alloca, self_type);
                if (owner_name != NULL)
                    llvm_register_var_class(ctx, "self", owner_name);
            } else {
                size_t logical_index = is_method ? (i - 1) : i;
                size_t seen = 0;
                FuncParam *p = NULL;
                size_t func_param_count = ast_func_param_count(func_decl);
                for (size_t param_index = 0; param_index < func_param_count;
                     param_index++) {
                    FuncParam *candidate = ast_func_param(func_decl,
                        param_index);
                    if (candidate != NULL
                        && candidate->type == NULL
                        && candidate->name != NULL
                        && strcmp(candidate->name, "self") == 0) {
                        continue;
                    }
                    if (seen == logical_index) {
                        p = candidate;
                        break;
                    }
                    seen++;
                }
                if (p == NULL || p->name == NULL)
                    continue;
                LLVMTypeRef pt = llvm_mir_required_type_from_ast(
                    ctx, func_decl, p->type, "function parameter");
                if (ctx->has_error || pt == NULL)
                    return;
                if (p->type != NULL && llvm_mir_param_uses_pointer_self(ctx,
                    p->type)) {
                    pt = LLVMPointerType(pt, 0);
                }
                LLVMValueRef alloca = LLVMBuildAlloca(ctx->builder, pt, p->name);
                LLVMBuildStore(ctx->builder, LLVMGetParam(fn, (unsigned)i),
                    alloca);
                llvm_scope_declare(ctx, p->name, alloca, pt);
                llvm_register_typed_var(ctx, p->name, p->type);
            }
        }
    }
}

#endif
