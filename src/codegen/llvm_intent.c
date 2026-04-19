/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM backend — MIR-backed intent declaration helpers
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

static const char *
llvm_resolve_intent_zone_slot_name_for_zone(LLVMGenCtx *ctx, ASTNode *intent,
                                            const char *zone_type_name, const char *alias);

static const char *
llvm_intent_involves_type_name(ASTNode *involves)
{
    if (involves == NULL || involves->type != AST_INTENT_INVOLVES
        || involves->data.intent_involves.subject_type == NULL
        || involves->data.intent_involves.subject_type->type != AST_TYPE) {
        return NULL;
    }
    return involves->data.intent_involves.subject_type->data.type.name;
}

static bool
llvm_intent_type_is_subject_participant(LLVMGenCtx *ctx, const char *type_name)
{
    LLVMClassTypeEntry *cls;

    if (ctx == NULL || type_name == NULL)
        return false;
    cls = llvm_lookup_class(ctx, type_name);
    return cls != NULL && cls->is_subject;
}

bool
llvm_intent_involves_uses_pointer_self(LLVMGenCtx *ctx, ASTNode *involves)
{
    const char *type_name = llvm_intent_involves_type_name(involves);

    if (ctx == NULL || type_name == NULL)
        return false;
    return llvm_type_name_uses_pointer_self(ctx, type_name);
}

static const char *
llvm_intent_step_effective_zone_alias(ASTNode *step)
{
    if (step == NULL || step->type != AST_INTENT_STEP)
        return NULL;
    if (step->data.intent_step.using_expr != NULL
        && step->data.intent_step.using_expr->type == AST_IDENTIFIER) {
        return step->data.intent_step.using_expr->data.identifier.name;
    }
    return step->data.intent_step.transfer_to_alias;
}

static const char *
llvm_find_mir_intent_meta_arg(const MIRRoutine *routine,
                              const char *step_name,
                              const char *inst_name)
{
    if (routine == NULL || inst_name == NULL)
        return NULL;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            if (inst->kind != MIR_INST_STMT)
                continue;
            if (inst->name == NULL || strcmp(inst->name, inst_name) != 0)
                continue;
            if (inst->arg0 == NULL)
                continue;
            if (step_name != NULL) {
                if (inst->arg1 == NULL || strcmp(inst->arg1, step_name) != 0)
                    continue;
            } else if (inst->arg1 != NULL) {
                continue;
            }
            return inst->arg0;
        }
    }
    return NULL;
}

static bool
llvm_mir_intent_has_stmt(const MIRRoutine *routine,
                         const char *step_name,
                         const char *inst_name,
                         const char *arg0)
{
    if (routine == NULL || inst_name == NULL)
        return false;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            if (inst->kind != MIR_INST_STMT)
                continue;
            if (inst->name == NULL || strcmp(inst->name, inst_name) != 0)
                continue;
            if (step_name != NULL) {
                if (inst->arg1 == NULL || strcmp(inst->arg1, step_name) != 0)
                    continue;
            } else if (inst->arg1 != NULL) {
                continue;
            }
            if (arg0 != NULL) {
                if (inst->arg0 == NULL || strcmp(inst->arg0, arg0) != 0)
                    continue;
            }
            return true;
        }
    }

    return false;
}

static size_t
llvm_collect_mir_intent_who_aliases(const MIRRoutine *routine,
                                    const char *step_name,
                                    const char ***aliases_out)
{
    const char **aliases = NULL;
    size_t count = 0;
    size_t capacity = 0;

    if (aliases_out != NULL)
        *aliases_out = NULL;
    if (routine == NULL || aliases_out == NULL)
        return 0;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            const char **grown;

            if (inst->kind != MIR_INST_STMT)
                continue;
            if (inst->name == NULL || strcmp(inst->name, "IntentWho") != 0)
                continue;
            if (inst->arg0 == NULL)
                continue;
            if (step_name != NULL) {
                if (inst->arg1 == NULL || strcmp(inst->arg1, step_name) != 0)
                    continue;
            } else if (inst->arg1 != NULL) {
                continue;
            }

            if (count >= capacity) {
                size_t new_capacity = capacity == 0 ? 4 : capacity * 2;
                grown = realloc((void *)aliases, new_capacity * sizeof(const char *));
                if (grown == NULL) {
                    free((void *)aliases);
                    return 0;
                }
                aliases = grown;
                capacity = new_capacity;
            }
            aliases[count++] = inst->arg0;
        }
    }

    *aliases_out = aliases;
    return count;
}

static size_t
llvm_collect_mir_intent_participants(const MIRRoutine *routine,
                                     const char ***aliases_out,
                                     const char ***types_out)
{
    const char **aliases = NULL;
    const char **types = NULL;
    size_t count = 0;
    size_t capacity = 0;

    if (aliases_out != NULL)
        *aliases_out = NULL;
    if (types_out != NULL)
        *types_out = NULL;
    if (routine == NULL || aliases_out == NULL || types_out == NULL)
        return 0;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            const char **grown_aliases;
            const char **grown_types;

            if (inst->kind != MIR_INST_STMT)
                continue;
            if (inst->name == NULL || strcmp(inst->name, "IntentParticipant") != 0)
                continue;
            if (inst->arg0 == NULL || inst->arg1 == NULL)
                continue;

            if (count >= capacity) {
                size_t new_capacity = capacity == 0 ? 4 : capacity * 2;
                grown_aliases = malloc(new_capacity * sizeof(const char *));
                grown_types = malloc(new_capacity * sizeof(const char *));
                if (grown_aliases == NULL || grown_types == NULL) {
                    free((void *)grown_aliases);
                    free((void *)grown_types);
                    free((void *)aliases);
                    free((void *)types);
                    return 0;
                }
                if (count > 0) {
                    memcpy((void *)grown_aliases, (const void *)aliases,
                           count * sizeof(const char *));
                    memcpy((void *)grown_types, (const void *)types,
                           count * sizeof(const char *));
                }
                free((void *)aliases);
                free((void *)types);
                aliases = grown_aliases;
                types = grown_types;
                capacity = new_capacity;
            }
            aliases[count] = inst->arg0;
            types[count] = inst->arg1;
            count++;
        }
    }

    *aliases_out = aliases;
    *types_out = types;
    return count;
}

static const char *
llvm_intent_zone_binding_type_name(LLVMGenCtx *ctx, const char *alias)
{
    if (ctx == NULL || alias == NULL)
        return NULL;
    return llvm_lookup_var_class(ctx, alias);
}

static const char *
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

static void
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

static bool
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

static void
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

static void
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
        zone_bound_ptr = LLVMBuildLoad2(ctx->builder, participant_ptr_type, participant_var->alloca, llvm_tmp_name(ctx));
        zone_bound_value = LLVMBuildLoad2(ctx->builder, participant_value_type, zone_bound_ptr, llvm_tmp_name(ctx));
        saved_ptr = LLVMBuildLoad2(ctx->builder, participant_ptr_type, saved_allocas[i], llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, zone_bound_value, saved_ptr);
        LLVMBuildStore(ctx->builder, saved_ptr, participant_var->alloca);
    }
}

static const MIRRoutine *
llvm_find_mir_intent_routine(const LLVMGenCtx *ctx, ASTNode *intent_decl)
{
    if (ctx == NULL || ctx->mir == NULL || intent_decl == NULL
        || intent_decl->type != AST_INTENT_DECL
        || intent_decl->data.intent_decl.name == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < ctx->mir->routine_count; i++) {
        const MIRRoutine *routine = &ctx->mir->routines[i];
        if (routine->kind != MIR_SCOPE_INTENT || routine->name == NULL)
            continue;
        if (strcmp(routine->name, intent_decl->data.intent_decl.name) == 0)
            return routine;
    }

    return NULL;
}

