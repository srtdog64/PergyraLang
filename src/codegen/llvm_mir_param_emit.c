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
#include "llvm_inventory_decl_lookup.h"
#include "llvm_domain_role_helpers.h"
#include "llvm_inventory_decl_lookup.h"
#include "codegen_slot_type_policy.h"
#include "llvm_backend_type_map_internal.h"
#include "parser/ast_api.h"
#include "codegen_type_mapping.h"
#include "../compiler/mir_decl_headers.h"
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
            ? llvm_role_for_type_name(ctx, role_decl) : NULL;
        if (subject != NULL)
            return subject;
    }
    return owner_name;
}

static const char *
llvm_field_slot_paired_token(const LLVMHostedFieldView *field_view,
                             ASTNode *host,
                             const char *slot_name)
{
    const MIRDeclHeader *header;
    size_t group_count = ast_class_field_destructure_count(host);

    header = field_view != NULL ? field_view->decl_header : NULL;
    for (size_t i = 0; header != NULL
         && i < mir_decl_header_field_claim_count(header); i++) {
        const MIRDeclFieldClaim *claim = mir_decl_header_field_claim(header, i);
        const char *claim_slot = mir_decl_field_claim_slot_name(claim);
        if (claim_slot != NULL && slot_name != NULL
            && strcmp(claim_slot, slot_name) == 0) {
            return mir_decl_field_claim_token_name(claim);
        }
    }
    if (field_view != NULL && field_view->requires_mir_metadata)
        return NULL;

    for (size_t gi = 0; gi < group_count; gi++)
    {
        ASTNode *group = ast_class_field_destructure_at(host, gi);
        if (group == NULL || ast_let_destructure_name_count(group) < 2)
            continue;
        if (ast_let_destructure_name(group, 0) == NULL
            || strcmp(ast_let_destructure_name(group, 0), slot_name) != 0)
            continue;
        return ast_let_destructure_name(group, 1);
    }
    return NULL;
}

static void
llvm_register_field_token(LLVMGenCtx *ctx, ASTNode *host,
                          LLVMClassTypeEntry *cls, LLVMValueRef self_base,
                          const LLVMHostedFieldView *field_view,
                          const char *slot_name, const char *inner)
{
    const char *token_field =
        llvm_field_slot_paired_token(field_view, host, slot_name);
    int tidx;
    LLVMValueRef tgep;
    char tname[256];

    if (token_field == NULL)
        return;
    tidx = llvm_class_field_index(cls, token_field);
    if (tidx < 0)
        return;
    tgep = LLVMBuildStructGEP2(ctx->builder, cls->struct_type, self_base,
        (unsigned)tidx, token_field);
    snprintf(tname, sizeof(tname), "%s_token", slot_name);
    llvm_scope_declare(ctx, pergyra_strdup(tname), tgep,
        llvm_secure_token_type(ctx, inner));
}

static void
llvm_register_one_field_slot(LLVMGenCtx *ctx, ASTNode *host,
                             LLVMClassTypeEntry *cls, LLVMValueRef self_base,
                             const LLVMHostedFieldView *field_view,
                             size_t field_index)
{
    const MIRDeclField *field_meta =
        llvm_hosted_field_view_metadata(field_view, field_index);
    const char *field_name =
        llvm_hosted_field_view_name(field_view, field_index);
    const char *type_name = field_meta != NULL
        ? llvm_mir_decl_field_type_name(field_meta) : NULL;
    ASTNode *field_type = NULL;
    char *owned_type_name = NULL;
    const char *inner;
    char inner_buf[128];
    bool is_secure;
    int idx;
    LLVMValueRef gep;
    LLVMTypeRef slot_ty;

    if (field_name == NULL)
        return;
    if (type_name == NULL || type_name[0] == '\0') {
        if (llvm_active_has_mir(ctx)) {
            const char *host_name = host != NULL
                ? llvm_decl_node_name(host) : NULL;
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path missing class field type-name metadata for '%s' index %zu",
                host_name != NULL ? host_name : "(anonymous-host)",
                field_index);
            return;
        }
        field_type = llvm_hosted_field_view_type(field_view, field_index);
        if (field_type != NULL) {
            owned_type_name = llvm_render_type_name_in_ctx(ctx, field_type);
            type_name = owned_type_name;
        }
    }
    if (!pgy_codegen_type_name_is_slot(type_name)
        && !pgy_codegen_type_name_is_secure_slot(type_name)) {
        free(owned_type_name);
        return;
    }

    is_secure = pgy_codegen_type_name_is_secure_slot(type_name);
    if (!llvm_constructed_arg_name_copy(type_name, 0,
            inner_buf, sizeof(inner_buf))) {
        pergyra_str_copy(inner_buf, sizeof(inner_buf), "Int");
    }
    inner = inner_buf;
    idx = llvm_class_field_index(cls, field_name);
    if (idx < 0) {
        free(owned_type_name);
        return;
    }

    gep = LLVMBuildStructGEP2(ctx->builder, cls->struct_type, self_base,
        (unsigned)idx, field_name);
    slot_ty = is_secure ? llvm_secure_slot_struct_type(ctx, inner)
                        : llvm_slot_struct_type(ctx, inner);
    llvm_scope_declare(ctx, field_name, gep, slot_ty);
    llvm_register_slot_var_binding(ctx, field_name, gep, inner, is_secure);
    if (is_secure)
        llvm_register_field_token(ctx, host, cls, self_base, field_view,
            field_name, inner);
    free(owned_type_name);
}

