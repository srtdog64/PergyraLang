#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"
#include "llvm_inventory_decl_lookup.h"
#include "parser/ast_api.h"

static bool
llvm_zone_action_field_name(LLVMGenCtx *ctx, char *out, size_t out_size,
                            const char *prefix, const char *name)
{
    int written;

    if (out == NULL || out_size == 0 || prefix == NULL || name == NULL)
        return false;
    written = snprintf(out, out_size, "%s%s", prefix, name);
    if (written >= 0 && (size_t)written < out_size)
        return true;
    llvm_set_error_with_hints(ctx,
        PGY_CODE_LLVM_SPEC_LIMIT,
        PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
        PGY_FIX_REFACTOR_OR_RAISE_LIMIT,
        "LLVM zone action generated field name is too long for '%s'",
        name);
    return false;
}

static bool
llvm_zone_action_sync_name(LLVMGenCtx *ctx, char *out, size_t out_size,
                           const char *effect_name)
{
    int written;

    if (out == NULL || out_size == 0 || effect_name == NULL)
        return false;
    written = snprintf(out, out_size, "%s_sync", effect_name);
    if (written >= 0 && (size_t)written < out_size)
        return true;
    llvm_set_error_with_hints(ctx,
        PGY_CODE_LLVM_SPEC_LIMIT,
        PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
        PGY_FIX_REFACTOR_OR_RAISE_LIMIT,
        "LLVM zone action sync function name is too long for effect '%s'",
        effect_name);
    return false;
}