static void
llvm_emit_mir_resource_hook(LLVMGenCtx *ctx,
                            const MIRInstruction *inst,
                            LLVMValueRef handle,
                            bool cleanup_hook)
{
    const char *helper_name = cleanup_hook
        ? "pgy_mir_cleanup_op_export"
        : "pgy_mir_resource_op_export";
    const char *op_name = "unknown";
    const char *slot_anchor = "";
    const char *arg_name = "";
    LLVMFuncEntry *helper;

    if (ctx == NULL || inst == NULL)
        return;

    helper = llvm_lookup_function(ctx, helper_name);
    if (helper == NULL)
        return;

    if (inst->name != NULL)
        op_name = inst->name;
    if (inst->rir_op != NULL && inst->rir_op->slot_anchor != NULL)
        slot_anchor = inst->rir_op->slot_anchor;
    else if (inst->arg0 != NULL)
        slot_anchor = inst->arg0;
    if (inst->arg1 != NULL)
        arg_name = inst->arg1;
    else if (inst->rir_op != NULL && inst->rir_op->arg0 != NULL)
        arg_name = inst->rir_op->arg0;

    {
        LLVMValueRef args[] = {
            handle,
            LLVMBuildGlobalStringPtr(ctx->builder, op_name, llvm_tmp_name(ctx)),
            LLVMBuildGlobalStringPtr(ctx->builder, slot_anchor, llvm_tmp_name(ctx)),
            LLVMBuildGlobalStringPtr(ctx->builder, arg_name, llvm_tmp_name(ctx))
        };
        LLVMBuildCall2(ctx->builder, helper->fn_type, helper->fn, args, 4, "");
    }
}

static size_t
llvm_collect_mir_intent_steps(const MIRRoutine *routine, ASTNode ***steps_out)
{
    ASTNode **steps = NULL;
    size_t count = 0;
    size_t capacity = 0;

    if (steps_out != NULL)
        *steps_out = NULL;
    if (routine == NULL || steps_out == NULL)
        return 0;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            ASTNode **grown;

            if (inst->kind != MIR_INST_STMT || inst->ast == NULL
                || inst->ast->type != AST_INTENT_STEP) {
                continue;
            }
            if (inst->name == NULL || strcmp(inst->name, "IntentStep") != 0)
                continue;
            if (count >= capacity) {
                size_t new_capacity = capacity == 0 ? 8 : capacity * 2;
                grown = realloc(steps, new_capacity * sizeof(ASTNode *));
                if (grown == NULL) {
                    free(steps);
                    return 0;
                }
                steps = grown;
                capacity = new_capacity;
            }
            steps[count++] = inst->ast;
        }
    }

    *steps_out = steps;
    return count;
}

static size_t
llvm_collect_mir_intent_step_names(const MIRRoutine *routine, const char ***names_out)
{
    const char **names = NULL;
    size_t count = 0;
    size_t capacity = 0;

    if (names_out != NULL)
        *names_out = NULL;
    if (routine == NULL || names_out == NULL)
        return 0;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            const char **grown;

            if (inst->kind != MIR_INST_STMT)
                continue;
            if (inst->name == NULL || strcmp(inst->name, "IntentStep") != 0)
                continue;

            if (count >= capacity) {
                size_t new_capacity = capacity == 0 ? 8 : capacity * 2;
                grown = realloc((void *)names, new_capacity * sizeof(const char *));
                if (grown == NULL) {
                    free((void *)names);
                    return 0;
                }
                names = grown;
                capacity = new_capacity;
            }
            names[count++] = inst->arg0 != NULL ? inst->arg0 : inst->name;
        }
    }

    *names_out = names;
    return count;
}

static ASTNode *
llvm_find_mir_intent_check_expr(const MIRRoutine *routine,
                                const char *step_name,
                                const char *phase_name)
{
    if (routine == NULL || phase_name == NULL)
        return NULL;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            if (inst->kind != MIR_INST_STMT || inst->ast == NULL)
                continue;
            if (inst->name == NULL || strcmp(inst->name, "IntentCheck") != 0)
                continue;
            if (inst->arg0 == NULL || strcmp(inst->arg0, phase_name) != 0)
                continue;
            if (step_name != NULL) {
                if (inst->arg1 == NULL || strcmp(inst->arg1, step_name) != 0)
                    continue;
            } else if (inst->arg1 != NULL) {
                continue;
            }
            return inst->ast;
        }
    }
    return NULL;
}

static size_t
llvm_collect_mir_intent_eval_exprs(const MIRRoutine *routine,
                                   const char *step_name,
                                   const char *phase_name,
                                   ASTNode ***exprs_out)
{
    ASTNode **exprs = NULL;
    size_t count = 0;
    size_t capacity = 0;

    if (exprs_out != NULL)
        *exprs_out = NULL;
    if (routine == NULL || phase_name == NULL || exprs_out == NULL)
        return 0;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            ASTNode **grown;

            if (inst->kind != MIR_INST_STMT || inst->ast == NULL)
                continue;
            if (inst->name == NULL || strcmp(inst->name, "IntentEval") != 0)
                continue;
            if (inst->arg0 == NULL || strcmp(inst->arg0, phase_name) != 0)
                continue;
            if (step_name != NULL) {
                if (inst->arg1 == NULL || strcmp(inst->arg1, step_name) != 0)
                    continue;
            } else if (inst->arg1 != NULL) {
                continue;
            }

            if (count >= capacity) {
                size_t new_capacity = capacity == 0 ? 4 : capacity * 2;
                grown = realloc(exprs, new_capacity * sizeof(ASTNode *));
                if (grown == NULL) {
                    free(exprs);
                    return 0;
                }
                exprs = grown;
                capacity = new_capacity;
            }
            exprs[count++] = inst->ast;
        }
    }

    *exprs_out = exprs;
    return count;
}

static ASTNode *
llvm_find_mir_intent_eval_expr(const MIRRoutine *routine,
                               const char *step_name,
                               const char *phase_name)
{
    ASTNode **exprs = NULL;
    ASTNode *result = NULL;
    size_t count = llvm_collect_mir_intent_eval_exprs(
        routine, step_name, phase_name, &exprs);
    if (count > 0)
        result = exprs[0];
    free(exprs);
    return result;
}

static size_t
llvm_collect_mir_intent_dispatch_aliases(const MIRRoutine *routine,
                                         const char *step_name,
                                         const char ***aliases_out)
{
    const char **aliases = NULL;
    size_t count = 0;
    size_t capacity = 0;

    if (aliases_out != NULL)
        *aliases_out = NULL;
    if (routine == NULL || aliases_out == NULL)
        return 0;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            const char **grown;

            if (inst->kind != MIR_INST_STMT)
                continue;
            if (inst->name == NULL || strcmp(inst->name, "IntentDispatch") != 0)
                continue;
            if (inst->arg0 == NULL)
                continue;
            if (step_name != NULL) {
                if (inst->arg1 == NULL || strcmp(inst->arg1, step_name) != 0)
                    continue;
            } else if (inst->arg1 != NULL) {
                continue;
            }

            if (count >= capacity) {
                size_t new_capacity = capacity == 0 ? 4 : capacity * 2;
                grown = realloc((void *)aliases, new_capacity * sizeof(const char *));
                if (grown == NULL) {
                    free((void *)aliases);
                    return 0;
                }
                aliases = grown;
                capacity = new_capacity;
            }
            aliases[count++] = inst->arg0;
        }
    }

    *aliases_out = aliases;
    return count;
}

