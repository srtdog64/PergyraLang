/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM backend intent zone binding and sync helpers.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_intent_internal.h"

const char *
llvm_intent_zone_binding_type_name(LLVMGenCtx *ctx, const char *alias)
{
    if (ctx == NULL || alias == NULL)
        return NULL;
    return llvm_lookup_var_class(ctx, alias);
}

bool
llvm_intent_zone_sync_name(char *out,
    size_t out_size,
    const char *zone_type_name)
{
    int written;

    if (out == NULL || out_size == 0 || zone_type_name == NULL)
        return false;

    written = snprintf(out, out_size, "%s_sync", zone_type_name);
    return written >= 0 && (size_t)written < out_size;
}

const char *
llvm_resolve_intent_zone_slot_name_for_zone(LLVMGenCtx *ctx, ASTNode *intent,
                                            const char *zone_type_name, const char *alias)
{
    LLVMClassTypeEntry *zone_cls = NULL;
    const char *participant_type = NULL;
    LLVMTypeRef participant_llvm_type = NULL;
    const char *named_match = NULL;
    const char *typed_match = NULL;
    bool typed_ambiguous = false;

    (void)intent;

    if (ctx == NULL || zone_type_name == NULL || alias == NULL) {
        return "<unbound>";
    }

    zone_cls = llvm_lookup_class(ctx, zone_type_name);
    participant_type = llvm_lookup_var_class(ctx, alias);
    if (participant_type != NULL)
        participant_llvm_type = pergyra_type_to_llvm(ctx, participant_type);
    if (zone_cls == NULL)
        return "<unbound>";

    for (int i = 0; i < llvm_class_field_count(zone_cls); i++) {
        const char *field_name = llvm_class_field_name_at(zone_cls, i);
        LLVMTypeRef field_type = llvm_class_field_type_at(zone_cls, i);
        if (!llvm_class_field_is_subject_slot_at(zone_cls, i)
            || field_name == NULL) {
            continue;
        }
        if (strcmp(field_name, alias) == 0) {
            named_match = field_name;
            break;
        }
        if (participant_type != NULL && participant_llvm_type != NULL
            && field_type == participant_llvm_type) {
            if (typed_match != NULL)
                typed_ambiguous = true;
            else
                typed_match = field_name;
        }
    }

    if (named_match != NULL)
        return named_match;
    if (typed_match != NULL && !typed_ambiguous)
        return typed_match;
    return "<unbound>";
}

bool
llvm_emit_intent_step_rebind_bound_zone_aliases(LLVMGenCtx *ctx,
                                                ASTNode *intent,
                                                const char *zone_type_name,
                                                const char *zone_alias,
                                                const char **who_aliases,
                                                size_t who_alias_count,
                                                LLVMValueRef *saved_allocas)
{
    LLVMVarEntry *zone_var;
    LLVMValueRef zone_ptr;
    LLVMClassTypeEntry *zone_cls;
    bool rebound = false;

    if (ctx == NULL || intent == NULL || saved_allocas == NULL) {
        return false;
    }
    if (zone_alias == NULL || zone_type_name == NULL)
        return false;

    zone_var = llvm_scope_lookup(ctx, zone_alias);
    if (zone_var == NULL || LLVMGetTypeKind(zone_var->type) != LLVMPointerTypeKind)
        return false;

    zone_ptr = LLVMBuildLoad2(ctx->builder, zone_var->type, zone_var->alloca, llvm_tmp_name(ctx));
    zone_cls = llvm_lookup_class(ctx, zone_type_name);
    if (zone_cls == NULL)
        return false;

    for (size_t i = 0; i < who_alias_count; i++) {
        const char *alias = who_aliases[i];
        const char *slot_name = llvm_resolve_intent_zone_slot_name_for_zone(ctx, intent, zone_type_name, alias);
        LLVMVarEntry *participant_var;
        int field_idx;
        LLVMValueRef original_ptr;
        LLVMValueRef slot_ptr;

        if (alias == NULL || slot_name == NULL || strcmp(slot_name, "<unbound>") == 0)
            continue;

        participant_var = llvm_scope_lookup(ctx, alias);
        if (participant_var == NULL || LLVMGetTypeKind(participant_var->type) != LLVMPointerTypeKind)
            continue;

        field_idx = llvm_class_field_index(zone_cls, slot_name);
        if (field_idx < 0)
            continue;

        original_ptr = LLVMBuildLoad2(ctx->builder, participant_var->type, participant_var->alloca, llvm_tmp_name(ctx));
        saved_allocas[i] = LLVMBuildAlloca(ctx->builder, participant_var->type, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, original_ptr, saved_allocas[i]);
        slot_ptr = LLVMBuildStructGEP2(ctx->builder, zone_cls->struct_type, zone_ptr,
            (unsigned)field_idx, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, slot_ptr, participant_var->alloca);
        rebound = true;
    }

    return rebound;
}

