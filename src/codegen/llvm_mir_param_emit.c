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
#include "llvm_mir_signature.h"
#include "llvm_mir_type_helpers.h"
#include "llvm_domain_role_helpers.h"
#include "llvm_inventory_decl_lookup.h"
#include "parser/ast_api.h"
#include "../common/string_compat.h"

/* The var-class for `self` is the host class for plain methods, but for a role
 * method (`role R for Subject`) it must be the subject so nominal inference of
 * `self.field...` resolves. Mirrors the C backend's owner_role_subject_name. */
static const char *
llvm_mir_self_var_class(LLVMGenCtx *ctx, const char *owner_name,
                        bool owner_is_role)
{
    if (owner_name == NULL)
        return NULL;
    if (owner_is_role) {
        ASTNode *role_decl =
            llvm_find_host_decl_in_active_inventory(ctx, owner_name);
        const char *subject = role_decl != NULL
            ? llvm_role_for_type_name(role_decl) : NULL;
        if (subject != NULL)
            return subject;
    }
    return owner_name;
}

static void
llvm_register_callable_param_if_needed(LLVMGenCtx *ctx, FuncParam *param)
{
    if (ctx == NULL || param == NULL || param->name == NULL
        || param->type == NULL || param->type->type != AST_EVENT_HANDLER_TYPE)
        return;
    if (llvm_lookup_callable_entry(ctx, param->name) != NULL)
        return;
    llvm_register_callable_var(ctx, param->name, param->type);
}

