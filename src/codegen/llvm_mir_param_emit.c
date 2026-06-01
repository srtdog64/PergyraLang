/*
 * LLVM MIR parameter alloca emission.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_mir_local_emit.h"

#include <stdio.h>
#include <string.h>

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
            llvm_mir_register_nominal_class(ctx, p->name, p->type);
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
            LLVMTypeRef pt = NULL;
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
                llvm_mir_register_nominal_class(ctx, p->name, p->type);
            }
        }
    }
}

#endif
