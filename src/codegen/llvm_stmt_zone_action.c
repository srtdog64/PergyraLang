#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"

static ASTNode *
llvm_stmt_find_effect_decl(LLVMGenCtx *ctx, const char *effect_name)
{
    if (ctx == NULL || effect_name == NULL)
        return NULL;
    return llvm_find_decl_in_active_inventory(ctx, AST_EFFECT_DECL, effect_name);
}

static ASTNode *
llvm_stmt_find_zone_domain_slot_decl(ASTNode *zone_decl, const char *slot_name)
{
    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL || slot_name == NULL)
        return NULL;

    for (size_t i = 0; i < zone_decl->data.zone_decl.slot_count; i++) {
        ASTNode *slot = zone_decl->data.zone_decl.slots[i];
        if (slot != NULL && slot->type == AST_DOMAIN_SLOT
            && slot->data.domain_slot.slot_name != NULL
            && strcmp(slot->data.domain_slot.slot_name, slot_name) == 0) {
            return slot;
        }
    }
    return NULL;
}

static ASTNode *
llvm_stmt_find_nth_subject_slot(ASTNode **slots, size_t slot_count, size_t nth)
{
    size_t seen = 0;

    if (slots == NULL)
        return NULL;

    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        if (slot == NULL || slot->type != AST_DOMAIN_SLOT
            || !slot->data.domain_slot.is_subject) {
            continue;
        }
        if (seen == nth)
            return slot;
        seen++;
    }

    return NULL;
}

static bool
llvm_stmt_resolve_zone_subject_receiver(LLVMGenCtx *ctx, ASTNode *receiver,
                                        const char **slot_name_out,
                                        const char **type_name_out)
{
    ASTNode *zone_decl;
    ASTNode *slot_decl = NULL;
    const char *slot_name = NULL;
    const char *type_name = NULL;

    if (slot_name_out != NULL)
        *slot_name_out = NULL;
    if (type_name_out != NULL)
        *type_name_out = NULL;

    if (ctx == NULL || receiver == NULL)
        return false;

    zone_decl = llvm_current_host_decl(ctx);
    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL)
        return false;

    if (receiver->type == AST_IDENTIFIER && receiver->data.identifier.name != NULL) {
        slot_name = receiver->data.identifier.name;
        slot_decl = llvm_stmt_find_zone_domain_slot_decl(zone_decl, slot_name);
    } else if (receiver->type == AST_MEMBER_ACCESS
               && receiver->data.member.object != NULL
               && receiver->data.member.object->type == AST_IDENTIFIER
               && receiver->data.member.object->data.identifier.name != NULL
               && strcmp(receiver->data.member.object->data.identifier.name, "self") == 0
               && receiver->data.member.name != NULL) {
        slot_name = receiver->data.member.name;
        slot_decl = llvm_stmt_find_zone_domain_slot_decl(zone_decl, slot_name);
    }

    if (slot_decl == NULL || !slot_decl->data.domain_slot.is_subject
        || slot_decl->data.domain_slot.type == NULL
        || slot_decl->data.domain_slot.type->type != AST_TYPE
        || slot_decl->data.domain_slot.type->data.type.name == NULL) {
        return false;
    }

    type_name = slot_decl->data.domain_slot.type->data.type.name;
    if (slot_name_out != NULL)
        *slot_name_out = slot_name;
    if (type_name_out != NULL)
        *type_name_out = type_name;
    return true;
}

