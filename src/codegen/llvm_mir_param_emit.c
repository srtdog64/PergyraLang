/*
 * LLVM MIR parameter alloca emission.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_mir_local_emit.h"

#include <stdio.h>
#include <string.h>

#include "llvm_intent_internal.h"
#include "llvm_boundary_slot_param.h"
#include "llvm_internal_api.h"
#include "llvm_mir_type_helpers.h"
#include "parser/ast_api.h"
#include "../common/string_compat.h"

void
llvm_emit_mir_param_allocas(const MIRRoutine *routine, ASTNode *func_decl,
                            LLVMValueRef fn, LLVMGenCtx *ctx, bool is_intent,
                            bool is_method, LLVMClassTypeEntry *owner_cls,
                            const char *owner_name, size_t param_count)
{
    bool owner_is_role = is_method && routine != NULL
        && routine->owner_ast_type == AST_ROLE_DECL;
    const char **participant_aliases = NULL;
    const char **participant_types = NULL;
    const char **value_aliases = NULL;
    const char **value_types = NULL;
    size_t participant_count = 0;
    size_t mir_value_count = 0;
    size_t intent_involve_count = 0;
    size_t intent_value_count = 0;
    if (is_intent && routine != NULL && func_decl != NULL) {
        intent_involve_count = ast_intent_decl_involve_count(func_decl);
        intent_value_count = ast_intent_decl_value_count(func_decl);
        participant_count = llvm_collect_mir_intent_participants(
            routine, ctx, &participant_aliases, &participant_types);
        mir_value_count = llvm_collect_mir_intent_values(
            routine, ctx, &value_aliases, &value_types);
        if (intent_involve_count > 0 && participant_count < intent_involve_count) {
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path missing intent participant metadata for '%s'",
                routine->name != NULL ? routine->name : "(anonymous)");
            return;
        }
        if (intent_value_count > 0 && mir_value_count < intent_value_count) {
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path missing intent value metadata for '%s'",
                routine->name != NULL ? routine->name : "(anonymous)");
            return;
        }
    }
    if (!is_intent) {
        size_t emitted_index = 0;
        if (is_method) {
            LLVMTypeRef self_type = owner_is_role
                ? ctx->type_i8ptr
                : (owner_cls != NULL
                ? (owner_cls->is_pointer_self_host
                    ? LLVMPointerType(owner_cls->struct_type, 0)
                    : owner_cls->struct_type)
                : ctx->type_i8ptr);
            LLVMValueRef alloca = LLVMBuildAlloca(ctx->builder, self_type,
                (owner_cls != NULL && owner_cls->is_pointer_self_host
                 && !owner_is_role)
                    ? "self.addr" : "self");
            LLVMBuildStore(ctx->builder, LLVMGetParam(fn, 0), alloca);
            llvm_scope_declare(ctx, "self", alloca, self_type);
            if (owner_name != NULL)
                llvm_register_var_class(ctx, "self", owner_name);
            emitted_index = 1;
        }

        size_t func_param_count = llvm_mir_routine_has_signature(routine)
            ? llvm_mir_routine_param_count(routine)
            : ast_func_param_count(func_decl);
        for (size_t param_index = 0; param_index < func_param_count;
             param_index++) {
            FuncParam *p = llvm_mir_routine_has_signature(routine)
                ? llvm_mir_routine_param(routine, param_index)
                : ast_func_param(func_decl, param_index);
            bool is_secure_slot = false;
            const char *slot_inner;
            LLVMTypeRef pt;
            LLVMValueRef alloca;
            const char *param_type_name =
                llvm_mir_routine_has_signature(routine)
                    ? llvm_mir_routine_param_type_name(routine, param_index)
                    : NULL;

            if (p == NULL || (is_method && llvm_param_is_implicit_self_local(p))) {
                continue;
            }

            slot_inner = param_type_name != NULL
                ? llvm_boundary_slot_inner_name_from_type_name(ctx,
                    p,
                    param_type_name,
                    &is_secure_slot)
                : llvm_mir_boundary_slot_inner_name(ctx, p, &is_secure_slot);
            if (p->name == NULL) {
                emitted_index += (slot_inner != NULL && is_secure_slot) ? 2 : 1;
                continue;
            }

            pt = param_type_name != NULL
                ? pergyra_type_to_llvm(ctx, param_type_name)
                : llvm_mir_required_type_from_ast(ctx, func_decl, p->type,
                    "function parameter");
            if (ctx->has_error || pt == NULL)
                return;
            if (slot_inner != NULL) {
                LLVMTypeRef slot_ptr_ty = LLVMPointerType(pt, 0);
                alloca = LLVMBuildAlloca(ctx->builder, slot_ptr_ty, p->name);
                LLVMBuildStore(ctx->builder,
                    LLVMGetParam(fn, (unsigned)emitted_index++), alloca);
                llvm_scope_declare(ctx, p->name, alloca, slot_ptr_ty);
                if (param_type_name != NULL)
                    llvm_register_typed_var_abi_binding(ctx, p->name, alloca,
                        param_type_name);
                else
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

            if (param_type_name != NULL
                ? llvm_type_name_uses_pointer_self(ctx, param_type_name)
                : (p->type != NULL
                    && llvm_mir_param_uses_pointer_self(ctx, p->type)))
                pt = LLVMPointerType(pt, 0);
            alloca = LLVMBuildAlloca(ctx->builder, pt, p->name);
            LLVMBuildStore(ctx->builder,
                LLVMGetParam(fn, (unsigned)emitted_index++), alloca);
            llvm_scope_declare(ctx, p->name, alloca, pt);
            if (param_type_name != NULL)
                llvm_register_typed_var_abi_binding(ctx, p->name, alloca,
                    param_type_name);
            else
                llvm_register_typed_var(ctx, p->name, p->type);
            if (param_type_name != NULL)
                llvm_register_var_class(ctx, p->name, param_type_name);
            else
                llvm_mir_register_nominal_class(ctx, p->name, p->type);
        }
        return;
    }

    for (size_t i = 0, participant_index = 0, value_index = 0;
         i < param_count; i++) {
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
            LLVMTypeRef pt = NULL;
            bool pointer_param = false;
            if (binding != NULL && binding->type == AST_INTENT_INVOLVES) {
                const char *participant_type =
                    participant_types != NULL && participant_index < participant_count
                        ? participant_types[participant_index]
                        : NULL;
                alias = (participant_aliases != NULL
                         && participant_index < participant_count
                         && participant_aliases[participant_index] != NULL)
                    ? participant_aliases[participant_index]
                    : ast_intent_involves_alias(binding);
                if (participant_type != NULL) {
                    type_name = participant_type;
                    pt = pergyra_type_to_llvm(ctx, participant_type);
                    if (ctx->has_error || pt == NULL)
                        return;
                    pointer_param = llvm_type_name_uses_pointer_self(
                        ctx, participant_type);
                } else {
                    type_node = ast_intent_involves_subject_type(binding);
                    pointer_param = llvm_intent_involves_uses_pointer_self(ctx,
                        binding);
                }
                participant_index++;
            } else if (binding != NULL && binding->type == AST_INTENT_VALUE) {
                const char *value_type =
                    value_types != NULL && value_index < mir_value_count
                        ? value_types[value_index]
                        : NULL;
                alias = (value_aliases != NULL && value_index < mir_value_count
                         && value_aliases[value_index] != NULL)
                    ? value_aliases[value_index]
                    : ast_intent_value_alias(binding);
                if (value_type != NULL) {
                    type_name = value_type;
                    pt = pergyra_type_to_llvm(ctx, value_type);
                    if (ctx->has_error || pt == NULL)
                        return;
                } else {
                    type_node = ast_intent_value_type(binding);
                }
                value_index++;
            }
            if (type_node != NULL && type_node->type == AST_TYPE)
                type_name = ast_type_name(type_node);
            if (pt == NULL) {
                pt = llvm_mir_required_type_from_ast(ctx, binding, type_node,
                    "intent binding");
                if (ctx->has_error || pt == NULL)
                    return;
            }
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
                LLVMTypeRef self_type = owner_is_role
                    ? ctx->type_i8ptr
                    : (owner_cls != NULL
                    ? (owner_cls->is_pointer_self_host
                        ? LLVMPointerType(owner_cls->struct_type, 0)
                        : owner_cls->struct_type)
                    : ctx->type_i8ptr);
                LLVMValueRef alloca = LLVMBuildAlloca(ctx->builder, self_type,
                    (owner_cls != NULL && owner_cls->is_pointer_self_host
                     && !owner_is_role)
                        ? "self.addr" : "self");
                LLVMBuildStore(ctx->builder, LLVMGetParam(fn, 0), alloca);
                llvm_scope_declare(ctx, "self", alloca, self_type);
                if (owner_name != NULL)
                    llvm_register_var_class(ctx, "self", owner_name);
            } else {
                size_t logical_index = is_method ? (i - 1) : i;
                size_t seen = 0;
                FuncParam *p = NULL;
                size_t source_param_index = (size_t)-1;
                size_t func_param_count = llvm_mir_routine_has_signature(routine)
                    ? llvm_mir_routine_param_count(routine)
                    : ast_func_param_count(func_decl);
                for (size_t param_index = 0; param_index < func_param_count;
                     param_index++) {
                    FuncParam *candidate =
                        llvm_mir_routine_has_signature(routine)
                            ? llvm_mir_routine_param(routine, param_index)
                            : ast_func_param(func_decl, param_index);
                    if (llvm_param_is_implicit_self_local(candidate)) {
                        continue;
                    }
                    if (seen == logical_index) {
                        p = candidate;
                        source_param_index = param_index;
                        break;
                    }
                    seen++;
                }
                if (p == NULL || p->name == NULL)
                    continue;
                const char *param_type_name =
                    source_param_index != (size_t)-1
                        && llvm_mir_routine_has_signature(routine)
                        ? llvm_mir_routine_param_type_name(routine,
                            source_param_index)
                        : NULL;
                LLVMTypeRef pt = param_type_name != NULL
                    ? pergyra_type_to_llvm(ctx, param_type_name)
                    : llvm_mir_required_type_from_ast(
                        ctx, func_decl, p->type, "function parameter");
                if (ctx->has_error || pt == NULL)
                    return;
                if (param_type_name != NULL
                    ? llvm_type_name_uses_pointer_self(ctx, param_type_name)
                    : (p->type != NULL && llvm_mir_param_uses_pointer_self(ctx,
                        p->type))) {
                    pt = LLVMPointerType(pt, 0);
                }
                LLVMValueRef alloca = LLVMBuildAlloca(ctx->builder, pt, p->name);
                LLVMBuildStore(ctx->builder, LLVMGetParam(fn, (unsigned)i),
                    alloca);
                llvm_scope_declare(ctx, p->name, alloca, pt);
                if (param_type_name != NULL)
                    llvm_register_typed_var_abi_binding(ctx, p->name, alloca,
                        param_type_name);
                else
                    llvm_register_typed_var(ctx, p->name, p->type);
                if (param_type_name != NULL)
                    llvm_register_var_class(ctx, p->name, param_type_name);
                else
                    llvm_mir_register_nominal_class(ctx, p->name, p->type);
            }
        }
    }
}

#endif