void
llvm_forward_declare_intent(ASTNode *node, LLVMGenCtx *ctx)
{
    const MIRRoutine *mir_routine;
    const char *name;
    LLVMTypeRef *param_types = NULL;
    LLVMTypeRef fn_type;
    LLVMValueRef fn;
    const char **participant_aliases = NULL;
    const char **participant_types = NULL;
    size_t participant_count = 0;
    size_t param_count = 0;
    bool mir_only_intent = false;

    if (node == NULL || node->type != AST_INTENT_DECL || ctx == NULL)
        return;
    name = node->data.intent_decl.name;
    if (name == NULL || llvm_lookup_function(ctx, name) != NULL)
        return;
    mir_routine = llvm_find_mir_intent_routine(ctx, node);
    if (mir_routine != NULL) {
        participant_count = llvm_collect_mir_intent_participants(
            mir_routine, &participant_aliases, &participant_types);
    }
    mir_only_intent = ctx->mir != NULL && node->data.intent_decl.step_count > 0;
    if (mir_only_intent && node->data.intent_decl.involve_count > 0) {
        if (participant_count < node->data.intent_decl.involve_count) {
            char msg[256];
            snprintf(msg, sizeof(msg),
                     "MIR-only LLVM path missing intent participant metadata for '%s'",
                     node->data.intent_decl.name != NULL
                         ? node->data.intent_decl.name
                         : "(anonymous)");
            llvm_set_error(ctx, msg);
            free((void *)participant_aliases);
            free((void *)participant_types);
            return;
        }
        for (size_t i = 0; i < node->data.intent_decl.involve_count; i++) {
            if (participant_aliases == NULL || participant_types == NULL
                || participant_aliases[i] == NULL || participant_types[i] == NULL) {
                char msg[256];
                snprintf(msg, sizeof(msg),
                         "MIR-only LLVM path has incomplete intent participant metadata for '%s'",
                         node->data.intent_decl.name != NULL
                             ? node->data.intent_decl.name
                             : "(anonymous)");
                llvm_set_error(ctx, msg);
                free((void *)participant_aliases);
                free((void *)participant_types);
                return;
            }
        }
    }
    param_count = node->data.intent_decl.binding_count > 0
        ? node->data.intent_decl.binding_count
        : (node->data.intent_decl.involve_count + node->data.intent_decl.value_count);
    if (participant_count == 0)
        participant_count = node->data.intent_decl.involve_count;
    if (param_count == 0)
        param_count = participant_count;

    if (param_count > 0) {
        param_types = calloc(param_count, sizeof(LLVMTypeRef));
        size_t participant_index = 0;
        for (size_t i = 0; i < param_count; i++) {
            LLVMTypeRef pt = ctx->type_i32;
            ASTNode *binding = node->data.intent_decl.binding_count > 0
                ? node->data.intent_decl.bindings[i]
                : (i < node->data.intent_decl.involve_count
                    ? node->data.intent_decl.involves[i]
                    : node->data.intent_decl.values[i - node->data.intent_decl.involve_count]);
            if (binding != NULL && binding->type == AST_INTENT_INVOLVES) {
                const char *type_name = (participant_types != NULL && participant_index < participant_count)
                    ? participant_types[participant_index]
                    : llvm_intent_involves_type_name(binding);
                if (type_name != NULL) {
                    pt = pergyra_type_to_llvm(ctx, type_name);
                    if (llvm_type_name_uses_pointer_self(ctx, type_name))
                        pt = LLVMPointerType(pt, 0);
                } else if (!mir_only_intent
                           && binding->data.intent_involves.subject_type != NULL) {
                    pt = ast_type_to_llvm(ctx, binding->data.intent_involves.subject_type);
                    if (llvm_intent_involves_uses_pointer_self(ctx, binding))
                        pt = LLVMPointerType(pt, 0);
                }
                participant_index++;
            } else {
                if (binding != NULL && binding->data.intent_value.value_type != NULL)
                    pt = ast_type_to_llvm(ctx, binding->data.intent_value.value_type);
            }
            param_types[i] = pt;
        }
    }

    fn_type = LLVMFunctionType(ctx->type_i1, param_types,
        (unsigned)param_count, 0);
    fn = LLVMAddFunction(ctx->module, name, fn_type);
    /* Disable stack probing (avoids __chkstk linking issues with mingw) */
    {
        unsigned attr_kind = LLVMGetEnumAttributeKindForName(
            "no-stack-arg-probe", 18);
        if (attr_kind != 0) {
            LLVMAddAttributeAtIndex(fn, LLVMAttributeFunctionIndex,
                LLVMCreateEnumAttribute(ctx->context, attr_kind, 0));
        }
    }
    llvm_register_function(ctx, name, fn, fn_type, ctx->type_i1);
    free((void *)participant_aliases);
    free((void *)participant_types);
    free(param_types);
}