void
llvm_emit_intent_step_dirty_zone_projections(LLVMGenCtx *ctx,
                                             const char *zone_type_name,
                                             const char *zone_alias)
{
    LLVMVarEntry *zone_var;
    LLVMValueRef zone_ptr;
    LLVMClassTypeEntry *zone_cls;

    if (ctx == NULL || zone_alias == NULL || zone_type_name == NULL)
        return;

    zone_var = llvm_scope_lookup(ctx, zone_alias);
    zone_cls = llvm_lookup_class(ctx, zone_type_name);
    if (zone_var == NULL || zone_cls == NULL
        || LLVMGetTypeKind(zone_var->type) != LLVMPointerTypeKind) {
        return;
    }

    zone_ptr = LLVMBuildLoad2(ctx->builder, zone_var->type,
        zone_var->alloca, llvm_tmp_name(ctx));

    for (int i = 0; i < llvm_class_field_count(zone_cls); i++) {
        const char *field_name = llvm_class_field_name_at(zone_cls, i);
        int field_index = llvm_class_field_struct_index_at(zone_cls, i);
        if (field_name == NULL || field_index < 0)
            continue;
        if (strncmp(field_name, "__projection_dirty_", 19) == 0) {
            LLVMValueRef field_ptr = LLVMBuildStructGEP2(ctx->builder,
                zone_cls->struct_type, zone_ptr, (unsigned)field_index,
                llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0), field_ptr);
        } else if (strncmp(field_name, "__projection_ready_", 19) == 0) {
            LLVMValueRef field_ptr = LLVMBuildStructGEP2(ctx->builder,
                zone_cls->struct_type, zone_ptr, (unsigned)field_index,
                llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0), field_ptr);
        }
    }
}

void
llvm_emit_intent_step_sync_effective_zone(LLVMGenCtx *ctx,
                                          const char *zone_type_name,
                                          const char *zone_alias)
{
    LLVMVarEntry *zone_var;
    LLVMValueRef zone_ptr;
    char sync_name[256];
    LLVMFuncEntry *sync_fn;

    if (ctx == NULL) {
        return;
    }
    if (zone_alias == NULL || zone_type_name == NULL)
        return;

    zone_var = llvm_scope_lookup(ctx, zone_alias);
    if (zone_var == NULL || LLVMGetTypeKind(zone_var->type) != LLVMPointerTypeKind)
        return;

    zone_ptr = LLVMBuildLoad2(ctx->builder, zone_var->type, zone_var->alloca, llvm_tmp_name(ctx));
    if (!llvm_intent_zone_sync_name(sync_name, sizeof(sync_name),
            zone_type_name)) {
        llvm_set_error(ctx, "zone sync function name is too long");
        return;
    }
    sync_fn = llvm_lookup_function(ctx, sync_name);
    if (sync_fn != NULL) {
        LLVMValueRef args[] = { zone_ptr };
        LLVMBuildCall2(ctx->builder, sync_fn->fn_type, sync_fn->fn, args, 1, "");
    }
}

void
llvm_emit_intent_step_restore_bound_zone_aliases(LLVMGenCtx *ctx,
                                                 ASTNode *intent,
                                                 const char *zone_type_name,
                                                 const char **who_aliases,
                                                 size_t who_alias_count,
                                                 LLVMValueRef *saved_allocas)
{
    if (ctx == NULL || intent == NULL || saved_allocas == NULL) {
        return;
    }
    if (zone_type_name == NULL)
        return;

    for (size_t i = 0; i < who_alias_count; i++) {
        const char *alias = who_aliases[i];
        const char *slot_name = llvm_resolve_intent_zone_slot_name_for_zone(ctx, intent, zone_type_name, alias);
        LLVMVarEntry *participant_var;
        const char *participant_type_name;
        LLVMTypeRef participant_ptr_type;
        LLVMTypeRef participant_value_type;
        LLVMValueRef zone_bound_ptr;
        LLVMValueRef zone_bound_value;
        LLVMValueRef saved_ptr;

        if (saved_allocas[i] == NULL || alias == NULL || slot_name == NULL
            || strcmp(slot_name, "<unbound>") == 0) {
            continue;
        }

        participant_var = llvm_scope_lookup(ctx, alias);
        participant_type_name = llvm_lookup_var_class(ctx, alias);
        if (participant_var == NULL || participant_type_name == NULL) {
            continue;
        }

        participant_ptr_type = participant_var->type;
        participant_value_type = pergyra_type_to_llvm(ctx, participant_type_name);
        if (ctx->has_error || participant_value_type == NULL)
            return;
        zone_bound_ptr = LLVMBuildLoad2(ctx->builder, participant_ptr_type, participant_var->alloca,
            llvm_tmp_name(ctx));
        zone_bound_value = LLVMBuildLoad2(ctx->builder, participant_value_type, zone_bound_ptr,
            llvm_tmp_name(ctx));
        saved_ptr = LLVMBuildLoad2(ctx->builder, participant_ptr_type, saved_allocas[i], llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, zone_bound_value, saved_ptr);
        LLVMBuildStore(ctx->builder, saved_ptr, participant_var->alloca);
    }
}

#endif