/* Mirror of the C backend register_class_field_slots: bind each owning slot
 * field of the method's class as a `self->field` GEP so slot ops resolve their
 * inner type and address through self instead of a local alloca. */
void
llvm_register_class_field_slots(LLVMGenCtx *ctx, const char *owner_name)
{
    ASTNode *host;
    LLVMClassTypeEntry *cls;
    LLVMValueRef self_base;
    LLVMHostedFieldView fields_view;

    if (ctx == NULL || owner_name == NULL)
        return;
    host = llvm_find_host_decl_in_active_inventory(ctx, owner_name);
    if (host == NULL || host->type != AST_CLASS_DECL)
        return;
    cls = llvm_lookup_class(ctx, owner_name);
    if (cls == NULL)
        return;
    self_base = llvm_current_self_base_ptr(ctx, cls);
    if (self_base == NULL)
        return;

    /* Route class-field access through the declaration inventory field view
     * instead of reopening the field array directly (declaration
     * source-of-truth). */
    fields_view = llvm_hosted_class_field_view_from_decl(ctx, owner_name, host);
    if (llvm_hosted_field_view_missing_mir_metadata(&fields_view)) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing class-field slot registration metadata for '%s'",
            owner_name != NULL ? owner_name : "<class>");
        return;
    }
    for (size_t i = 0; i < fields_view.count; i++)
        llvm_register_one_field_slot(ctx, host, cls, self_base,
            &fields_view, i);
}

bool
llvm_emit_mir_mut_ref_writebacks(const MIRRoutine *routine,
                                 const MIRBasicBlock *block,
                                 LLVMMirVar *vars,
                                 size_t var_count,
                                 LLVMGenCtx *ctx)
{
    int mut_ref_index = 0;

    if (routine == NULL || block == NULL || ctx == NULL)
        return false;

    for (size_t i = 0; i < mir_routine_param_count(routine); i++) {
        FuncParam *param;
        const char *exit_name = NULL;
        LLVMMirVar *exit_var = NULL;

        if (mir_routine_param_carriage(routine, i)
            != MIR_PARAM_CARRIAGE_VALUE_RESULT) {
            continue;
        }
        param = mir_routine_param(routine, i);
        if (param == NULL || param->name == NULL) {
            llvm_set_mir_inventory_missing(ctx,
                "LLVM MIR value-result parameter is missing its identity");
            return false;
        }
        if (mut_ref_index >= ctx->mut_ref_count) {
            llvm_set_mir_inventory_missing(ctx,
                "LLVM MIR value-result parameter '%s' has no registered copy-out boundary",
                param->name);
            return false;
        }

        for (size_t j = 0; j < block->ssa_exit_value_count; j++) {
            const char *candidate = block->ssa_exit_values[j];
            const char *separator;
            size_t base_len;

            if (candidate == NULL)
                continue;
            separator = strrchr(candidate, '.');
            base_len = separator != NULL
                ? (size_t)(separator - candidate)
                : strlen(candidate);
            if (strlen(param->name) != base_len
                || strncmp(candidate, param->name, base_len) != 0) {
                continue;
            }
            if (exit_name != NULL) {
                llvm_set_mir_topology_invalid(ctx,
                    "LLVM MIR block %llu has duplicate exit SSA facts for value-result parameter '%s'",
                    (unsigned long long)block->id,
                    param->name);
                return false;
            }
            exit_name = candidate;
        }

        if (exit_name != NULL) {
            exit_var = llvm_mir_get_var_entry(vars, var_count, exit_name);
            if (exit_var == NULL || exit_var->alloca == NULL
                || exit_var->type == NULL) {
                llvm_set_mir_inventory_missing(ctx,
                    "LLVM MIR value-result exit identity '%s' has no storage fact",
                    exit_name);
                return false;
            }
            if (exit_var->type != ctx->mut_ref_pt[mut_ref_index]) {
                llvm_set_mir_inventory_missing(ctx,
                    "LLVM MIR value-result exit identity '%s' disagrees with parameter ABI type",
                    exit_name);
                return false;
            }
        }

        /* An absent exit SSA row denotes version zero: this path did not
         * rebind the parameter, so its registered copy-in storage is the
         * current value.  A present row is authoritative and must resolve. */
        LLVMValueRef storage = exit_var != NULL
            ? exit_var->alloca
            : ctx->mut_ref_alloca[mut_ref_index];
        LLVMValueRef value = LLVMBuildLoad2(ctx->builder,
            ctx->mut_ref_pt[mut_ref_index], storage, "");
        LLVMBuildStore(ctx->builder, value,
            ctx->mut_ref_ptr[mut_ref_index]);
        mut_ref_index++;
    }

    if (mut_ref_index != ctx->mut_ref_count) {
        llvm_set_mir_inventory_missing(ctx,
            "LLVM MIR value-result parameter inventory disagrees with registered copy-out boundaries");
        return false;
    }
    return true;
}