void
llvm_emit_mir_param_allocas(const MIRRoutine *routine, ASTNode *func_decl,
                            LLVMValueRef fn, LLVMGenCtx *ctx, bool is_intent,
                            bool is_method, LLVMClassTypeEntry *owner_cls,
                            const char *owner_name, size_t param_count)
{
    const char *routine_name = llvm_mir_routine_name(routine);
    bool owner_is_role = is_method && routine != NULL
        && llvm_mir_routine_owner_ast_type(routine) == AST_ROLE_DECL;
    IntentBindingMetadataView binding_metadata = {0};
    size_t mir_binding_count = 0;

    if (ctx != NULL && llvm_active_has_mir(ctx) && routine == NULL) {
        const char *decl_name = func_decl != NULL
            ? ast_declaration_name(func_decl)
            : NULL;
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing function parameter routine for '%s'",
            decl_name != NULL ? decl_name : "(anonymous)");
        return;
    }

    if (is_intent && routine != NULL && func_decl != NULL) {
        mir_binding_count = llvm_collect_mir_intent_bindings(
            routine, ctx, &binding_metadata);
        if (param_count > 0 && mir_binding_count != param_count) {
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path missing ordered intent binding metadata for '%s'",
                routine_name != NULL ? routine_name : "(anonymous)");
            return;
        }
        for (size_t i = 0; i < mir_binding_count; i++) {
            if (!intent_binding_metadata_view_has_complete_row(
                    &binding_metadata, i)) {
                llvm_set_mir_inventory_missing(ctx,
                    "MIR-only LLVM path has incomplete ordered intent binding metadata for '%s'",
                    routine_name != NULL ? routine_name : "(anonymous)");
                return;
            }
        }
    }
    if (!is_intent
        && !llvm_mir_routine_signature_metadata_complete_for(ctx,
            routine,
            func_decl,
            LLVM_MIR_SIGNATURE_REQUIRE_PARAM_TYPE_NAMES,
            "MIR-only LLVM path missing function parameter signature metadata for '%s'",
            NULL,
            "MIR-only LLVM path missing function parameter type-name metadata for '%s'")) {
        return;
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
            {
                const char *self_class = llvm_mir_self_var_class(
                    ctx, owner_name, owner_is_role);
                if (self_class != NULL)
                    llvm_register_var_class(ctx, "self", self_class);
            }
            emitted_index = 1;
        }

        size_t func_param_count = llvm_mir_routine_param_count(routine);
        for (size_t param_index = 0; param_index < func_param_count;
             param_index++) {
            FuncParam *p = llvm_mir_routine_param(routine, param_index);
            bool is_secure_slot = false;
            const char *slot_inner;
            LLVMTypeRef pt;
            LLVMValueRef alloca;
            const char *param_type_name =
                llvm_mir_routine_param_type_name(routine, param_index);

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
            llvm_register_callable_param_if_needed(ctx, p);
            if (param_type_name != NULL)
                llvm_register_var_class(ctx, p->name, param_type_name);
            else
                llvm_mir_register_nominal_class(ctx, p->name, p->type);
        }
        return;
    }

    for (size_t i = 0; i < param_count; i++) {
        if (is_intent) {
            const char *kind =
                intent_binding_metadata_view_kind_at(&binding_metadata, i);
            const char *alias = NULL;
            const char *type_name = NULL;
            LLVMTypeRef pt = NULL;
            bool pointer_param = false;
            if (kind != NULL && strcmp(kind, "participant") == 0) {
                alias = intent_binding_metadata_view_alias_at(
                    &binding_metadata, i);
                type_name = intent_binding_metadata_view_type_at(
                    &binding_metadata, i);
                if (type_name != NULL) {
                    pt = pergyra_type_to_llvm(ctx, type_name);
                    if (ctx->has_error || pt == NULL)
                        return;
                    pointer_param = llvm_type_name_uses_pointer_self(
                        ctx, type_name);
                } else {
                    llvm_set_mir_inventory_missing(ctx,
                        "MIR-only LLVM path missing intent participant type metadata for '%s'",
                        routine_name != NULL ? routine_name : "(anonymous)");
                    return;
                }
            } else if (kind != NULL && strcmp(kind, "value") == 0) {
                alias = intent_binding_metadata_view_alias_at(
                    &binding_metadata, i);
                type_name = intent_binding_metadata_view_type_at(
                    &binding_metadata, i);
                if (type_name != NULL) {
                    pt = pergyra_type_to_llvm(ctx, type_name);
                    if (ctx->has_error || pt == NULL)
                        return;
                } else {
                    llvm_set_mir_inventory_missing(ctx,
                        "MIR-only LLVM path missing intent value type metadata for '%s'",
                        routine_name != NULL ? routine_name : "(anonymous)");
                    return;
                }
            } else {
                llvm_set_mir_inventory_missing(ctx,
                    "MIR-only LLVM path missing intent parameter metadata for '%s'",
                    routine_name != NULL ? routine_name : "(anonymous)");
                return;
            }
            if (pt == NULL) {
                llvm_set_mir_inventory_missing(ctx,
                    "MIR-only LLVM path missing intent parameter type metadata for '%s'",
                    routine_name != NULL ? routine_name : "(anonymous)");
                return;
            }
            if (pointer_param)
                pt = LLVMPointerType(pt, 0);
            LLVMValueRef alloca = LLVMBuildAlloca(ctx->builder, pt,
                alias != NULL ? alias : "binding");
            LLVMBuildStore(ctx->builder, LLVMGetParam(fn, (unsigned)i), alloca);
            llvm_scope_declare(ctx, alias != NULL ? alias : "binding", alloca,
                pt);
            if (type_name != NULL && alias != NULL)
                llvm_register_typed_var_abi_binding(ctx, alias, alloca,
                    type_name);
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
                {
                    const char *self_class = llvm_mir_self_var_class(
                        ctx, owner_name, owner_is_role);
                    if (self_class != NULL)
                        llvm_register_var_class(ctx, "self", self_class);
                }
            } else {
                size_t logical_index = is_method ? (i - 1) : i;
                size_t seen = 0;
                FuncParam *p = NULL;
                size_t source_param_index = (size_t)-1;
                size_t func_param_count =
                    llvm_mir_routine_param_count(routine);
                for (size_t param_index = 0; param_index < func_param_count;
                     param_index++) {
                    FuncParam *candidate =
                        llvm_mir_routine_param(routine, param_index);
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
                llvm_register_callable_param_if_needed(ctx, p);
                if (param_type_name != NULL)
                    llvm_register_var_class(ctx, p->name, param_type_name);
                else
                    llvm_mir_register_nominal_class(ctx, p->name, p->type);
            }
        }
    }
}

#endif