void
llvm_emit_intent_decl(ASTNode *node, LLVMGenCtx *ctx)
{
    const MIRRoutine *mir_routine;
    ASTNode **mir_steps = NULL;
    ASTNode **step_nodes = NULL;
    const char **mir_step_names = NULL;
    const char **participant_aliases = NULL;
    const char **participant_types = NULL;
    LLVMFuncEntry *entry;
    LLVMFuncEntry *enter_fn;
    LLVMFuncEntry *exit_fn;
    LLVMFuncEntry *trace_step_fn;
    LLVMFuncEntry *trace_bind_fn;
    LLVMFuncEntry *trace_step_ok_fn;
    LLVMFuncEntry *trace_fail_fn;
    LLVMValueRef fn;
    LLVMValueRef saved_fn;
    LLVMTypeRef saved_ret_type;
    LLVMBasicBlockRef entry_bb;
    LLVMBasicBlockRef run_bb;
    LLVMBasicBlockRef fail_enter_bb;
    LLVMBasicBlockRef fail_bb;
    LLVMBasicBlockRef cleanup_bb;
    LLVMBasicBlockRef compensate_bb;
    LLVMBasicBlockRef maybe_exit_bb;
    LLVMBasicBlockRef do_exit_bb;
    LLVMBasicBlockRef ret_bb;
    LLVMValueRef result_alloca;
    LLVMValueRef failed_alloca;
    LLVMValueRef fail_reason_alloca;
    LLVMValueRef handle_alloca;
    LLVMValueRef subjects_ptr;
    LLVMValueRef *completed_allocas = NULL;
    size_t participant_count = 0;
    size_t param_count = 0;
    size_t subject_count = 0;
    size_t step_count = 0;
    bool has_compensate_steps = false;
    bool mir_only_intent = false;

    if (node == NULL || node->type != AST_INTENT_DECL || ctx == NULL)
        return;
    mir_routine = llvm_find_mir_intent_routine(ctx, node);
    if (mir_routine != NULL) {
        step_count = llvm_collect_mir_intent_steps(mir_routine, &mir_steps);
        (void)llvm_collect_mir_intent_step_names(mir_routine, &mir_step_names);
    }
    if (ctx->mir != NULL && node->data.intent_decl.step_count > 0) {
        if (mir_routine == NULL) {
            char msg[256];
            snprintf(msg, sizeof(msg),
                     "MIR-only LLVM path missing intent routine for '%s'",
                     node->data.intent_decl.name != NULL
                         ? node->data.intent_decl.name
                         : "(anonymous)");
            llvm_set_error(ctx, msg);
            free(mir_steps);
            free((void *)mir_step_names);
            return;
        }
        if (step_count == 0) {
            char msg[256];
            snprintf(msg, sizeof(msg),
                     "MIR-only LLVM path missing intent step sequence for '%s'",
                     node->data.intent_decl.name != NULL
                         ? node->data.intent_decl.name
                         : "(anonymous)");
            llvm_set_error(ctx, msg);
            free(mir_steps);
            free((void *)mir_step_names);
            return;
        }
        mir_only_intent = true;
    }
    if (step_count > 0) {
        step_nodes = mir_steps;
    } else {
        step_count = node->data.intent_decl.step_count;
        step_nodes = node->data.intent_decl.steps;
    }
    if (mir_routine != NULL) {
        participant_count = llvm_collect_mir_intent_participants(
            mir_routine, &participant_aliases, &participant_types);
    }
    if (mir_only_intent && node->data.intent_decl.involve_count > 0) {
        if (participant_count < node->data.intent_decl.involve_count) {
            char msg[256];
            snprintf(msg, sizeof(msg),
                     "MIR-only LLVM path missing intent participant metadata for '%s'",
                     node->data.intent_decl.name != NULL
                         ? node->data.intent_decl.name
                         : "(anonymous)");
            llvm_set_error(ctx, msg);
            free((void *)participant_aliases);
            free((void *)participant_types);
            free(mir_steps);
            free((void *)mir_step_names);
            return;
        }
        for (size_t i = 0; i < node->data.intent_decl.involve_count; i++) {
            if (participant_aliases == NULL || participant_types == NULL
                || participant_aliases[i] == NULL || participant_types[i] == NULL) {
                char msg[256];
                snprintf(msg, sizeof(msg),
                         "MIR-only LLVM path has incomplete intent participant metadata for '%s'",
                         node->data.intent_decl.name != NULL
                             ? node->data.intent_decl.name
                             : "(anonymous)");
                llvm_set_error(ctx, msg);
                free((void *)participant_aliases);
                free((void *)participant_types);
                free(mir_steps);
                free((void *)mir_step_names);
                return;
            }
        }
    }
    if (participant_count == 0)
        participant_count = node->data.intent_decl.involve_count;
    param_count = node->data.intent_decl.binding_count > 0
        ? node->data.intent_decl.binding_count
        : (node->data.intent_decl.involve_count + node->data.intent_decl.value_count);
    if (param_count == 0)
        param_count = participant_count;

    for (size_t i = 0; i < step_count; i++) {
        ASTNode *step = step_nodes[i];
        const char *step_name = (mir_step_names != NULL) ? mir_step_names[i] : NULL;
        if (step != NULL && step->type == AST_INTENT_STEP
            && ((mir_only_intent && mir_routine != NULL
                 && llvm_mir_intent_has_stmt(
                     mir_routine, step_name != NULL ? step_name : step->data.intent_step.name,
                     "IntentEval", "compensate"))
                || (!mir_only_intent && step->data.intent_step.compensate_expr_count > 0))) {
            has_compensate_steps = true;
            break;
        }
    }
    if (node->data.intent_decl.rollback_policy == INTENT_ROLLBACK_NONE)
        has_compensate_steps = false;
    entry = llvm_lookup_function(ctx, node->data.intent_decl.name);
    if (entry == NULL)
        return;
    enter_fn = llvm_lookup_function(ctx, "pgy_intent_enter_export");
    exit_fn = llvm_lookup_function(ctx, "pgy_intent_exit_export");
    trace_step_fn = llvm_lookup_function(ctx, "pgy_intent_trace_step_export");
    trace_bind_fn = llvm_lookup_function(ctx, "pgy_intent_trace_bind_export");
    trace_step_ok_fn = llvm_lookup_function(ctx, "pgy_intent_trace_step_ok_export");
    trace_fail_fn = llvm_lookup_function(ctx, "pgy_intent_trace_fail_export");
    if (enter_fn == NULL || exit_fn == NULL
        || trace_step_fn == NULL || trace_bind_fn == NULL
        || trace_step_ok_fn == NULL || trace_fail_fn == NULL)
        return;

    fn = entry->fn;
    saved_fn = ctx->current_function;
    saved_ret_type = ctx->current_ret_type;
    ctx->current_function = fn;
    ctx->current_ret_type = ctx->type_i1;

    entry_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "entry");
    run_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "intent.run");
    fail_enter_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "intent.fail.enter");
    fail_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "intent.fail");
    cleanup_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "intent.cleanup");
    compensate_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "intent.compensate");
    maybe_exit_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "intent.maybe_exit");
    do_exit_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "intent.do_exit");
    ret_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "intent.ret");
    LLVMPositionBuilderAtEnd(ctx->builder, entry_bb);
    llvm_scope_push(ctx);

    for (size_t i = 0, participant_index = 0; i < param_count; i++) {
        LLVMTypeRef pt = ctx->type_i8ptr;
        const char *alias = NULL;
        const char *type_name = NULL;
        ASTNode *binding = node->data.intent_decl.binding_count > 0
            ? node->data.intent_decl.bindings[i]
            : (i < node->data.intent_decl.involve_count
                ? node->data.intent_decl.involves[i]
                : node->data.intent_decl.values[i - node->data.intent_decl.involve_count]);
        if (binding != NULL && binding->type == AST_INTENT_INVOLVES) {
            ASTNode *involves = binding;
            alias = (mir_only_intent && participant_aliases != NULL && participant_index < participant_count)
                ? participant_aliases[participant_index]
                : (participant_aliases != NULL && participant_index < participant_count
                    ? participant_aliases[participant_index]
                    : (involves != NULL ? involves->data.intent_involves.alias : NULL));
            type_name = (mir_only_intent && participant_types != NULL && participant_index < participant_count)
                ? participant_types[participant_index]
                : (participant_types != NULL && participant_index < participant_count
                    ? participant_types[participant_index]
                    : ((involves != NULL
                        && involves->data.intent_involves.subject_type != NULL
                        && involves->data.intent_involves.subject_type->type == AST_TYPE)
                        ? involves->data.intent_involves.subject_type->data.type.name : NULL));
            if (type_name != NULL) {
                pt = pergyra_type_to_llvm(ctx, type_name);
                if (llvm_type_name_uses_pointer_self(ctx, type_name))
                    pt = LLVMPointerType(pt, 0);
            } else if (!mir_only_intent
                       && involves != NULL
                       && involves->data.intent_involves.subject_type != NULL) {
                pt = ast_type_to_llvm(ctx, involves->data.intent_involves.subject_type);
                if (llvm_intent_involves_uses_pointer_self(ctx, involves))
                    pt = LLVMPointerType(pt, 0);
            }
            participant_index++;
        } else if (binding != NULL && binding->type == AST_INTENT_VALUE) {
            ASTNode *value = binding;
            alias = value->data.intent_value.alias;
            type_name = (value->data.intent_value.value_type != NULL
                && value->data.intent_value.value_type->type == AST_TYPE)
                ? value->data.intent_value.value_type->data.type.name : NULL;
            if (value->data.intent_value.value_type != NULL)
                pt = ast_type_to_llvm(ctx, value->data.intent_value.value_type);
        }
        LLVMValueRef a = llvm_create_entry_alloca(ctx, pt, alias != NULL ? alias : "param");
        LLVMBuildStore(ctx->builder, LLVMGetParam(fn, (unsigned)i), a);
        llvm_scope_declare(ctx, alias != NULL ? alias : "param", a, pt);
        if (type_name != NULL)
            llvm_register_var_class(ctx, alias, type_name);
    }

    result_alloca = llvm_create_entry_alloca(ctx, ctx->type_i1, "__intent_result");
    LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0), result_alloca);
    failed_alloca = llvm_create_entry_alloca(ctx, ctx->type_i1, "__intent_failed");
    LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0), failed_alloca);
    fail_reason_alloca = llvm_create_entry_alloca(ctx, ctx->type_i8ptr, "__intent_fail_reason");
    LLVMBuildStore(ctx->builder, LLVMBuildGlobalStringPtr(ctx->builder, "", llvm_tmp_name(ctx)),
        fail_reason_alloca);
    handle_alloca = llvm_create_entry_alloca(ctx, ctx->type_i32, "__intent_handle");
    LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i32, 0, 0), handle_alloca);
    llvm_scope_declare(ctx, "__intent_handle", handle_alloca, ctx->type_i32);
    if (has_compensate_steps && step_count > 0) {
        completed_allocas = calloc(step_count, sizeof(LLVMValueRef));
        for (size_t i = 0; i < step_count; i++) {
            completed_allocas[i] = llvm_create_entry_alloca(ctx, ctx->type_i1, "__intent_step_done");
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0), completed_allocas[i]);
        }
    }

    for (size_t i = 0; i < participant_count; i++) {
        ASTNode *involves = i < node->data.intent_decl.involve_count
            ? node->data.intent_decl.involves[i]
            : NULL;
        const char *type_name = (mir_only_intent && participant_types != NULL && i < participant_count)
            ? participant_types[i]
            : (participant_types != NULL && i < participant_count
                ? participant_types[i]
                : llvm_intent_involves_type_name(involves));
        if (llvm_intent_type_is_subject_participant(ctx, type_name)) {
            subject_count++;
        }
    }

    if (subject_count > 0) {
        LLVMTypeRef subject_array_type = LLVMArrayType(ctx->type_i8ptr,
            (unsigned)subject_count);
        LLVMValueRef subjects_alloca = llvm_create_entry_alloca(ctx,
            subject_array_type, "__intent_subjects");
        LLVMValueRef zero = LLVMConstInt(ctx->type_i32, 0, 0);
        unsigned subject_index = 0;

        for (size_t i = 0; i < participant_count; i++) {
            ASTNode *involves = i < node->data.intent_decl.involve_count
                ? node->data.intent_decl.involves[i]
                : NULL;
            const char *alias = (mir_only_intent && participant_aliases != NULL && i < participant_count)
                ? participant_aliases[i]
                : (participant_aliases != NULL && i < participant_count
                    ? participant_aliases[i]
                    : (involves != NULL ? involves->data.intent_involves.alias : NULL));
            const char *type_name = (mir_only_intent && participant_types != NULL && i < participant_count)
                ? participant_types[i]
                : (participant_types != NULL && i < participant_count
                    ? participant_types[i]
                    : llvm_intent_involves_type_name(involves));
            LLVMVarEntry *participant_var = llvm_scope_lookup(ctx, alias != NULL ? alias : "participant");
            LLVMValueRef indices[] = {
                zero,
                LLVMConstInt(ctx->type_i32, subject_index, 0)
            };
            LLVMValueRef participant_ptr = participant_var != NULL
                ? LLVMBuildLoad2(ctx->builder, participant_var->type, participant_var->alloca, llvm_tmp_name(ctx))
                : LLVMConstPointerNull(ctx->type_i8ptr);
            if (!llvm_intent_type_is_subject_participant(ctx, type_name))
                continue;
            if (LLVMGetTypeKind(LLVMTypeOf(participant_ptr)) != LLVMPointerTypeKind)
                continue;
            LLVMValueRef cast_participant = participant_ptr;
            if (LLVMTypeOf(participant_ptr) != ctx->type_i8ptr) {
                cast_participant = LLVMBuildBitCast(ctx->builder, participant_ptr,
                    ctx->type_i8ptr, llvm_tmp_name(ctx));
            }
            LLVMValueRef elem_ptr = LLVMBuildGEP2(ctx->builder, subject_array_type,
                subjects_alloca, indices, 2, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, cast_participant, elem_ptr);
            subject_index++;
        }

        {
            LLVMValueRef indices[] = { zero, zero };
            subjects_ptr = LLVMBuildGEP2(ctx->builder, subject_array_type,
                subjects_alloca, indices, 2, llvm_tmp_name(ctx));
        }
    } else {
        subjects_ptr = LLVMConstPointerNull(LLVMPointerType(ctx->type_i8ptr, 0));
    }

    {
        LLVMValueRef priority = node->data.intent_decl.priority_expr != NULL
            ? llvm_emit_expression(node->data.intent_decl.priority_expr, ctx)
            : LLVMConstInt(ctx->type_i32, 0, 0);
        LLVMValueRef enter_args[] = {
            LLVMBuildGlobalStringPtr(ctx->builder, node->data.intent_decl.name,
                llvm_tmp_name(ctx)),
            subjects_ptr,
            LLVMConstInt(ctx->type_i32, (unsigned)subject_count, 0),
            LLVMConstInt(ctx->type_i1, node->data.intent_decl.is_concurrent ? 1 : 0, 0),
            priority
        };
        LLVMValueRef handle = LLVMBuildCall2(ctx->builder, enter_fn->fn_type, enter_fn->fn,
            enter_args, 5, llvm_tmp_name(ctx));
        LLVMValueRef entered = LLVMBuildICmp(ctx->builder, LLVMIntNE, handle,
            LLVMConstInt(ctx->type_i32, 0, 0), llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, handle, handle_alloca);
        LLVMBuildCondBr(ctx->builder, entered, run_bb, fail_enter_bb);
    }

    LLVMPositionBuilderAtEnd(ctx->builder, fail_enter_bb);
    {
        LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0), failed_alloca);
        LLVMBuildStore(ctx->builder,
            LLVMBuildGlobalStringPtr(ctx->builder, "enter-conflict", llvm_tmp_name(ctx)),
            fail_reason_alloca);
        LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0), result_alloca);
        LLVMBuildBr(ctx->builder, cleanup_bb);
    }

    LLVMPositionBuilderAtEnd(ctx->builder, run_bb);

    for (size_t i = 0; i < step_count; i++) {
        ASTNode *step = step_nodes[i];
        const char *step_name = (mir_step_names != NULL) ? mir_step_names[i] : NULL;
        ASTNode *pre_expr = NULL;
        ASTNode *guard_expr = NULL;
        ASTNode *post_expr = NULL;
        ASTNode *expect_expr = NULL;
        ASTNode *invariant_pre_expr = NULL;
        ASTNode *invariant_post_expr = NULL;
        ASTNode **on_exprs = NULL;
        size_t on_expr_count = 0;
        ASTNode *subintent_expr = NULL;
        const char *zone_type_name = NULL;
        const char *zone_alias = NULL;
        const char *from_alias = NULL;
        const char **who_aliases = NULL;
        size_t who_alias_count = 0;
        const char **dispatch_aliases = NULL;
        size_t dispatch_alias_count = 0;
        LLVMValueRef *saved_participant_ptrs = NULL;
        bool rebound_aliases = false;
        if (step == NULL || step->type != AST_INTENT_STEP)
            continue;
        if (step_name == NULL)
            step_name = step->data.intent_step.name;
        if (mir_routine != NULL) {
            pre_expr = llvm_find_mir_intent_check_expr(mir_routine, step_name, "pre");
            guard_expr = llvm_find_mir_intent_check_expr(mir_routine, step_name, "guard");
            post_expr = llvm_find_mir_intent_check_expr(mir_routine, step_name, "post");
            expect_expr = llvm_find_mir_intent_check_expr(mir_routine, step_name, "expect");
            invariant_pre_expr = llvm_find_mir_intent_check_expr(mir_routine, step_name, "invariant-pre");
            invariant_post_expr = llvm_find_mir_intent_check_expr(mir_routine, step_name, "invariant-post");
            on_expr_count = llvm_collect_mir_intent_eval_exprs(
                mir_routine, step_name, "on", &on_exprs);
            subintent_expr = llvm_find_mir_intent_eval_expr(mir_routine, step_name, "intent");
            zone_type_name = llvm_find_mir_intent_meta_arg(mir_routine, step_name, "IntentZoneWhere");
            zone_alias = llvm_find_mir_intent_meta_arg(mir_routine, step_name, "IntentZoneAlias");
            from_alias = llvm_find_mir_intent_meta_arg(mir_routine, step_name, "IntentZoneFrom");
            who_alias_count = llvm_collect_mir_intent_who_aliases(mir_routine, step_name, &who_aliases);
            dispatch_alias_count = llvm_collect_mir_intent_dispatch_aliases(
                mir_routine, step_name, &dispatch_aliases);
        }
        if (mir_only_intent) {
            if (llvm_mir_intent_has_stmt(mir_routine, step_name, "IntentCheck", "pre")
                && pre_expr == NULL)
                goto mir_step_missing_pre;
            if (llvm_mir_intent_has_stmt(mir_routine, step_name, "IntentCheck", "guard")
                && guard_expr == NULL)
                goto mir_step_missing_guard;
            if (llvm_mir_intent_has_stmt(mir_routine, step_name, "IntentCheck", "post")
                && post_expr == NULL)
                goto mir_step_missing_post;
            if (llvm_mir_intent_has_stmt(mir_routine, step_name, "IntentCheck", "expect")
                && expect_expr == NULL)
                goto mir_step_missing_expect;
            if ((llvm_mir_intent_has_stmt(mir_routine, step_name, "IntentCheck", "invariant-pre")
                 || llvm_mir_intent_has_stmt(mir_routine, step_name, "IntentCheck", "invariant-post"))
                && (invariant_pre_expr == NULL || invariant_post_expr == NULL))
                goto mir_step_missing_invariant;
            if (llvm_mir_intent_has_stmt(mir_routine, step_name, "IntentEval", "intent")
                && subintent_expr == NULL)
                goto mir_step_missing_subintent;
            if (llvm_mir_intent_has_stmt(mir_routine, step_name, "IntentEval", "on")
                && on_expr_count == 0)
                goto mir_step_missing_on;
            if (llvm_mir_intent_has_stmt(mir_routine, step_name, "IntentZoneWhere", NULL)
                && zone_type_name == NULL)
                goto mir_step_missing_zone_where;
            if (llvm_mir_intent_has_stmt(mir_routine, step_name, "IntentZoneAlias", NULL)
                && zone_alias == NULL)
                goto mir_step_missing_zone_alias;
            if (llvm_mir_intent_has_stmt(mir_routine, step_name, "IntentZoneFrom", NULL)
                && from_alias == NULL)
                goto mir_step_missing_zone_from;
            if (llvm_mir_intent_has_stmt(mir_routine, step_name, "IntentWho", NULL)
                && who_alias_count == 0)
                goto mir_step_missing_who;
        } else {
            if (pre_expr == NULL)
                pre_expr = step->data.intent_step.pre_expr;
            if (guard_expr == NULL)
                guard_expr = step->data.intent_step.guard_expr;
            if (post_expr == NULL)
                post_expr = step->data.intent_step.post_expr;
            if (expect_expr == NULL)
                expect_expr = step->data.intent_step.expect_expr;
            if (invariant_pre_expr == NULL)
                invariant_pre_expr = step->data.intent_step.invariant_expr;
            if (invariant_post_expr == NULL)
                invariant_post_expr = step->data.intent_step.invariant_expr;
            if (subintent_expr == NULL)
                subintent_expr = step->data.intent_step.intent_expr;
            if (zone_type_name == NULL
                && step->data.intent_step.where_type != NULL
                && step->data.intent_step.where_type->type == AST_TYPE) {
                zone_type_name = step->data.intent_step.where_type->data.type.name;
            }
            if (zone_alias == NULL)
                zone_alias = llvm_intent_step_effective_zone_alias(step);
            if (from_alias == NULL)
                from_alias = step->data.intent_step.transfer_from_alias;
            if (who_alias_count == 0) {
                who_alias_count = step->data.intent_step.who_count;
                who_aliases = (const char **)step->data.intent_step.who_names;
            }
        }

        {
            LLVMValueRef handle = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
                handle_alloca, llvm_tmp_name(ctx));
            LLVMValueRef args[] = {
                handle,
                LLVMBuildGlobalStringPtr(ctx->builder,
                    step_name != NULL ? step_name : "<step>",
                    llvm_tmp_name(ctx)),
                LLVMBuildGlobalStringPtr(ctx->builder,
                    zone_type_name != NULL ? zone_type_name : "<zone>",
                    llvm_tmp_name(ctx))
            };
            LLVMBuildCall2(ctx->builder, trace_step_fn->fn_type, trace_step_fn->fn, args, 3, "");
        }
        for (size_t j = 0; j < who_alias_count; j++) {
            LLVMValueRef handle = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
                handle_alloca, llvm_tmp_name(ctx));
            const char *alias = who_aliases[j];
            const char *slot_name = llvm_resolve_intent_zone_slot_name_for_zone(
                ctx, node, zone_type_name, alias);
            LLVMValueRef args[] = {
                handle,
                LLVMBuildGlobalStringPtr(ctx->builder, alias != NULL ? alias : "<participant>",
                    llvm_tmp_name(ctx)),
                LLVMBuildGlobalStringPtr(ctx->builder, slot_name != NULL ? slot_name : "<unbound>",
                    llvm_tmp_name(ctx))
            };
            LLVMBuildCall2(ctx->builder, trace_bind_fn->fn_type, trace_bind_fn->fn, args, 3, "");
        }
        llvm_emit_intent_step_bind_bound_zone(
            ctx, node, zone_type_name, zone_alias, from_alias, who_aliases, who_alias_count);
        if (who_alias_count > 0) {
            saved_participant_ptrs = calloc(who_alias_count, sizeof(LLVMValueRef));
            if (saved_participant_ptrs != NULL)
                rebound_aliases = llvm_emit_intent_step_rebind_bound_zone_aliases(
                    ctx, node, zone_type_name, zone_alias, who_aliases, who_alias_count, saved_participant_ptrs);
        }

        if (pre_expr != NULL) {
            char reason[256];
            LLVMBasicBlockRef next_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "intent.pre.ok");
            LLVMValueRef cond = llvm_emit_expression(pre_expr, ctx);
            snprintf(reason, sizeof(reason), "pre:%s",
                step_name != NULL ? step_name : "<step>");
            LLVMBuildStore(ctx->builder,
                LLVMBuildGlobalStringPtr(ctx->builder,
                    reason,
                    llvm_tmp_name(ctx)),
                fail_reason_alloca);
            LLVMBuildCondBr(ctx->builder, cond, next_bb, fail_bb);
            LLVMPositionBuilderAtEnd(ctx->builder, next_bb);
        }

        if (invariant_pre_expr != NULL) {
            char reason[256];
            LLVMBasicBlockRef next_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "intent.invariant.pre.ok");
            LLVMValueRef cond = llvm_emit_expression(invariant_pre_expr, ctx);
            snprintf(reason, sizeof(reason), "invariant-pre:%s",
                step_name != NULL ? step_name : "<step>");
            LLVMBuildStore(ctx->builder,
                LLVMBuildGlobalStringPtr(ctx->builder,
                    reason,
                    llvm_tmp_name(ctx)),
                fail_reason_alloca);
            LLVMBuildCondBr(ctx->builder, cond, next_bb, fail_bb);
            LLVMPositionBuilderAtEnd(ctx->builder, next_bb);
        }

        if (on_expr_count > 0) {
            for (size_t j = 0; j < on_expr_count; j++) {
                if (on_exprs[j] != NULL)
                    (void)llvm_emit_expression(on_exprs[j], ctx);
            }
        }
        if (subintent_expr != NULL) {
            char reason[256];
            LLVMBasicBlockRef next_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "intent.subintent.ok");
            LLVMValueRef cond = llvm_emit_expression(subintent_expr, ctx);
            snprintf(reason, sizeof(reason), "intent:%s",
                step_name != NULL ? step_name : "<step>");
            LLVMBuildStore(ctx->builder,
                LLVMBuildGlobalStringPtr(ctx->builder,
                    reason,
                    llvm_tmp_name(ctx)),
                fail_reason_alloca);
            LLVMBuildCondBr(ctx->builder, cond, next_bb, fail_bb);
            LLVMPositionBuilderAtEnd(ctx->builder, next_bb);
        } else if (on_expr_count == 0) {
            size_t alias_count = dispatch_alias_count;
            if (!mir_only_intent && alias_count == 0)
                alias_count = step->data.intent_step.who_count;
            else if (mir_only_intent && alias_count == 0
                     && llvm_mir_intent_has_stmt(mir_routine, step_name, "IntentDispatch", NULL))
                goto mir_step_missing_dispatch;
            for (size_t j = 0; j < alias_count; j++) {
                const char *alias = dispatch_alias_count > 0
                    ? dispatch_aliases[j]
                    : step->data.intent_step.who_names[j];
                const char *subject_name = llvm_lookup_var_class(ctx, alias);
                if (subject_name != NULL) {
                    char full_name[256];
                    LLVMFuncEntry *action_fn;
                    LLVMVarEntry *participant_var = llvm_scope_lookup(ctx, alias);
                    snprintf(full_name, sizeof(full_name), "%s_%s",
                        subject_name, step_name);
                    action_fn = llvm_lookup_function(ctx, full_name);
                    if (action_fn != NULL && action_fn->is_action
                        && action_fn->action_self_only && participant_var != NULL) {
                        LLVMValueRef participant_ptr = LLVMBuildLoad2(ctx->builder,
                            participant_var->type, participant_var->alloca, llvm_tmp_name(ctx));
                        LLVMValueRef args[] = { participant_ptr };
                        if (action_fn->ret_type == ctx->type_void) {
                            LLVMBuildCall2(ctx->builder, action_fn->fn_type, action_fn->fn,
                                args, 1, "");
                        } else {
                            (void)LLVMBuildCall2(ctx->builder, action_fn->fn_type, action_fn->fn,
                                args, 1, llvm_tmp_name(ctx));
                        }
                    }
                }
            }
        }
        if (rebound_aliases)
            llvm_emit_intent_step_sync_effective_zone(ctx, zone_type_name, zone_alias);
        else
            llvm_emit_intent_step_bind_bound_zone(
                ctx, node, zone_type_name, zone_alias, from_alias, who_aliases, who_alias_count);
        if (rebound_aliases)
            llvm_emit_intent_step_restore_bound_zone_aliases(
                ctx, node, zone_type_name, who_aliases, who_alias_count, saved_participant_ptrs);

        if (completed_allocas != NULL) {
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0), completed_allocas[i]);
        }

        if (guard_expr != NULL) {
            char reason[256];
            LLVMBasicBlockRef next_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "intent.guard.ok");
            LLVMValueRef cond = llvm_emit_expression(guard_expr, ctx);
            snprintf(reason, sizeof(reason), "guard:%s",
                step_name != NULL ? step_name : "<step>");
            LLVMBuildStore(ctx->builder,
                LLVMBuildGlobalStringPtr(ctx->builder,
                    reason,
                    llvm_tmp_name(ctx)),
                fail_reason_alloca);
            LLVMBuildCondBr(ctx->builder, cond, next_bb, fail_bb);
            LLVMPositionBuilderAtEnd(ctx->builder, next_bb);
        }

        if (expect_expr != NULL) {
            char reason[256];
            LLVMBasicBlockRef next_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "intent.expect.ok");
            LLVMValueRef cond = llvm_emit_expression(expect_expr, ctx);
            snprintf(reason, sizeof(reason), "expect:%s",
                step_name != NULL ? step_name : "<step>");
            LLVMBuildStore(ctx->builder,
                LLVMBuildGlobalStringPtr(ctx->builder,
                    reason,
                    llvm_tmp_name(ctx)),
                fail_reason_alloca);
            LLVMBuildCondBr(ctx->builder, cond, next_bb, fail_bb);
            LLVMPositionBuilderAtEnd(ctx->builder, next_bb);
        }

        if (post_expr != NULL) {
            char reason[256];
            LLVMBasicBlockRef next_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "intent.post.ok");
            LLVMValueRef cond = llvm_emit_expression(post_expr, ctx);
            snprintf(reason, sizeof(reason), "post:%s",
                step_name != NULL ? step_name : "<step>");
            LLVMBuildStore(ctx->builder,
                LLVMBuildGlobalStringPtr(ctx->builder,
                    reason,
                    llvm_tmp_name(ctx)),
                fail_reason_alloca);
            LLVMBuildCondBr(ctx->builder, cond, next_bb, fail_bb);
            LLVMPositionBuilderAtEnd(ctx->builder, next_bb);
        }

        if (invariant_post_expr != NULL) {
            char reason[256];
            LLVMBasicBlockRef next_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "intent.invariant.post.ok");
            LLVMValueRef cond = llvm_emit_expression(invariant_post_expr, ctx);
            snprintf(reason, sizeof(reason), "invariant-post:%s",
                step_name != NULL ? step_name : "<step>");
            LLVMBuildStore(ctx->builder,
                LLVMBuildGlobalStringPtr(ctx->builder,
                    reason,
                    llvm_tmp_name(ctx)),
                fail_reason_alloca);
            LLVMBuildCondBr(ctx->builder, cond, next_bb, fail_bb);
            LLVMPositionBuilderAtEnd(ctx->builder, next_bb);
        }
        free(on_exprs);
        if (mir_only_intent && who_aliases != NULL)
            free((void *)who_aliases);
        if (mir_only_intent && dispatch_aliases != NULL)
            free((void *)dispatch_aliases);
        free(saved_participant_ptrs);
        saved_participant_ptrs = NULL;
        {
            LLVMValueRef handle = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
                handle_alloca, llvm_tmp_name(ctx));
            LLVMValueRef args[] = {
                handle,
                LLVMBuildGlobalStringPtr(ctx->builder,
                    step_name != NULL ? step_name : "<step>",
                    llvm_tmp_name(ctx))
            };
            LLVMBuildCall2(ctx->builder, trace_step_ok_fn->fn_type, trace_step_ok_fn->fn, args, 2, "");
        }
    }

    {
        LLVMValueRef success = node->data.intent_decl.success_expr != NULL
            ? llvm_emit_expression(node->data.intent_decl.success_expr, ctx)
            : LLVMConstInt(ctx->type_i1, 1, 0);
        LLVMBuildStore(ctx->builder, success, result_alloca);
        LLVMBuildBr(ctx->builder, cleanup_bb);
    }

    LLVMPositionBuilderAtEnd(ctx->builder, fail_bb);
    {
        LLVMValueRef handle = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
            handle_alloca, llvm_tmp_name(ctx));
        LLVMValueRef reason = LLVMBuildLoad2(ctx->builder, ctx->type_i8ptr,
            fail_reason_alloca, llvm_tmp_name(ctx));
        LLVMValueRef trace_args[] = { handle, reason };
        LLVMBuildCall2(ctx->builder, trace_fail_fn->fn_type, trace_fail_fn->fn, trace_args, 2, "");
        LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0), failed_alloca);
        LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0), result_alloca);
        LLVMBuildBr(ctx->builder, cleanup_bb);
    }

    LLVMPositionBuilderAtEnd(ctx->builder, cleanup_bb);
    {
        if (mir_routine != NULL && mir_routine->has_cleanup_block) {
            const MIRBasicBlock *cleanup_block = &mir_routine->blocks[mir_routine->cleanup_block];
            LLVMValueRef handle = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
                handle_alloca, llvm_tmp_name(ctx));
            for (size_t i = 0; i < cleanup_block->instruction_count; i++) {
                const MIRInstruction *inst = &cleanup_block->instructions[i];
                if (inst->kind == MIR_INST_CLEANUP_EDGE || inst->kind == MIR_INST_RESOURCE_OP)
                    llvm_emit_mir_resource_hook(ctx, inst, handle, true);
            }
        }
        LLVMValueRef failed = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
            failed_alloca, llvm_tmp_name(ctx));
        if (mir_routine != NULL && mir_routine->has_rollback_block)
            LLVMBuildCondBr(ctx->builder, failed, compensate_bb, maybe_exit_bb);
        else
            LLVMBuildBr(ctx->builder, maybe_exit_bb);
    }

    LLVMPositionBuilderAtEnd(ctx->builder, compensate_bb);
    if (mir_routine != NULL && mir_routine->has_rollback_block) {
        const MIRBasicBlock *rollback_block = &mir_routine->blocks[mir_routine->rollback_block];
        LLVMValueRef handle = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
            handle_alloca, llvm_tmp_name(ctx));
        for (size_t i = 0; i < rollback_block->instruction_count; i++) {
            const MIRInstruction *inst = &rollback_block->instructions[i];
            if (inst->kind == MIR_INST_CLEANUP_EDGE || inst->kind == MIR_INST_RESOURCE_OP)
                llvm_emit_mir_resource_hook(ctx, inst, handle, true);
        }
    }
    if (completed_allocas != NULL) {
        for (size_t i = step_count; i-- > 0;) {
            ASTNode *step = step_nodes[i];
            const char *step_name = (mir_step_names != NULL) ? mir_step_names[i] : NULL;
            ASTNode **compensate_exprs = NULL;
            size_t compensate_expr_count = 0;
            const char *zone_type_name = NULL;
            const char *zone_alias = NULL;
            const char *from_alias = NULL;
            const char **who_aliases = NULL;
            size_t who_alias_count = 0;
            bool has_compensate = false;
            if (step != NULL && step->type == AST_INTENT_STEP) {
                if (step_name == NULL)
                    step_name = step->data.intent_step.name;
                has_compensate = mir_only_intent
                    ? llvm_mir_intent_has_stmt(
                        mir_routine, step_name,
                        "IntentEval", "compensate")
                    : step->data.intent_step.compensate_expr_count > 0;
            }
            if (step == NULL || step->type != AST_INTENT_STEP || !has_compensate)
                continue;
            if (mir_routine != NULL) {
                compensate_expr_count = llvm_collect_mir_intent_eval_exprs(
                    mir_routine, step_name, "compensate", &compensate_exprs);
                zone_type_name = llvm_find_mir_intent_meta_arg(
                    mir_routine, step_name, "IntentZoneWhere");
                zone_alias = llvm_find_mir_intent_meta_arg(
                    mir_routine, step_name, "IntentZoneAlias");
                from_alias = llvm_find_mir_intent_meta_arg(
                    mir_routine, step_name, "IntentZoneFrom");
                who_alias_count = llvm_collect_mir_intent_who_aliases(
                    mir_routine, step_name, &who_aliases);
            }
            if (mir_only_intent
                && llvm_mir_intent_has_stmt(mir_routine, step_name,
                                            "IntentEval", "compensate")
                && compensate_expr_count == 0)
                goto mir_step_missing_compensate;
            if (!mir_only_intent && compensate_expr_count == 0) {
                compensate_expr_count = step->data.intent_step.compensate_expr_count;
                compensate_exprs = step->data.intent_step.compensate_exprs;
            }
            if (mir_only_intent) {
                if (llvm_mir_intent_has_stmt(mir_routine, step_name, "IntentZoneWhere", NULL)
                    && zone_type_name == NULL)
                    goto mir_step_missing_zone_where;
                if (llvm_mir_intent_has_stmt(mir_routine, step_name, "IntentZoneAlias", NULL)
                    && zone_alias == NULL)
                    goto mir_step_missing_zone_alias;
                if (llvm_mir_intent_has_stmt(mir_routine, step_name, "IntentZoneFrom", NULL)
                    && from_alias == NULL)
                    goto mir_step_missing_zone_from;
                if (llvm_mir_intent_has_stmt(mir_routine, step_name, "IntentWho", NULL)
                    && who_alias_count == 0)
                    goto mir_step_missing_who;
            } else {
                if (zone_type_name == NULL
                    && step->data.intent_step.where_type != NULL
                    && step->data.intent_step.where_type->type == AST_TYPE) {
                    zone_type_name = step->data.intent_step.where_type->data.type.name;
                }
                if (zone_alias == NULL)
                    zone_alias = llvm_intent_step_effective_zone_alias(step);
                if (from_alias == NULL)
                    from_alias = step->data.intent_step.transfer_from_alias;
                if (who_alias_count == 0) {
                    who_alias_count = step->data.intent_step.who_count;
                    who_aliases = (const char **)step->data.intent_step.who_names;
                }
            }
            {
                LLVMBasicBlockRef do_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "intent.comp.do");
                LLVMBasicBlockRef next_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "intent.comp.next");
                LLVMValueRef done = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
                    completed_allocas[i], llvm_tmp_name(ctx));
                LLVMBuildCondBr(ctx->builder, done, do_bb, next_bb);
                LLVMPositionBuilderAtEnd(ctx->builder, do_bb);
                for (size_t j = compensate_expr_count; j-- > 0;) {
                    if (compensate_exprs[j] != NULL)
                        (void)llvm_emit_expression(compensate_exprs[j], ctx);
                }
                llvm_emit_intent_step_bind_bound_zone(
                    ctx, node, zone_type_name, zone_alias, from_alias, who_aliases, who_alias_count);
                if (node->data.intent_decl.rollback_policy == INTENT_ROLLBACK_CURRENT)
                    LLVMBuildBr(ctx->builder, maybe_exit_bb);
                else
                    LLVMBuildBr(ctx->builder, next_bb);
                LLVMPositionBuilderAtEnd(ctx->builder, next_bb);
            }
            if (mir_only_intent && compensate_exprs != NULL) {
                free(compensate_exprs);
            }
            if (mir_only_intent && who_aliases != NULL)
                free((void *)who_aliases);
        }
    }
    LLVMBuildBr(ctx->builder, maybe_exit_bb);

    LLVMPositionBuilderAtEnd(ctx->builder, maybe_exit_bb);
    {
        if (mir_routine != NULL && mir_routine->has_invalidation_block) {
            const MIRBasicBlock *invalidation_block = &mir_routine->blocks[mir_routine->invalidation_block];
            LLVMValueRef hook_handle = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
                handle_alloca, llvm_tmp_name(ctx));
            for (size_t i = 0; i < invalidation_block->instruction_count; i++) {
                const MIRInstruction *inst = &invalidation_block->instructions[i];
                if (inst->kind == MIR_INST_CLEANUP_EDGE || inst->kind == MIR_INST_RESOURCE_OP)
                    llvm_emit_mir_resource_hook(ctx, inst, hook_handle, true);
            }
        }
        LLVMValueRef handle = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
            handle_alloca, llvm_tmp_name(ctx));
        LLVMValueRef entered = LLVMBuildICmp(ctx->builder, LLVMIntNE, handle,
            LLVMConstInt(ctx->type_i32, 0, 0), llvm_tmp_name(ctx));
        LLVMBuildCondBr(ctx->builder, entered, do_exit_bb, ret_bb);
    }

    LLVMPositionBuilderAtEnd(ctx->builder, do_exit_bb);
    {
        LLVMValueRef handle = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
            handle_alloca, llvm_tmp_name(ctx));
        LLVMValueRef exit_args[] = { handle };
        LLVMBuildCall2(ctx->builder, exit_fn->fn_type, exit_fn->fn, exit_args, 1, "");
        LLVMBuildBr(ctx->builder, ret_bb);
    }

    LLVMPositionBuilderAtEnd(ctx->builder, ret_bb);
    {
        LLVMValueRef result = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
            result_alloca, llvm_tmp_name(ctx));
        LLVMBuildRet(ctx->builder, result);
    }

    llvm_scope_pop(ctx);
    free(completed_allocas);
    free((void *)participant_aliases);
    free((void *)participant_types);
    free(mir_steps);
    free((void *)mir_step_names);
    ctx->current_function = saved_fn;
    ctx->current_ret_type = saved_ret_type;

    if (saved_fn != NULL) {
        LLVMBasicBlockRef last = LLVMGetLastBasicBlock(saved_fn);
        if (last != NULL)
            LLVMPositionBuilderAtEnd(ctx->builder, last);
    }
    return;

