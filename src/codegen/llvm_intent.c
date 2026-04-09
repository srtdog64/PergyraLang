/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM backend — intent declaration helpers and HIR fallback emission
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

static ASTNode *
llvm_find_intent_actor_local(ASTNode *intent, const char *alias)
{
    if (intent == NULL || intent->type != AST_INTENT_DECL || alias == NULL)
        return NULL;
    for (size_t i = 0; i < intent->data.intent_decl.involve_count; i++) {
        ASTNode *involves = intent->data.intent_decl.involves[i];
        if (involves != NULL && involves->type == AST_INTENT_INVOLVES
            && involves->data.intent_involves.alias != NULL
            && strcmp(involves->data.intent_involves.alias, alias) == 0) {
            return involves;
        }
    }
    return NULL;
}

static ASTNode *
llvm_find_zone_decl_in_hir(LLVMGenCtx *ctx, const char *zone_name)
{
    if (ctx == NULL || ctx->hir == NULL || zone_name == NULL)
        return NULL;

    for (size_t i = 0; i < ctx->hir->zone_count; i++) {
        ASTNode *zone = ctx->hir->zones[i];
        if (zone != NULL && zone->type == AST_ZONE_DECL
            && zone->data.zone_decl.name != NULL
            && strcmp(zone->data.zone_decl.name, zone_name) == 0) {
            return zone;
        }
    }
    return NULL;
}

static const char *
llvm_intent_actor_type_name(ASTNode *intent, const char *alias)
{
    ASTNode *involves = llvm_find_intent_actor_local(intent, alias);
    if (involves != NULL
        && involves->data.intent_involves.subject_type != NULL
        && involves->data.intent_involves.subject_type->type == AST_TYPE) {
        return involves->data.intent_involves.subject_type->data.type.name;
    }
    return NULL;
}

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
llvm_intent_involves_is_subject_participant(LLVMGenCtx *ctx, ASTNode *involves)
{
    const char *type_name = llvm_intent_involves_type_name(involves);

    if (ctx == NULL || type_name == NULL || ctx->hir == NULL)
        return false;

    for (size_t i = 0; i < ctx->hir->item_count; i++) {
        ASTNode *stmt = ctx->hir->items[i].ast;
        if (stmt == NULL)
            continue;
        if (stmt->type == AST_CLASS_DECL
            && stmt->data.class_decl.name != NULL
            && strcmp(stmt->data.class_decl.name, type_name) == 0
            && stmt->data.class_decl.nominal_kind == NOMINAL_DECL_SUBJECT) {
            return true;
        }
        if (stmt->type == AST_ACTOR_DECL
            && stmt->data.actor_decl.name != NULL
            && strcmp(stmt->data.actor_decl.name, type_name) == 0) {
            return true;
        }
    }
    return false;
}

bool
llvm_intent_involves_uses_pointer_self(LLVMGenCtx *ctx, ASTNode *involves)
{
    const char *type_name = llvm_intent_involves_type_name(involves);
    if (ctx == NULL || type_name == NULL || ctx->hir == NULL)
        return false;

    {
        LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, type_name);
        if (cls != NULL && cls->is_pointer_self_host)
            return true;
    }

    for (size_t i = 0; i < ctx->hir->item_count; i++) {
        ASTNode *stmt = ctx->hir->items[i].ast;
        if (stmt == NULL)
            continue;
        switch (stmt->type) {
        case AST_CLASS_DECL:
            if (stmt->data.class_decl.name != NULL
                && strcmp(stmt->data.class_decl.name, type_name) == 0
                && stmt->data.class_decl.nominal_kind == NOMINAL_DECL_VESSEL)
                return true;
            break;
        case AST_ACTOR_DECL:
        case AST_PARTY_DECL:
        case AST_SYSTEMIC_DECL:
        case AST_WORLD_DECL:
        case AST_RELATION_DECL:
        case AST_EFFECT_DECL:
        case AST_ZONE_DECL:
            if (((stmt->type == AST_ACTOR_DECL) && stmt->data.actor_decl.name != NULL
                    && strcmp(stmt->data.actor_decl.name, type_name) == 0)
                || ((stmt->type == AST_PARTY_DECL) && stmt->data.party_decl.name != NULL
                    && strcmp(stmt->data.party_decl.name, type_name) == 0)
                || ((stmt->type == AST_SYSTEMIC_DECL) && stmt->data.systemic_decl.name != NULL
                    && strcmp(stmt->data.systemic_decl.name, type_name) == 0)
                || ((stmt->type == AST_WORLD_DECL) && stmt->data.world_decl.name != NULL
                    && strcmp(stmt->data.world_decl.name, type_name) == 0)
                || ((stmt->type == AST_RELATION_DECL) && stmt->data.relation_decl.name != NULL
                    && strcmp(stmt->data.relation_decl.name, type_name) == 0)
                || ((stmt->type == AST_EFFECT_DECL) && stmt->data.effect_decl.name != NULL
                    && strcmp(stmt->data.effect_decl.name, type_name) == 0)
                || ((stmt->type == AST_ZONE_DECL) && stmt->data.zone_decl.name != NULL
                    && strcmp(stmt->data.zone_decl.name, type_name) == 0)) {
                return true;
            }
            break;
        default:
            break;
        }
    }

    return false;
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
llvm_intent_zone_binding_type_name(ASTNode *intent, const char *alias)
{
    ASTNode *involves = llvm_find_intent_actor_local(intent, alias);
    if (involves != NULL
        && involves->data.intent_involves.subject_type != NULL
        && involves->data.intent_involves.subject_type->type == AST_TYPE) {
        return involves->data.intent_involves.subject_type->data.type.name;
    }
    return NULL;
}

