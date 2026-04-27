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

    for (int i = 0; i < zone_cls->field_count; i++) {
        LLVMClassFieldInfo *field = &zone_cls->fields[i];
        if (!field->is_subject_slot || field->field_name == NULL) {
            continue;
        }
        if (strcmp(field->field_name, alias) == 0) {
            named_match = field->field_name;
            break;
        }
        if (participant_type != NULL && participant_llvm_type != NULL
            && field->field_type == participant_llvm_type) {
            if (typed_match != NULL)
                typed_ambiguous = true;
            else
                typed_match = field->field_name;
        }
    }

    if (named_match != NULL)
        return named_match;
    if (typed_match != NULL && !typed_ambiguous)
        return typed_match;
    return "<unbound>";
}

void
llvm_emit_intent_step_bind_bound_zone(LLVMGenCtx *ctx,
                                      ASTNode *intent,
                                      const char *zone_type_name,
                                      const char *zone_alias,
                                      const char *from_alias,
                                      const char **who_aliases,
                                      size_t who_alias_count)
{
    const char *from_zone_type_name;
    LLVMVarEntry *zone_var;
    LLVMValueRef zone_ptr;
    char sync_name[256];
    LLVMFuncEntry *sync_fn;
    LLVMClassTypeEntry *zone_cls;
    LLVMFuncEntry *trace_materialize_fn;
    LLVMFuncEntry *trace_transfer_fn;

    if (ctx == NULL || intent == NULL) {
        return;
    }
    if (zone_alias == NULL || zone_type_name == NULL)
        return;

    zone_var = llvm_scope_lookup(ctx, zone_alias);
    if (zone_var == NULL)
        return;
    if (LLVMGetTypeKind(zone_var->type) != LLVMPointerTypeKind)
        return;

    zone_ptr = LLVMBuildLoad2(ctx->builder, zone_var->type, zone_var->alloca, llvm_tmp_name(ctx));
    if (LLVMGetTypeKind(LLVMTypeOf(zone_ptr)) != LLVMPointerTypeKind)
        return;

    zone_cls = llvm_lookup_class(ctx, zone_type_name);
    trace_materialize_fn = llvm_lookup_function(ctx, "pgy_intent_trace_materialize_export");
    trace_transfer_fn = llvm_lookup_function(ctx, "pgy_intent_trace_transfer_export");
    from_zone_type_name = llvm_intent_zone_binding_type_name(ctx, from_alias);

    if (from_alias != NULL && from_zone_type_name != NULL) {
        LLVMVarEntry *from_zone_var = llvm_scope_lookup(ctx, from_alias);
        LLVMValueRef from_zone_ptr;
        LLVMClassTypeEntry *from_zone_cls = llvm_lookup_class(ctx, from_zone_type_name);
        char from_sync_name[256];
        LLVMFuncEntry *from_sync_fn;

        if (from_zone_var == NULL || LLVMGetTypeKind(from_zone_var->type) != LLVMPointerTypeKind)
            return;

        from_zone_ptr = LLVMBuildLoad2(ctx->builder, from_zone_var->type,
            from_zone_var->alloca, llvm_tmp_name(ctx));
        if (LLVMGetTypeKind(LLVMTypeOf(from_zone_ptr)) != LLVMPointerTypeKind)
            return;

        if (from_zone_cls != NULL) {
            for (size_t i = 0; i < who_alias_count; i++) {
                const char *alias = who_aliases[i];
                const char *from_slot_name = llvm_resolve_intent_zone_slot_name_for_zone(
                    ctx, intent, from_zone_type_name, alias);
                const char *to_slot_name = llvm_resolve_intent_zone_slot_name_for_zone(
                    ctx, intent, zone_type_name, alias);
                LLVMVarEntry *participant_var;
                const char *participant_type_name;
                LLVMTypeRef participant_ptr_type;
                LLVMTypeRef participant_value_type;
                LLVMValueRef participant_ptr;
                LLVMValueRef participant_value;
                LLVMValueRef handle;

                if (alias == NULL)
                    continue;

                participant_var = llvm_scope_lookup(ctx, alias);
                participant_type_name = llvm_lookup_var_class(ctx, alias);
                if (participant_var == NULL || participant_type_name == NULL) {
                    continue;
                }

                participant_ptr_type = participant_var->type;
                participant_value_type = pergyra_type_to_llvm(ctx, participant_type_name);
                participant_ptr = LLVMBuildLoad2(ctx->builder, participant_ptr_type, participant_var->alloca,
                    llvm_tmp_name(ctx));
                participant_value = LLVMBuildLoad2(ctx->builder, participant_value_type, participant_ptr,
                    llvm_tmp_name(ctx));
                handle = llvm_scope_lookup(ctx, "__intent_handle") != NULL
                    ? LLVMBuildLoad2(ctx->builder, ctx->type_i32,
                        llvm_scope_lookup(ctx, "__intent_handle")->alloca,
                        llvm_tmp_name(ctx))
                    : LLVMConstInt(ctx->type_i32, 0, 0);

                if (from_slot_name != NULL && strcmp(from_slot_name, "<unbound>") != 0) {
                    int from_field_idx = llvm_class_field_index(from_zone_cls, from_slot_name);
                    if (from_field_idx >= 0) {
                        LLVMValueRef from_slot_ptr = LLVMBuildStructGEP2(ctx->builder,
                            from_zone_cls->struct_type, from_zone_ptr, (unsigned)from_field_idx,
                            llvm_tmp_name(ctx));
                        LLVMBuildStore(ctx->builder, participant_value, from_slot_ptr);
                        if (trace_materialize_fn != NULL) {
                            LLVMValueRef args[] = {
                                handle,
                                LLVMBuildGlobalStringPtr(ctx->builder, alias, llvm_tmp_name(ctx)),
                                LLVMBuildGlobalStringPtr(ctx->builder, from_slot_name, llvm_tmp_name(ctx)),
                                LLVMBuildGlobalStringPtr(ctx->builder, from_zone_type_name, llvm_tmp_name(ctx))
                            };
                            LLVMBuildCall2(ctx->builder, trace_materialize_fn->fn_type,
                                trace_materialize_fn->fn, args, 4, "");
                        }
                    }
                }

                if (to_slot_name != NULL && strcmp(to_slot_name, "<unbound>") != 0
                    && zone_cls != NULL) {
                    int to_field_idx = llvm_class_field_index(zone_cls, to_slot_name);
                    if (to_field_idx >= 0) {
                        LLVMValueRef to_slot_ptr = LLVMBuildStructGEP2(ctx->builder,
                            zone_cls->struct_type, zone_ptr, (unsigned)to_field_idx,
                            llvm_tmp_name(ctx));
                        LLVMBuildStore(ctx->builder, participant_value, to_slot_ptr);
                        if (trace_transfer_fn != NULL) {
                            LLVMValueRef transfer_args[] = {
                                handle,
                                LLVMBuildGlobalStringPtr(ctx->builder, alias, llvm_tmp_name(ctx)),
                                LLVMBuildGlobalStringPtr(ctx->builder, from_zone_type_name, llvm_tmp_name(ctx)),
                                LLVMBuildGlobalStringPtr(ctx->builder,
                                    from_slot_name != NULL ? from_slot_name : "<unbound>",
                                    llvm_tmp_name(ctx)),
                                LLVMBuildGlobalStringPtr(ctx->builder, zone_type_name, llvm_tmp_name(ctx)),
                                LLVMBuildGlobalStringPtr(ctx->builder, to_slot_name, llvm_tmp_name(ctx))
                            };
                            LLVMBuildCall2(ctx->builder, trace_transfer_fn->fn_type,
                                trace_transfer_fn->fn, transfer_args, 6, "");
                        }
                    }
                }
            }
        }

        snprintf(from_sync_name, sizeof(from_sync_name), "%s_sync", from_zone_type_name);
        from_sync_fn = llvm_lookup_function(ctx, from_sync_name);
        if (from_sync_fn != NULL) {
            LLVMValueRef args[] = { from_zone_ptr };
            LLVMBuildCall2(ctx->builder, from_sync_fn->fn_type, from_sync_fn->fn, args, 1, "");
        }
        if (strcmp(from_alias, zone_alias) != 0 || strcmp(from_zone_type_name, zone_type_name) != 0) {
            snprintf(sync_name, sizeof(sync_name), "%s_sync", zone_type_name);
            sync_fn = llvm_lookup_function(ctx, sync_name);
            if (sync_fn != NULL) {
                LLVMValueRef args[] = { zone_ptr };
                LLVMBuildCall2(ctx->builder, sync_fn->fn_type, sync_fn->fn, args, 1, "");
            }
        }
        return;
    }

    if (zone_cls != NULL) {
        for (size_t i = 0; i < who_alias_count; i++) {
            const char *alias = who_aliases[i];
            const char *slot_name = llvm_resolve_intent_zone_slot_name_for_zone(
                ctx, intent, zone_type_name, alias);
            LLVMVarEntry *participant_var;
            const char *participant_type_name;
            LLVMTypeRef participant_ptr_type;
            LLVMTypeRef participant_value_type;
            LLVMValueRef participant_ptr;
            LLVMValueRef participant_value;
            LLVMValueRef slot_ptr;
            int field_idx;

            if (alias == NULL || slot_name == NULL || strcmp(slot_name, "<unbound>") == 0)
                continue;

            field_idx = llvm_class_field_index(zone_cls, slot_name);
            if (field_idx < 0)
                continue;

            participant_var = llvm_scope_lookup(ctx, alias);
            participant_type_name = llvm_lookup_var_class(ctx, alias);
            if (participant_var == NULL || participant_type_name == NULL) {
                continue;
            }

            participant_ptr_type = participant_var->type;
            participant_value_type = pergyra_type_to_llvm(ctx, participant_type_name);
            participant_ptr = LLVMBuildLoad2(ctx->builder, participant_ptr_type, participant_var->alloca,
                llvm_tmp_name(ctx));
            participant_value = LLVMBuildLoad2(ctx->builder, participant_value_type, participant_ptr,
                llvm_tmp_name(ctx));
            slot_ptr = LLVMBuildStructGEP2(ctx->builder, zone_cls->struct_type, zone_ptr,
                (unsigned)field_idx, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, participant_value, slot_ptr);

            if (trace_materialize_fn != NULL) {
                LLVMValueRef handle = llvm_scope_lookup(ctx, "__intent_handle") != NULL
                    ? LLVMBuildLoad2(ctx->builder, ctx->type_i32,
                        llvm_scope_lookup(ctx, "__intent_handle")->alloca,
                        llvm_tmp_name(ctx))
                    : LLVMConstInt(ctx->type_i32, 0, 0);
                LLVMValueRef args[] = {
                    handle,
                    LLVMBuildGlobalStringPtr(ctx->builder, alias, llvm_tmp_name(ctx)),
                    LLVMBuildGlobalStringPtr(ctx->builder, slot_name, llvm_tmp_name(ctx)),
                    LLVMBuildGlobalStringPtr(ctx->builder, zone_type_name, llvm_tmp_name(ctx))
                };
                LLVMBuildCall2(ctx->builder, trace_materialize_fn->fn_type,
                    trace_materialize_fn->fn, args, 4, "");
            }
        }
    }

    snprintf(sync_name, sizeof(sync_name), "%s_sync", zone_type_name);
    sync_fn = llvm_lookup_function(ctx, sync_name);
    if (sync_fn != NULL) {
        LLVMValueRef args[] = { zone_ptr };
        LLVMBuildCall2(ctx->builder, sync_fn->fn_type, sync_fn->fn, args, 1, "");
    }
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

    for (int i = 0; i < zone_cls->field_count; i++) {
        const char *field_name = zone_cls->fields[i].field_name;
        if (field_name == NULL)
            continue;
        if (strncmp(field_name, "__projection_dirty_", 19) == 0) {
            LLVMValueRef field_ptr = LLVMBuildStructGEP2(ctx->builder,
                zone_cls->struct_type, zone_ptr, (unsigned)zone_cls->fields[i].index,
                llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0), field_ptr);
        } else if (strncmp(field_name, "__projection_ready_", 19) == 0) {
            LLVMValueRef field_ptr = LLVMBuildStructGEP2(ctx->builder,
                zone_cls->struct_type, zone_ptr, (unsigned)zone_cls->fields[i].index,
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
    snprintf(sync_name, sizeof(sync_name), "%s_sync", zone_type_name);
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