mir_step_missing_pre:
    llvm_set_error_with_code(ctx, "PGY_MIR_INTENT_CARRIER_MISSING",
        "MIR-only LLVM path missing intent pre check carrier");
    goto intent_emit_fail;
mir_step_missing_guard:
    llvm_set_error_with_code(ctx, "PGY_MIR_INTENT_CARRIER_MISSING",
        "MIR-only LLVM path missing intent guard check carrier");
    goto intent_emit_fail;
mir_step_missing_post:
    llvm_set_error_with_code(ctx, "PGY_MIR_INTENT_CARRIER_MISSING",
        "MIR-only LLVM path missing intent post check carrier");
    goto intent_emit_fail;
mir_step_missing_expect:
    llvm_set_error_with_code(ctx, "PGY_MIR_INTENT_CARRIER_MISSING",
        "MIR-only LLVM path missing intent expect check carrier");
    goto intent_emit_fail;
mir_step_missing_invariant:
    llvm_set_error_with_code(ctx, "PGY_MIR_INTENT_CARRIER_MISSING",
        "MIR-only LLVM path missing intent invariant check carrier");
    goto intent_emit_fail;
mir_step_missing_subintent:
    llvm_set_error_with_code(ctx, "PGY_MIR_INTENT_CARRIER_MISSING",
        "MIR-only LLVM path missing intent subintent eval carrier");
    goto intent_emit_fail;
