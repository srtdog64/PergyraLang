/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM backend intent zone materialization and transfer binding emission.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_intent_internal.h"

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

    zone_ptr = LLVMBuildLoad2(ctx->builder, zone_var->type,
        zone_var->alloca, llvm_tmp_name(ctx));
    if (LLVMGetTypeKind(LLVMTypeOf(zone_ptr)) != LLVMPointerTypeKind)
        return;

    zone_cls = llvm_lookup_class(ctx, zone_type_name);
    trace_materialize_fn = ctx->uses_intent_observability
        ? llvm_lookup_function(ctx, "pgy_intent_trace_materialize_export") : NULL;
    trace_transfer_fn = ctx->uses_intent_observability
        ? llvm_lookup_function(ctx, "pgy_intent_trace_transfer_export") : NULL;
    from_zone_type_name = llvm_intent_zone_binding_type_name(ctx, from_alias);

    if (from_alias != NULL && from_zone_type_name != NULL) {
        LLVMVarEntry *from_zone_var = llvm_scope_lookup(ctx, from_alias);
        LLVMValueRef from_zone_ptr;
        LLVMClassTypeEntry *from_zone_cls = llvm_lookup_class(ctx, from_zone_type_name);
        char from_sync_name[256];
        LLVMFuncEntry *from_sync_fn;

        if (from_zone_var == NULL
            || LLVMGetTypeKind(from_zone_var->type) != LLVMPointerTypeKind)
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
                if (participant_var == NULL || participant_type_name == NULL)
                    continue;

                participant_ptr_type = participant_var->type;
                participant_value_type = pergyra_type_to_llvm(ctx, participant_type_name);
                if (ctx->has_error || participant_value_type == NULL)
                    return;
                participant_ptr = LLVMBuildLoad2(ctx->builder, participant_ptr_type,
                    participant_var->alloca, llvm_tmp_name(ctx));
                participant_value = LLVMBuildLoad2(ctx->builder, participant_value_type,
                    participant_ptr, llvm_tmp_name(ctx));
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
                                LLVMBuildGlobalStringPtr(ctx->builder, from_slot_name,
                                    llvm_tmp_name(ctx)),
                                LLVMBuildGlobalStringPtr(ctx->builder, from_zone_type_name,
                                    llvm_tmp_name(ctx))
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
                                LLVMBuildGlobalStringPtr(ctx->builder, from_zone_type_name,
                                    llvm_tmp_name(ctx)),
                                LLVMBuildGlobalStringPtr(ctx->builder,
                                    from_slot_name != NULL ? from_slot_name : "<unbound>",
                                    llvm_tmp_name(ctx)),
                                LLVMBuildGlobalStringPtr(ctx->builder, zone_type_name,
                                    llvm_tmp_name(ctx)),
                                LLVMBuildGlobalStringPtr(ctx->builder, to_slot_name,
                                    llvm_tmp_name(ctx))
                            };
                            LLVMBuildCall2(ctx->builder, trace_transfer_fn->fn_type,
                                trace_transfer_fn->fn, transfer_args, 6, "");
                        }
                    }
                }
            }
        }

        if (!llvm_intent_zone_sync_name(from_sync_name,
                sizeof(from_sync_name), from_zone_type_name)) {
            llvm_set_error(ctx, "from-zone sync function name is too long");
            return;
        }
        from_sync_fn = llvm_lookup_function(ctx, from_sync_name);
        if (from_sync_fn != NULL) {
            LLVMValueRef args[] = { from_zone_ptr };
            LLVMBuildCall2(ctx->builder, from_sync_fn->fn_type,
                from_sync_fn->fn, args, 1, "");
        }
        if (strcmp(from_alias, zone_alias) != 0
            || strcmp(from_zone_type_name, zone_type_name) != 0) {
            if (!llvm_intent_zone_sync_name(sync_name, sizeof(sync_name),
                    zone_type_name)) {
                llvm_set_error(ctx, "zone sync function name is too long");
                return;
            }
            sync_fn = llvm_lookup_function(ctx, sync_name);
            if (sync_fn != NULL) {
                LLVMValueRef args[] = { zone_ptr };
                LLVMBuildCall2(ctx->builder, sync_fn->fn_type,
                    sync_fn->fn, args, 1, "");
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

            if (alias == NULL || slot_name == NULL
                || strcmp(slot_name, "<unbound>") == 0)
                continue;

            field_idx = llvm_class_field_index(zone_cls, slot_name);
            if (field_idx < 0)
                continue;

            participant_var = llvm_scope_lookup(ctx, alias);
            participant_type_name = llvm_lookup_var_class(ctx, alias);
            if (participant_var == NULL || participant_type_name == NULL)
                continue;

            participant_ptr_type = participant_var->type;
            participant_value_type = pergyra_type_to_llvm(ctx, participant_type_name);
            if (ctx->has_error || participant_value_type == NULL)
                return;
            participant_ptr = LLVMBuildLoad2(ctx->builder, participant_ptr_type,
                participant_var->alloca, llvm_tmp_name(ctx));
            participant_value = LLVMBuildLoad2(ctx->builder, participant_value_type,
                participant_ptr, llvm_tmp_name(ctx));
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

#endif