void
llvm_stmt_emit_zone_action_effect_runtime(ASTNode *call, LLVMGenCtx *ctx)
{
    ASTNode *callee;
    ASTNode *receiver;
    ASTNode *zone_decl;
    ASTNode *method_decl;
    ASTNode *effect_decl;
    LLVMClassTypeEntry *zone_cls;
    LLVMClassTypeEntry *effect_cls;
    LLVMVarEntry *self_var;
    LLVMValueRef self_ptr;
    const char *method_name;
    const char *receiver_slot_name = NULL;
    const char *receiver_type_name = NULL;
    const char *effect_name;

    if (ctx == NULL || call == NULL
        || call->type != AST_CALL) {
        return;
    }

    zone_decl = llvm_current_host_decl(ctx);
    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL)
        return;

    callee = call->data.call.callee;
    if (callee == NULL || callee->type != AST_MEMBER_ACCESS)
        return;

    receiver = callee->data.member.object;
    method_name = callee->data.member.name;
    if (receiver == NULL || method_name == NULL)
        return;

    if (!llvm_stmt_resolve_zone_subject_receiver(ctx, receiver,
            &receiver_slot_name, &receiver_type_name)) {
        return;
    }

    method_decl = llvm_find_host_method_decl_in_context(ctx, receiver_type_name,
                                                        method_name);
    if (method_decl == NULL || method_decl->type != AST_FUNC_DECL
        || method_decl->is_async_decl
        || !method_decl->data.func_decl.is_action
        || method_decl->data.func_decl.within_zone == NULL
        || method_decl->data.func_decl.causes_effect == NULL
        || strcmp(method_decl->data.func_decl.within_zone,
                  zone_decl->data.zone_decl.name) != 0) {
        return;
    }

    effect_name = method_decl->data.func_decl.causes_effect;
    effect_decl = llvm_stmt_find_effect_decl(ctx, effect_name);
    zone_cls = llvm_lookup_class(ctx, zone_decl->data.zone_decl.name);
    effect_cls = llvm_lookup_class(ctx, effect_name);
    self_var = llvm_scope_lookup(ctx, "self");
    if (effect_decl == NULL || zone_cls == NULL || effect_cls == NULL || self_var == NULL)
        return;

    self_ptr = LLVMBuildLoad2(ctx->builder,
        LLVMPointerType(zone_cls->struct_type, 0),
        self_var->alloca, llvm_tmp_name(ctx));

    for (size_t i = 0; i < zone_decl->data.zone_decl.layer_slot_count; i++) {
        ASTNode *layer_slot = zone_decl->data.zone_decl.layer_slots[i];
        ASTNode *subject_slot;
        const char *layer_name;
        int active_idx;
        int layer_idx;
        int target_idx;
        int subject_idx;
        LLVMValueRef active_ptr;
        LLVMValueRef layer_ptr;
        LLVMValueRef target_ptr;
        LLVMValueRef target_value;
        LLVMFuncEntry *sync_entry;
        char active_field[256];
        char sync_name[256];

        if (layer_slot == NULL || layer_slot->type != AST_ZONE_LAYER_SLOT
            || layer_slot->data.zone_layer_slot.is_relation
            || layer_slot->data.zone_layer_slot.layer_type == NULL
            || strcmp(layer_slot->data.zone_layer_slot.layer_type, effect_name) != 0) {
            continue;
        }

        layer_name = layer_slot->data.zone_layer_slot.slot_name;
        if (layer_name == NULL)
            continue;

        snprintf(active_field, sizeof(active_field), "__layer_active_%s", layer_name);
        active_idx = llvm_class_field_index(zone_cls, active_field);
        if (active_idx >= 0) {
            active_ptr = LLVMBuildStructGEP2(ctx->builder, zone_cls->struct_type,
                self_ptr, (unsigned)active_idx, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0), active_ptr);
        }

        subject_slot = llvm_stmt_find_nth_subject_slot(effect_decl->data.effect_decl.slots,
            effect_decl->data.effect_decl.slot_count, 0);
        if (subject_slot == NULL || subject_slot->data.domain_slot.slot_name == NULL)
            continue;

        layer_idx = llvm_class_field_index(zone_cls, layer_name);
        target_idx = llvm_class_field_index(zone_cls, receiver_slot_name);
        subject_idx = llvm_class_field_index(effect_cls, subject_slot->data.domain_slot.slot_name);
        if (layer_idx < 0 || target_idx < 0 || subject_idx < 0)
            continue;

        layer_ptr = LLVMBuildStructGEP2(ctx->builder, zone_cls->struct_type,
            self_ptr, (unsigned)layer_idx, llvm_tmp_name(ctx));
        target_ptr = LLVMBuildStructGEP2(ctx->builder, zone_cls->struct_type,
            self_ptr, (unsigned)target_idx, llvm_tmp_name(ctx));
        target_value = LLVMBuildLoad2(ctx->builder,
            zone_cls->fields[target_idx].field_type, target_ptr, llvm_tmp_name(ctx));
        {
            LLVMValueRef subject_ptr = LLVMBuildStructGEP2(ctx->builder, effect_cls->struct_type,
                layer_ptr, (unsigned)subject_idx, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, target_value, subject_ptr);
        }
        for (size_t ri = 0; ri < effect_decl->data.effect_decl.refresh_count; ri++) {
            ASTNode *refresh = effect_decl->data.effect_decl.refreshes[ri];
            const char *projection_name;
            const char *source_name;
            char dirty_field[256];
            char ready_field[256];
            int dirty_idx;
            int ready_idx;

            if (refresh == NULL || refresh->type != AST_ZONE_REFRESH)
                continue;
            projection_name = refresh->data.zone_refresh.object_slot_name;
            source_name = refresh->data.zone_refresh.source_slot_name;
            if (projection_name == NULL || source_name == NULL
                || strcmp(source_name, subject_slot->data.domain_slot.slot_name) != 0) {
                continue;
            }

            snprintf(dirty_field, sizeof(dirty_field), "__projection_dirty_%s",
                     projection_name);
            dirty_idx = llvm_class_field_index(effect_cls, dirty_field);
            if (dirty_idx >= 0) {
                LLVMValueRef dirty_ptr = LLVMBuildStructGEP2(
                    ctx->builder, effect_cls->struct_type, layer_ptr,
                    (unsigned)dirty_idx, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder,
                               LLVMConstInt(ctx->type_i1, 1, 0),
                               dirty_ptr);
            }

            snprintf(ready_field, sizeof(ready_field), "__projection_ready_%s",
                     projection_name);
            ready_idx = llvm_class_field_index(effect_cls, ready_field);
            if (ready_idx >= 0) {
                LLVMValueRef ready_ptr = LLVMBuildStructGEP2(
                    ctx->builder, effect_cls->struct_type, layer_ptr,
                    (unsigned)ready_idx, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder,
                               LLVMConstInt(ctx->type_i1, 0, 0),
                               ready_ptr);
            }
        }

        snprintf(sync_name, sizeof(sync_name), "%s_sync", effect_name);
        sync_entry = llvm_lookup_function(ctx, sync_name);
        if (sync_entry != NULL) {
            LLVMValueRef sync_args[] = { layer_ptr };
            LLVMBuildCall2(ctx->builder, sync_entry->fn_type, sync_entry->fn,
                sync_args, 1, "");
        }
    }
}

#endif /* PGY_LLVM_ENABLED */