static const char *
llvm_resolve_intent_zone_slot_name(LLVMGenCtx *ctx, ASTNode *intent,
                                   ASTNode *step, const char *alias)
{
    if (ctx == NULL || intent == NULL || step == NULL || alias == NULL
        || step->data.intent_step.where_type == NULL
        || step->data.intent_step.where_type->type != AST_TYPE
        || step->data.intent_step.where_type->data.type.name == NULL) {
        return "<unbound>";
    }
    return llvm_resolve_intent_zone_slot_name_for_zone(ctx, intent,
        step->data.intent_step.where_type->data.type.name, alias);
}

static const char *
llvm_resolve_intent_zone_slot_name_for_zone(LLVMGenCtx *ctx, ASTNode *intent,
                                            const char *zone_type_name, const char *alias)
{
    ASTNode *zone_decl = NULL;
    const char *actor_type = NULL;
    ASTNode *named_match = NULL;
    ASTNode *typed_match = NULL;

    if (ctx == NULL || intent == NULL || zone_type_name == NULL || alias == NULL) {
        return "<unbound>";
    }

    zone_decl = llvm_find_zone_decl_in_hir(ctx, zone_type_name);
    actor_type = llvm_intent_actor_type_name(intent, alias);
    if (zone_decl == NULL)
        return "<unbound>";

    for (size_t i = 0; i < zone_decl->data.zone_decl.slot_count; i++) {
        ASTNode *slot = zone_decl->data.zone_decl.slots[i];
        if (slot == NULL || slot->type != AST_DOMAIN_SLOT
            || !slot->data.domain_slot.is_subject
            || slot->data.domain_slot.slot_name == NULL) {
            continue;
        }
        if (strcmp(slot->data.domain_slot.slot_name, alias) == 0) {
            named_match = slot;
            break;
        }
        if (actor_type != NULL
            && slot->data.domain_slot.type != NULL
            && slot->data.domain_slot.type->type == AST_TYPE
            && slot->data.domain_slot.type->data.type.name != NULL
            && strcmp(slot->data.domain_slot.type->data.type.name, actor_type) == 0) {
            if (typed_match != NULL)
                typed_match = (ASTNode *)(uintptr_t)1;
            else
                typed_match = slot;
        }
    }

    if (named_match != NULL)
        return named_match->data.domain_slot.slot_name;
    if (typed_match != NULL && typed_match != (ASTNode *)(uintptr_t)1)
        return typed_match->data.domain_slot.slot_name;
    return "<unbound>";
}