void
llvm_register_mir_param_ssa_aliases(const MIRRoutine *routine,
                                    LLVMGenCtx *ctx,
                                    LLVMMirVar **vars_ptr,
                                    size_t *var_capacity_ptr,
                                    size_t *var_count_ptr)
{
    LLVMMirVar *vars;
    size_t capacity;
    size_t count;

    if (routine == NULL || ctx == NULL || vars_ptr == NULL
        || var_capacity_ptr == NULL || var_count_ptr == NULL)
        return;
    vars = *vars_ptr;
    capacity = *var_capacity_ptr;
    count = *var_count_ptr;

    for (size_t i = 0; i < mir_routine_param_count(routine); i++) {
        FuncParam *param = mir_routine_param(routine, i);
        LLVMVarEntry entry;
        char candidate[256];
        int written;
        char *owned_name;

        if (param == NULL || param->name == NULL)
            continue;
        written = snprintf(candidate, sizeof(candidate), "%s.0", param->name);
        if (written <= 0 || (size_t)written >= sizeof(candidate)) {
            llvm_set_mir_inventory_missing(ctx,
                "LLVM MIR parameter SSA identity is too long for '%s'",
                param->name);
            return;
        }
        if (llvm_mir_get_var_entry(vars, count, candidate) != NULL) {
            continue;
        }
        if (!llvm_scope_lookup_snapshot(ctx, param->name, &entry)
            || entry.alloca == NULL || entry.type == NULL) {
            llvm_set_mir_inventory_missing(ctx,
                "LLVM MIR parameter identity '%s' has no parameter storage",
                candidate);
            return;
        }
        if (count >= capacity) {
            size_t next_capacity = capacity > 0 ? capacity * 2 : 64;
            LLVMMirVar *grown = pgy_arena_calloc(&ctx->scratch,
                next_capacity * sizeof(LLVMMirVar));
            if (grown == NULL) {
                llvm_set_mir_memory_exhausted(ctx,
                    "LLVM MIR parameter SSA registry allocation failed");
                return;
            }
            if (vars != NULL && count > 0)
                memcpy(grown, vars, count * sizeof(LLVMMirVar));
            vars = grown;
            capacity = next_capacity;
        }
        owned_name = pgy_arena_strdup(&ctx->scratch, candidate);
        if (owned_name == NULL) {
            llvm_set_mir_memory_exhausted(ctx,
                "LLVM MIR parameter SSA identity allocation failed");
            return;
        }
        vars[count].mir_name = owned_name;
        vars[count].abi_type_name = mir_routine_param_type_name(routine, i);
        vars[count].alloca = entry.alloca;
        vars[count].type = entry.type;
        count++;
    }

    *vars_ptr = vars;
    *var_capacity_ptr = capacity;
    *var_count_ptr = count;
}