static ASTNode *
llvm_stmt_find_zone_domain_slot_decl(ASTNode *zone_decl, const char *slot_name)
{
    size_t slot_count = 0;
    ASTNode **slots;

    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL || slot_name == NULL)
        return NULL;

    slots = ast_zone_slots(zone_decl, &slot_count);
    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        const char *candidate_name = ast_domain_slot_name(slot);
        if (slot != NULL && slot->type == AST_DOMAIN_SLOT
            && candidate_name != NULL
            && strcmp(candidate_name, slot_name) == 0) {
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
            || !ast_domain_slot_is_subject(slot)) {
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
    ASTNode *slot_type = NULL;

    if (slot_name_out != NULL)
        *slot_name_out = NULL;
    if (type_name_out != NULL)
        *type_name_out = NULL;

    if (ctx == NULL || receiver == NULL)
        return false;

    zone_decl = llvm_current_host_decl(ctx);
    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL)
        return false;

    if (receiver->type == AST_IDENTIFIER && ast_identifier_name(receiver) != NULL) {
        slot_name = ast_identifier_name(receiver);
        slot_decl = llvm_stmt_find_zone_domain_slot_decl(zone_decl, slot_name);
    } else if (receiver->type == AST_MEMBER_ACCESS
               && ast_member_object(receiver) != NULL
               && ast_member_object(receiver)->type == AST_IDENTIFIER
               && ast_identifier_name(ast_member_object(receiver)) != NULL
               && strcmp(ast_identifier_name(ast_member_object(receiver)), "self") == 0
               && ast_member_name(receiver) != NULL) {
        slot_name = ast_member_name(receiver);
        slot_decl = llvm_stmt_find_zone_domain_slot_decl(zone_decl, slot_name);
    }

    slot_type = ast_domain_slot_type(slot_decl);
    if (slot_decl == NULL || !ast_domain_slot_is_subject(slot_decl)
        || slot_type == NULL
        || slot_type->type != AST_TYPE
        || ast_type_name(slot_type) == NULL) {
        return false;
    }

    type_name = ast_type_name(slot_type);
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
    LLVMHostedZoneLayerSlotView layer_view;
    size_t effect_slot_count = 0;
    ASTNode **effect_slots = NULL;
    size_t refresh_count = 0;
    ASTNode **refreshes = NULL;
    const char *method_name;
    const char *receiver_slot_name = NULL;
    const char *receiver_type_name = NULL;
    const char *effect_name;
    const char *zone_name;

    if (ctx == NULL || call == NULL
        || call->type != AST_CALL) {
        return;
    }

    zone_decl = llvm_current_host_decl(ctx);
    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL)
        return;
    zone_name = llvm_decl_node_name(zone_decl);
    if (zone_name == NULL)
        return;

    callee = ast_call_callee(call);
    if (callee == NULL || callee->type != AST_MEMBER_ACCESS)
        return;

    receiver = ast_member_object(callee);
    method_name = ast_member_name(callee);
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
        || !ast_func_is_action(method_decl)
        || ast_func_within_zone(method_decl) == NULL
        || ast_func_causes_effect(method_decl) == NULL
        || strcmp(ast_func_within_zone(method_decl), zone_name) != 0) {
        return;
    }

    effect_name = ast_func_causes_effect(method_decl);
    effect_decl = llvm_find_named_domain_decl(ctx, AST_EFFECT_DECL, effect_name);
    zone_cls = llvm_lookup_class(ctx, zone_name);
    effect_cls = llvm_lookup_class(ctx, effect_name);
    self_var = llvm_scope_lookup(ctx, "self");
    if (effect_decl == NULL || zone_cls == NULL || effect_cls == NULL || self_var == NULL)
        return;

    self_ptr = LLVMBuildLoad2(ctx->builder,
        LLVMPointerType(zone_cls->struct_type, 0),
        self_var->alloca, llvm_tmp_name(ctx));

    layer_view = llvm_hosted_zone_layer_slot_view_from_decl(ctx, zone_name, zone_decl);
    if (llvm_hosted_zone_layer_slot_view_missing_mir_metadata(&layer_view)) {
        llvm_set_error(ctx,
            "MIR declaration inventory missing zone layer-slot metadata for zone action emission");
        return;
    }
    effect_slots = ast_effect_slots(effect_decl, &effect_slot_count);
    refreshes = ast_effect_refreshes(effect_decl, &refresh_count);

    for (size_t i = 0; i < layer_view.count; i++) {
        ASTNode *layer_slot =
            llvm_hosted_zone_layer_slot_view_source_ast(&layer_view, i);
        ASTNode *subject_slot;
        const char *layer_name;
        const char *layer_type;
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

        layer_name = llvm_hosted_zone_layer_slot_view_name(&layer_view, i);
        layer_type = llvm_hosted_zone_layer_slot_view_type_name(&layer_view, i);
        if (layer_slot == NULL || layer_slot->type != AST_ZONE_LAYER_SLOT
            || ast_zone_layer_slot_is_relation(layer_slot)
            || layer_type == NULL
            || strcmp(layer_type, effect_name) != 0) {
            continue;
        }

        if (layer_name == NULL)
            continue;

        if (!llvm_zone_action_field_name(ctx, active_field,
                sizeof(active_field), "__layer_active_", layer_name))
            return;
        active_idx = llvm_class_field_index(zone_cls, active_field);
        if (active_idx >= 0) {
            active_ptr = LLVMBuildStructGEP2(ctx->builder, zone_cls->struct_type,
                self_ptr, (unsigned)active_idx, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0), active_ptr);
        }

        subject_slot = llvm_stmt_find_nth_subject_slot(effect_slots,
            effect_slot_count, 0);
        const char *subject_slot_name = ast_domain_slot_name(subject_slot);
        if (subject_slot == NULL || subject_slot_name == NULL)
            continue;

        layer_idx = llvm_class_field_index(zone_cls, layer_name);
        target_idx = llvm_class_field_index(zone_cls, receiver_slot_name);
        subject_idx = llvm_class_field_index(effect_cls, subject_slot_name);
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
        for (size_t ri = 0; ri < refresh_count; ri++) {
            ASTNode *refresh = refreshes[ri];
            const char *projection_name;
            const char *source_name;
            char dirty_field[256];
            char ready_field[256];
            int dirty_idx;
            int ready_idx;

            if (refresh == NULL || refresh->type != AST_ZONE_REFRESH)
                continue;
            projection_name = ast_zone_refresh_object_slot_name(refresh);
            source_name = ast_zone_refresh_source_slot_name(refresh);
            if (projection_name == NULL || source_name == NULL
                || strcmp(source_name, subject_slot_name) != 0) {
                continue;
            }

            if (!llvm_zone_action_field_name(ctx, dirty_field,
                    sizeof(dirty_field), "__projection_dirty_",
                    projection_name))
                return;
            dirty_idx = llvm_class_field_index(effect_cls, dirty_field);
            if (dirty_idx >= 0) {
                LLVMValueRef dirty_ptr = LLVMBuildStructGEP2(
                    ctx->builder, effect_cls->struct_type, layer_ptr,
                    (unsigned)dirty_idx, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder,
                               LLVMConstInt(ctx->type_i1, 1, 0),
                               dirty_ptr);
            }

            if (!llvm_zone_action_field_name(ctx, ready_field,
                    sizeof(ready_field), "__projection_ready_",
                    projection_name))
                return;
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

        if (!llvm_zone_action_sync_name(ctx, sync_name, sizeof(sync_name),
                effect_name))
            return;
        sync_entry = llvm_lookup_function(ctx, sync_name);
        if (sync_entry != NULL) {
            LLVMValueRef sync_args[] = { layer_ptr };
            LLVMBuildCall2(ctx->builder, sync_entry->fn_type, sync_entry->fn,
                sync_args, 1, "");
        }
    }
}

#endif /* PGY_LLVM_ENABLED */