static void
llvm_emit_intent_step_bind_bound_zone(LLVMGenCtx *ctx, ASTNode *intent, ASTNode *step)
{
    const char *zone_alias;
    const char *zone_type_name;
    const char *from_alias;
    const char *from_zone_type_name;
    LLVMVarEntry *zone_var;
    LLVMValueRef zone_ptr;
    char sync_name[256];
    LLVMFuncEntry *sync_fn;
    LLVMClassTypeEntry *zone_cls;
    LLVMFuncEntry *trace_materialize_fn;
    LLVMFuncEntry *trace_transfer_fn;

    if (ctx == NULL || step == NULL || step->type != AST_INTENT_STEP
        || step->data.intent_step.where_type == NULL
        || step->data.intent_step.where_type->type != AST_TYPE) {
        return;
    }

    zone_alias = llvm_intent_step_effective_zone_alias(step);
    zone_type_name = step->data.intent_step.where_type->data.type.name;
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
    from_alias = step->data.intent_step.transfer_from_alias;
    from_zone_type_name = llvm_intent_zone_binding_type_name(intent, from_alias);

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
            for (size_t i = 0; i < step->data.intent_step.who_count; i++) {
                const char *alias = step->data.intent_step.who_names[i];
                const char *from_slot_name = llvm_resolve_intent_zone_slot_name_for_zone(
                    ctx, intent, from_zone_type_name, alias);
                const char *to_slot_name = llvm_resolve_intent_zone_slot_name_for_zone(
                    ctx, intent, zone_type_name, alias);
                ASTNode *involves;
                LLVMVarEntry *actor_var;
                LLVMTypeRef actor_ptr_type;
                LLVMTypeRef actor_value_type;
                LLVMValueRef actor_ptr;
                LLVMValueRef actor_value;
                LLVMValueRef handle;

                if (alias == NULL)
                    continue;

                involves = llvm_find_intent_actor_local(intent, alias);
                actor_var = llvm_scope_lookup(ctx, alias);
                if (involves == NULL || actor_var == NULL
                    || involves->data.intent_involves.subject_type == NULL) {
                    continue;
                }

                actor_ptr_type = actor_var->type;
                actor_value_type = ast_type_to_llvm(ctx, involves->data.intent_involves.subject_type);
                actor_ptr = LLVMBuildLoad2(ctx->builder, actor_ptr_type, actor_var->alloca,
                    llvm_tmp_name(ctx));
                actor_value = LLVMBuildLoad2(ctx->builder, actor_value_type, actor_ptr,
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
                        LLVMBuildStore(ctx->builder, actor_value, from_slot_ptr);
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
                        LLVMBuildStore(ctx->builder, actor_value, to_slot_ptr);
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
        for (size_t i = 0; i < step->data.intent_step.who_count; i++) {
            const char *alias = step->data.intent_step.who_names[i];
            const char *slot_name = llvm_resolve_intent_zone_slot_name(ctx, intent, step, alias);
            ASTNode *involves;
            LLVMVarEntry *actor_var;
            LLVMTypeRef actor_ptr_type;
            LLVMTypeRef actor_value_type;
            LLVMValueRef actor_ptr;
            LLVMValueRef actor_value;
            LLVMValueRef slot_ptr;
            int field_idx;

            if (alias == NULL || slot_name == NULL || strcmp(slot_name, "<unbound>") == 0)
                continue;

            field_idx = llvm_class_field_index(zone_cls, slot_name);
            if (field_idx < 0)
                continue;

            involves = llvm_find_intent_actor_local(intent, alias);
            actor_var = llvm_scope_lookup(ctx, alias);
            if (involves == NULL || actor_var == NULL
                || involves->data.intent_involves.subject_type == NULL) {
                continue;
            }

            actor_ptr_type = actor_var->type;
            actor_value_type = ast_type_to_llvm(ctx, involves->data.intent_involves.subject_type);
            actor_ptr = LLVMBuildLoad2(ctx->builder, actor_ptr_type, actor_var->alloca,
                llvm_tmp_name(ctx));
            actor_value = LLVMBuildLoad2(ctx->builder, actor_value_type, actor_ptr,
                llvm_tmp_name(ctx));
            slot_ptr = LLVMBuildStructGEP2(ctx->builder, zone_cls->struct_type, zone_ptr,
                (unsigned)field_idx, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, actor_value, slot_ptr);

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
llvm_emit_intent_step_rebind_bound_zone_aliases(LLVMGenCtx *ctx, ASTNode *intent,
                                                ASTNode *step, LLVMValueRef *saved_allocas)
{
    const char *zone_alias;
    const char *zone_type_name;
    LLVMVarEntry *zone_var;
    LLVMValueRef zone_ptr;
    LLVMClassTypeEntry *zone_cls;
    bool rebound = false;

    if (ctx == NULL || intent == NULL || step == NULL || saved_allocas == NULL
        || step->type != AST_INTENT_STEP
        || step->data.intent_step.where_type == NULL
        || step->data.intent_step.where_type->type != AST_TYPE) {
        return false;
    }

    zone_alias = llvm_intent_step_effective_zone_alias(step);
    zone_type_name = step->data.intent_step.where_type->data.type.name;
    if (zone_alias == NULL || zone_type_name == NULL)
        return false;

    zone_var = llvm_scope_lookup(ctx, zone_alias);
    if (zone_var == NULL || LLVMGetTypeKind(zone_var->type) != LLVMPointerTypeKind)
        return false;

    zone_ptr = LLVMBuildLoad2(ctx->builder, zone_var->type, zone_var->alloca, llvm_tmp_name(ctx));
    zone_cls = llvm_lookup_class(ctx, zone_type_name);
    if (zone_cls == NULL)
        return false;

    for (size_t i = 0; i < step->data.intent_step.who_count; i++) {
        const char *alias = step->data.intent_step.who_names[i];
        const char *slot_name = llvm_resolve_intent_zone_slot_name_for_zone(ctx, intent, zone_type_name, alias);
        LLVMVarEntry *actor_var;
        int field_idx;
        LLVMValueRef original_ptr;
        LLVMValueRef slot_ptr;

        if (alias == NULL || slot_name == NULL || strcmp(slot_name, "<unbound>") == 0)
            continue;

        actor_var = llvm_scope_lookup(ctx, alias);
        if (actor_var == NULL || LLVMGetTypeKind(actor_var->type) != LLVMPointerTypeKind)
            continue;

        field_idx = llvm_class_field_index(zone_cls, slot_name);
        if (field_idx < 0)
            continue;

        original_ptr = LLVMBuildLoad2(ctx->builder, actor_var->type, actor_var->alloca, llvm_tmp_name(ctx));
        saved_allocas[i] = LLVMBuildAlloca(ctx->builder, actor_var->type, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, original_ptr, saved_allocas[i]);
        slot_ptr = LLVMBuildStructGEP2(ctx->builder, zone_cls->struct_type, zone_ptr,
            (unsigned)field_idx, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, slot_ptr, actor_var->alloca);
        rebound = true;
    }

    return rebound;
}

static void
llvm_emit_intent_step_sync_effective_zone(LLVMGenCtx *ctx, ASTNode *step)
{
    const char *zone_alias;
    const char *zone_type_name;
    LLVMVarEntry *zone_var;
    LLVMValueRef zone_ptr;
    char sync_name[256];
    LLVMFuncEntry *sync_fn;

    if (ctx == NULL || step == NULL || step->type != AST_INTENT_STEP
        || step->data.intent_step.where_type == NULL
        || step->data.intent_step.where_type->type != AST_TYPE) {
        return;
    }

    zone_alias = llvm_intent_step_effective_zone_alias(step);
    zone_type_name = step->data.intent_step.where_type->data.type.name;
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
llvm_emit_intent_step_restore_bound_zone_aliases(LLVMGenCtx *ctx, ASTNode *intent,
                                                 ASTNode *step, LLVMValueRef *saved_allocas)
{
    const char *zone_type_name;

    if (ctx == NULL || intent == NULL || step == NULL || saved_allocas == NULL
        || step->type != AST_INTENT_STEP
        || step->data.intent_step.where_type == NULL
        || step->data.intent_step.where_type->type != AST_TYPE) {
        return;
    }

    zone_type_name = step->data.intent_step.where_type->data.type.name;
    if (zone_type_name == NULL)
        return;

    for (size_t i = 0; i < step->data.intent_step.who_count; i++) {
        const char *alias = step->data.intent_step.who_names[i];
        const char *slot_name = llvm_resolve_intent_zone_slot_name_for_zone(ctx, intent, zone_type_name, alias);
        ASTNode *involves;
        LLVMVarEntry *actor_var;
        LLVMTypeRef actor_ptr_type;
        LLVMTypeRef actor_value_type;
        LLVMValueRef zone_bound_ptr;
        LLVMValueRef zone_bound_value;
        LLVMValueRef saved_ptr;

        if (saved_allocas[i] == NULL || alias == NULL || slot_name == NULL
            || strcmp(slot_name, "<unbound>") == 0) {
            continue;
        }

        actor_var = llvm_scope_lookup(ctx, alias);
        involves = llvm_find_intent_actor_local(intent, alias);
        if (actor_var == NULL || involves == NULL
            || involves->data.intent_involves.subject_type == NULL) {
            continue;
        }

        actor_ptr_type = actor_var->type;
        actor_value_type = ast_type_to_llvm(ctx, involves->data.intent_involves.subject_type);
        zone_bound_ptr = LLVMBuildLoad2(ctx->builder, actor_ptr_type, actor_var->alloca, llvm_tmp_name(ctx));
        zone_bound_value = LLVMBuildLoad2(ctx->builder, actor_value_type, zone_bound_ptr, llvm_tmp_name(ctx));
        saved_ptr = LLVMBuildLoad2(ctx->builder, actor_ptr_type, saved_allocas[i], llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, zone_bound_value, saved_ptr);
        LLVMBuildStore(ctx->builder, saved_ptr, actor_var->alloca);
    }
}

static ASTNode *
llvm_find_subject_action_decl(LLVMGenCtx *ctx, const char *subject_name, const char *action_name)
{
    if (ctx == NULL || ctx->hir == NULL || subject_name == NULL || action_name == NULL)
        return NULL;
    for (size_t i = 0; i < ctx->hir->type_count; i++) {
        ASTNode *decl = ctx->hir->types[i];
        if (decl == NULL || decl->type != AST_CLASS_DECL
            || decl->data.class_decl.nominal_kind != NOMINAL_DECL_SUBJECT
            || decl->data.class_decl.name == NULL
            || strcmp(decl->data.class_decl.name, subject_name) != 0) {
            continue;
        }
        for (size_t j = 0; j < decl->data.class_decl.method_count; j++) {
            ASTNode *method = decl->data.class_decl.methods[j];
            if (method != NULL && method->type == AST_FUNC_DECL
                && method->data.func_decl.is_action
                && method->data.func_decl.name != NULL
                && strcmp(method->data.func_decl.name, action_name) == 0) {
                return method;
            }
        }
    }
    return NULL;
}

static bool
llvm_intent_action_has_only_self(ASTNode *action_decl)
{
    size_t real_pc = 0;
    if (action_decl == NULL || action_decl->type != AST_FUNC_DECL)
        return false;
    for (size_t i = 0; i < action_decl->data.func_decl.param_count; i++) {
        FuncParam *p = action_decl->data.func_decl.params[i];
        if (p == NULL || p->name == NULL)
            continue;
        if (p->type == NULL && strcmp(p->name, "self") == 0)
            continue;
        real_pc++;
    }
    return real_pc == 0;
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

void
llvm_forward_declare_intent(ASTNode *node, LLVMGenCtx *ctx)
{
    const char *name;
    LLVMTypeRef *param_types = NULL;
    LLVMTypeRef fn_type;
    LLVMValueRef fn;

    if (node == NULL || node->type != AST_INTENT_DECL || ctx == NULL)
        return;
    name = node->data.intent_decl.name;
    if (name == NULL || llvm_lookup_function(ctx, name) != NULL)
        return;

    if (node->data.intent_decl.involve_count > 0) {
        param_types = calloc(node->data.intent_decl.involve_count, sizeof(LLVMTypeRef));
        for (size_t i = 0; i < node->data.intent_decl.involve_count; i++) {
            ASTNode *involves = node->data.intent_decl.involves[i];
            LLVMTypeRef pt = ctx->type_i32;
            if (involves != NULL && involves->data.intent_involves.subject_type != NULL) {
                pt = ast_type_to_llvm(ctx, involves->data.intent_involves.subject_type);
                if (llvm_intent_involves_uses_pointer_self(ctx, involves))
                    pt = LLVMPointerType(pt, 0);
            }
            param_types[i] = pt;
        }
    }

    fn_type = LLVMFunctionType(ctx->type_i1, param_types,
        (unsigned)node->data.intent_decl.involve_count, 0);
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
    free(param_types);
}

void
llvm_emit_intent_decl(ASTNode *node, LLVMGenCtx *ctx)
{
    const MIRRoutine *mir_routine;
    ASTNode **mir_steps = NULL;
    ASTNode **step_nodes = NULL;
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
    size_t subject_count = 0;
    size_t step_count = 0;
    bool has_compensate_steps = false;

    if (node == NULL || node->type != AST_INTENT_DECL || ctx == NULL)
        return;
    mir_routine = llvm_find_mir_intent_routine(ctx, node);
    if (mir_routine != NULL)
        step_count = llvm_collect_mir_intent_steps(mir_routine, &mir_steps);
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
            return;
        }
    }
    if (step_count > 0) {
        step_nodes = mir_steps;
    } else {
        step_count = node->data.intent_decl.step_count;
        step_nodes = node->data.intent_decl.steps;
    }

    for (size_t i = 0; i < step_count; i++) {
        ASTNode *step = step_nodes[i];
        if (step != NULL && step->type == AST_INTENT_STEP
            && step->data.intent_step.compensate_expr_count > 0) {
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

    for (size_t i = 0; i < node->data.intent_decl.involve_count; i++) {
        ASTNode *involves = node->data.intent_decl.involves[i];
        const char *alias = involves != NULL ? involves->data.intent_involves.alias : NULL;
        const char *type_name = (involves != NULL
            && involves->data.intent_involves.subject_type != NULL
            && involves->data.intent_involves.subject_type->type == AST_TYPE)
            ? involves->data.intent_involves.subject_type->data.type.name : NULL;
        LLVMTypeRef pt = ctx->type_i8ptr;
        if (involves != NULL && involves->data.intent_involves.subject_type != NULL) {
            pt = ast_type_to_llvm(ctx, involves->data.intent_involves.subject_type);
            if (llvm_intent_involves_uses_pointer_self(ctx, involves))
                pt = LLVMPointerType(pt, 0);
        }
        LLVMValueRef a = llvm_create_entry_alloca(ctx, pt, alias != NULL ? alias : "actor");
        LLVMBuildStore(ctx->builder, LLVMGetParam(fn, (unsigned)i), a);
        llvm_scope_declare(ctx, alias != NULL ? alias : "actor", a, pt);
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

    for (size_t i = 0; i < node->data.intent_decl.involve_count; i++) {
        if (llvm_intent_involves_is_subject_participant(ctx,
                node->data.intent_decl.involves[i])) {
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

        for (size_t i = 0; i < node->data.intent_decl.involve_count; i++) {
            ASTNode *involves = node->data.intent_decl.involves[i];
            const char *alias = involves != NULL ? involves->data.intent_involves.alias : NULL;
            LLVMVarEntry *actor_var = llvm_scope_lookup(ctx, alias != NULL ? alias : "actor");
            LLVMValueRef indices[] = {
                zero,
                LLVMConstInt(ctx->type_i32, subject_index, 0)
            };
            LLVMValueRef actor_ptr = actor_var != NULL
                ? LLVMBuildLoad2(ctx->builder, actor_var->type, actor_var->alloca, llvm_tmp_name(ctx))
                : LLVMConstPointerNull(ctx->type_i8ptr);
            LLVMValueRef cast_actor = LLVMBuildBitCast(ctx->builder, actor_ptr,
                ctx->type_i8ptr, llvm_tmp_name(ctx));
            if (!llvm_intent_involves_is_subject_participant(ctx, involves))
                continue;
            LLVMValueRef elem_ptr = LLVMBuildGEP2(ctx->builder, subject_array_type,
                subjects_alloca, indices, 2, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, cast_actor, elem_ptr);
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
        const char *step_name = NULL;
        ASTNode *pre_expr = NULL;
        ASTNode *guard_expr = NULL;
        ASTNode *post_expr = NULL;
        ASTNode *expect_expr = NULL;
        ASTNode *invariant_pre_expr = NULL;
        ASTNode *invariant_post_expr = NULL;
        LLVMValueRef *saved_actor_ptrs = NULL;
        bool rebound_aliases = false;
        if (step == NULL || step->type != AST_INTENT_STEP)
            continue;
        step_name = step->data.intent_step.name;
        if (mir_routine != NULL) {
            pre_expr = llvm_find_mir_intent_check_expr(mir_routine, step_name, "pre");
            guard_expr = llvm_find_mir_intent_check_expr(mir_routine, step_name, "guard");
            post_expr = llvm_find_mir_intent_check_expr(mir_routine, step_name, "post");
            expect_expr = llvm_find_mir_intent_check_expr(mir_routine, step_name, "expect");
            invariant_pre_expr = llvm_find_mir_intent_check_expr(mir_routine, step_name, "invariant-pre");
            invariant_post_expr = llvm_find_mir_intent_check_expr(mir_routine, step_name, "invariant-post");
        }
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

        {
            LLVMValueRef handle = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
                handle_alloca, llvm_tmp_name(ctx));
            LLVMValueRef args[] = {
                handle,
                LLVMBuildGlobalStringPtr(ctx->builder,
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                    llvm_tmp_name(ctx)),
                LLVMBuildGlobalStringPtr(ctx->builder,
                    (step->data.intent_step.where_type != NULL
                        && step->data.intent_step.where_type->type == AST_TYPE
                        && step->data.intent_step.where_type->data.type.name != NULL)
                        ? step->data.intent_step.where_type->data.type.name : "<zone>",
                    llvm_tmp_name(ctx))
            };
            LLVMBuildCall2(ctx->builder, trace_step_fn->fn_type, trace_step_fn->fn, args, 3, "");
        }
        for (size_t j = 0; j < step->data.intent_step.who_count; j++) {
            LLVMValueRef handle = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
                handle_alloca, llvm_tmp_name(ctx));
            const char *alias = step->data.intent_step.who_names[j];
            const char *slot_name = llvm_resolve_intent_zone_slot_name(ctx, node, step, alias);
            LLVMValueRef args[] = {
                handle,
                LLVMBuildGlobalStringPtr(ctx->builder, alias != NULL ? alias : "<actor>",
                    llvm_tmp_name(ctx)),
                LLVMBuildGlobalStringPtr(ctx->builder, slot_name != NULL ? slot_name : "<unbound>",
                    llvm_tmp_name(ctx))
            };
            LLVMBuildCall2(ctx->builder, trace_bind_fn->fn_type, trace_bind_fn->fn, args, 3, "");
        }
        llvm_emit_intent_step_bind_bound_zone(ctx, node, step);
        if (step->data.intent_step.who_count > 0) {
            saved_actor_ptrs = calloc(step->data.intent_step.who_count, sizeof(LLVMValueRef));
            if (saved_actor_ptrs != NULL)
                rebound_aliases = llvm_emit_intent_step_rebind_bound_zone_aliases(ctx, node, step, saved_actor_ptrs);
        }

        if (pre_expr != NULL) {
            char reason[256];
            LLVMBasicBlockRef next_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "intent.pre.ok");
            LLVMValueRef cond = llvm_emit_expression(pre_expr, ctx);
            snprintf(reason, sizeof(reason), "pre:%s",
                step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>");
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
                step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>");
            LLVMBuildStore(ctx->builder,
                LLVMBuildGlobalStringPtr(ctx->builder,
                    reason,
                    llvm_tmp_name(ctx)),
                fail_reason_alloca);
            LLVMBuildCondBr(ctx->builder, cond, next_bb, fail_bb);
            LLVMPositionBuilderAtEnd(ctx->builder, next_bb);
        }

        if (step->data.intent_step.on_expr_count > 0) {
            for (size_t j = 0; j < step->data.intent_step.on_expr_count; j++) {
                if (step->data.intent_step.on_exprs[j] != NULL)
                    (void)llvm_emit_expression(step->data.intent_step.on_exprs[j], ctx);
            }
        }
        if (step->data.intent_step.intent_expr != NULL) {
            char reason[256];
            LLVMBasicBlockRef next_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "intent.subintent.ok");
            LLVMValueRef cond = llvm_emit_expression(step->data.intent_step.intent_expr, ctx);
            snprintf(reason, sizeof(reason), "intent:%s",
                step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>");
            LLVMBuildStore(ctx->builder,
                LLVMBuildGlobalStringPtr(ctx->builder,
                    reason,
                    llvm_tmp_name(ctx)),
                fail_reason_alloca);
            LLVMBuildCondBr(ctx->builder, cond, next_bb, fail_bb);
            LLVMPositionBuilderAtEnd(ctx->builder, next_bb);
        } else if (step->data.intent_step.on_expr_count == 0) {
            for (size_t j = 0; j < step->data.intent_step.who_count; j++) {
                const char *alias = step->data.intent_step.who_names[j];
                ASTNode *involves = llvm_find_intent_actor_local(node, alias);
                const char *subject_name = (involves != NULL
                    && involves->data.intent_involves.subject_type != NULL
                    && involves->data.intent_involves.subject_type->type == AST_TYPE)
                    ? involves->data.intent_involves.subject_type->data.type.name : NULL;
                ASTNode *action_decl = llvm_find_subject_action_decl(ctx, subject_name,
                    step->data.intent_step.name);
                if (subject_name != NULL && action_decl != NULL
                    && llvm_intent_action_has_only_self(action_decl)) {
                    char full_name[256];
                    LLVMFuncEntry *action_fn;
                    LLVMVarEntry *actor_var = llvm_scope_lookup(ctx, alias);
                    snprintf(full_name, sizeof(full_name), "%s_%s",
                        subject_name, step->data.intent_step.name);
                    action_fn = llvm_lookup_function(ctx, full_name);
                    if (action_fn != NULL && actor_var != NULL) {
                        LLVMValueRef actor_ptr = LLVMBuildLoad2(ctx->builder,
                            actor_var->type, actor_var->alloca, llvm_tmp_name(ctx));
                        LLVMValueRef args[] = { actor_ptr };
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
            llvm_emit_intent_step_sync_effective_zone(ctx, step);
        else
            llvm_emit_intent_step_bind_bound_zone(ctx, node, step);
        if (rebound_aliases)
            llvm_emit_intent_step_restore_bound_zone_aliases(ctx, node, step, saved_actor_ptrs);

        if (completed_allocas != NULL) {
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0), completed_allocas[i]);
        }

        if (guard_expr != NULL) {
            char reason[256];
            LLVMBasicBlockRef next_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "intent.guard.ok");
            LLVMValueRef cond = llvm_emit_expression(guard_expr, ctx);
            snprintf(reason, sizeof(reason), "guard:%s",
                step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>");
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
                step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>");
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
                step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>");
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
                step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>");
            LLVMBuildStore(ctx->builder,
                LLVMBuildGlobalStringPtr(ctx->builder,
                    reason,
                    llvm_tmp_name(ctx)),
                fail_reason_alloca);
            LLVMBuildCondBr(ctx->builder, cond, next_bb, fail_bb);
            LLVMPositionBuilderAtEnd(ctx->builder, next_bb);
        }
        free(saved_actor_ptrs);
        saved_actor_ptrs = NULL;
        {
            LLVMValueRef handle = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
                handle_alloca, llvm_tmp_name(ctx));
            LLVMValueRef args[] = {
                handle,
                LLVMBuildGlobalStringPtr(ctx->builder,
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
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
            if (step == NULL || step->type != AST_INTENT_STEP
                || step->data.intent_step.compensate_expr_count == 0)
                continue;
            {
                LLVMBasicBlockRef do_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "intent.comp.do");
                LLVMBasicBlockRef next_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "intent.comp.next");
                LLVMValueRef done = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
                    completed_allocas[i], llvm_tmp_name(ctx));
                LLVMBuildCondBr(ctx->builder, done, do_bb, next_bb);
                LLVMPositionBuilderAtEnd(ctx->builder, do_bb);
                for (size_t j = step->data.intent_step.compensate_expr_count; j-- > 0;) {
                    if (step->data.intent_step.compensate_exprs[j] != NULL)
                        (void)llvm_emit_expression(step->data.intent_step.compensate_exprs[j], ctx);
                }
                llvm_emit_intent_step_bind_bound_zone(ctx, node, step);
                if (node->data.intent_decl.rollback_policy == INTENT_ROLLBACK_CURRENT)
                    LLVMBuildBr(ctx->builder, maybe_exit_bb);
                else
                    LLVMBuildBr(ctx->builder, next_bb);
                LLVMPositionBuilderAtEnd(ctx->builder, next_bb);
            }
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
    free(mir_steps);
    ctx->current_function = saved_fn;
    ctx->current_ret_type = saved_ret_type;

    if (saved_fn != NULL) {
        LLVMBasicBlockRef last = LLVMGetLastBasicBlock(saved_fn);
        if (last != NULL)
            LLVMPositionBuilderAtEnd(ctx->builder, last);
    }
}

#endif