void
llvm_emit_mir_param_allocas(const MIRRoutine *routine, ASTNode *func_decl,
                            LLVMValueRef fn, LLVMGenCtx *ctx, bool is_intent,
                            bool is_method, LLVMClassTypeEntry *owner_cls,
                            const char *owner_name, size_t param_count)
{
    const char *routine_name = llvm_mir_routine_name(routine);
    if (ctx != NULL)
        ctx->mut_ref_count = 0;
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

    if (is_intent && routine != NULL) {
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
            MIRParamResourceKind resource_kind = MIR_PARAM_RESOURCE_NONE;
            const char *slot_inner;
            LLVMTypeRef pt;
            LLVMValueRef alloca;
            const char *param_type_name =
                llvm_mir_routine_param_type_name(routine, param_index);
            const MIRCallableSig *param_callable_sig =
                llvm_mir_routine_param_callable_sig(routine, param_index);
            MIRParamCarriage carriage =
                llvm_mir_routine_param_carriage(routine, param_index);
            bool pass_indirect =
                llvm_mir_routine_param_passes_indirect(routine, param_index);

            if (p == NULL || (is_method && llvm_param_is_implicit_self_local(p))) {
                continue;
            }
            slot_inner = llvm_mir_boundary_resource_inner_name(
                ctx, routine, param_index, &resource_kind);
            if (p->name == NULL) {
                emitted_index +=
                    (slot_inner != NULL
                     && resource_kind == MIR_PARAM_RESOURCE_SECURE_SLOT)
                        ? 2
                        : 1;
                continue;
            }

            pt = param_callable_sig != NULL
                ? llvm_mir_callable_sig_to_llvm(ctx, param_callable_sig)
                : param_type_name != NULL
                ? pergyra_type_to_llvm(ctx, param_type_name)
                : NULL;
            if (pt == NULL && !ctx->has_error)
                llvm_set_mir_inventory_missing(ctx,
                    "MIR-only LLVM path missing parameter ABI type fact for '%s'",
                    p->name);
            if (ctx->has_error || pt == NULL)
                return;
            if (slot_inner != NULL) {
                if (resource_kind == MIR_PARAM_RESOURCE_DEVICE_SLOT) {
                    alloca = LLVMGetParam(fn, (unsigned)emitted_index++);
                    llvm_scope_declare(ctx, p->name, alloca, pt);
                    llvm_register_typed_var_abi_binding(ctx, p->name, alloca,
                        param_type_name);
                    llvm_register_device_slot_var_binding(ctx, p->name,
                        alloca, slot_inner);
                    continue;
                }
                LLVMTypeRef slot_ptr_ty = LLVMPointerType(pt, 0);
                alloca = LLVMBuildAlloca(ctx->builder, slot_ptr_ty, p->name);
                LLVMBuildStore(ctx->builder,
                    LLVMGetParam(fn, (unsigned)emitted_index++), alloca);
                llvm_scope_declare(ctx, p->name, alloca, slot_ptr_ty);
                if (param_callable_sig != NULL)
                    llvm_register_callable_mir_signature(ctx, p->name,
                        param_callable_sig->param_count,
                        (const char *const *)param_callable_sig->param_type_names,
                        NULL,
                        param_callable_sig->return_type_name,
                        NULL);
                else if (param_type_name != NULL)
                    llvm_register_typed_var_abi_binding(ctx, p->name, alloca,
                        param_type_name);
                else
                    llvm_set_mir_inventory_missing(ctx,
                        "MIR-only LLVM path missing parameter ABI type fact for '%s'",
                        p->name);
                llvm_register_slot_var(ctx, p->name, slot_inner,
                    resource_kind == MIR_PARAM_RESOURCE_SECURE_SLOT);
                if (resource_kind == MIR_PARAM_RESOURCE_SECURE_SLOT) {
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

            if (pass_indirect
                || (param_type_name != NULL
                    && llvm_type_name_uses_pointer_self(ctx, param_type_name)))
                pt = LLVMPointerType(pt, 0);
            if (carriage == MIR_PARAM_CARRIAGE_VALUE_RESULT) {
                LLVMValueRef mr_ptr =
                    LLVMGetParam(fn, (unsigned)emitted_index++);
                alloca = LLVMBuildAlloca(ctx->builder, pt, p->name);
                LLVMBuildStore(ctx->builder,
                    LLVMBuildLoad2(ctx->builder, pt, mr_ptr, p->name), alloca);
                if (ctx->mut_ref_count < 64) {
                    ctx->mut_ref_ptr[ctx->mut_ref_count] = mr_ptr;
                    ctx->mut_ref_alloca[ctx->mut_ref_count] = alloca;
                    ctx->mut_ref_pt[ctx->mut_ref_count] = pt;
                    ctx->mut_ref_count++;
                }
            } else {
                alloca = LLVMBuildAlloca(ctx->builder, pt, p->name);
                LLVMBuildStore(ctx->builder,
                    LLVMGetParam(fn, (unsigned)emitted_index++), alloca);
            }
            llvm_scope_declare(ctx, p->name, alloca, pt);
            if (param_callable_sig != NULL) {
                llvm_register_callable_mir_signature(ctx, p->name,
                    param_callable_sig->param_count,
                    (const char *const *)param_callable_sig->param_type_names,
                    NULL,
                    param_callable_sig->return_type_name,
                    NULL);
            } else if (param_type_name != NULL)
                llvm_register_typed_var_abi_binding(ctx, p->name, alloca,
                    param_type_name);
            else
                llvm_set_mir_inventory_missing(ctx,
                    "MIR-only LLVM path missing parameter ABI type fact for '%s'",
                    p->name);
            if (param_type_name != NULL)
                llvm_register_var_class(ctx, p->name, param_type_name);
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
        }
    }
}

#endif