mir_step_missing_on:
    llvm_set_error_with_code(ctx, "PGY_MIR_INTENT_CARRIER_MISSING",
        "MIR-only LLVM path missing intent on-eval carrier");
    goto intent_emit_fail;
mir_step_missing_zone_where:
    llvm_set_error_with_code(ctx, "PGY_MIR_INTENT_CARRIER_MISSING",
        "MIR-only LLVM path missing intent zone where metadata");
    goto intent_emit_fail;
mir_step_missing_zone_alias:
    llvm_set_error_with_code(ctx, "PGY_MIR_INTENT_CARRIER_MISSING",
        "MIR-only LLVM path missing intent zone alias metadata");
    goto intent_emit_fail;
mir_step_missing_zone_from:
    llvm_set_error_with_code(ctx, "PGY_MIR_INTENT_CARRIER_MISSING",
        "MIR-only LLVM path missing intent transfer-from metadata");
    goto intent_emit_fail;
mir_step_missing_who:
    llvm_set_error_with_code(ctx, "PGY_MIR_INTENT_CARRIER_MISSING",
        "MIR-only LLVM path missing intent who metadata");
    goto intent_emit_fail;
mir_step_missing_dispatch:
    llvm_set_error_with_code(ctx, "PGY_MIR_INTENT_CARRIER_MISSING",
        "MIR-only LLVM path missing intent dispatch carrier");
    goto intent_emit_fail;
mir_step_missing_compensate:
    llvm_set_error_with_code(ctx, "PGY_MIR_INTENT_CARRIER_MISSING",
        "MIR-only LLVM path missing intent compensate eval carrier");
    goto intent_emit_fail;

intent_emit_fail:
    free(completed_allocas);
    free((void *)participant_aliases);
    free((void *)participant_types);
    free(mir_steps);
    free((void *)mir_step_names);
    ctx->current_function = saved_fn;
    ctx->current_ret_type = saved_ret_type;
    if (saved_fn != NULL) {
        LLVMBasicBlockRef last = LLVMGetLastBasicBlock(saved_fn);
        if (last != NULL)
            LLVMPositionBuilderAtEnd(ctx->builder, last);
    }
}

#endif
