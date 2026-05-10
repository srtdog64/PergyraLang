#ifdef PGY_LLVM_ENABLED
#include "llvm_domain_zone_sync_internal.h"

#include <stdbool.h>
#include <stdio.h>

static bool
llvm_zone_frontier_prev_name(char *out,
                             size_t out_size,
                             const char *kind,
                             const char *name)
{
    int written;

    if (out == NULL || out_size == 0 || kind == NULL || name == NULL)
        return false;
    written = snprintf(out, out_size, "zone.prev_%s.%s", kind, name);
    return written >= 0 && (size_t)written < out_size;
}

static bool
llvm_zone_frontier_field_name(char *out,
                              size_t out_size,
                              const char *kind,
                              const char *name)
{
    int written;

    if (out == NULL || out_size == 0 || kind == NULL || name == NULL)
        return false;
    written = snprintf(out, out_size, "__%s_%s", kind, name);
    return written >= 0 && (size_t)written < out_size;
}

void
llvm_zone_sync_alloc_previous_state(ASTNode *stmt, LLVMGenCtx *ctx,
                                    LLVMValueRef **prev_state_addrs_out,
                                    LLVMValueRef **prev_layer_addrs_out)
{
    LLVMValueRef *prev_state_addrs = pgy_arena_calloc(&ctx->scratch,
        (stmt->data.zone_decl.state_count > 0 ? stmt->data.zone_decl.state_count : 1)
            * sizeof(LLVMValueRef));
    LLVMValueRef *prev_layer_addrs = pgy_arena_calloc(&ctx->scratch,
        (stmt->data.zone_decl.layer_slot_count > 0 ? stmt->data.zone_decl.layer_slot_count : 1)
            * sizeof(LLVMValueRef));

    for (size_t i = 0; i < stmt->data.zone_decl.state_count; i++) {
        ASTNode *state = stmt->data.zone_decl.states[i];
        char prev_name[256];
        if (state == NULL || state->type != AST_ZONE_STATE
            || state->data.zone_state.state_name == NULL)
            continue;
        if (!llvm_zone_frontier_prev_name(prev_name, sizeof(prev_name),
                "state", state->data.zone_state.state_name))
            continue;
        prev_state_addrs[i] = llvm_create_entry_alloca(ctx, ctx->type_i1, prev_name);
    }
    for (size_t i = 0; i < stmt->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = stmt->data.zone_decl.layer_slots[i];
        char prev_name[256];
        if (slot == NULL || slot->type != AST_ZONE_LAYER_SLOT
            || slot->data.zone_layer_slot.slot_name == NULL)
            continue;
        if (!llvm_zone_frontier_prev_name(prev_name, sizeof(prev_name),
                "layer", slot->data.zone_layer_slot.slot_name))
            continue;
        prev_layer_addrs[i] = llvm_create_entry_alloca(ctx, ctx->type_i1, prev_name);
    }
    if (prev_state_addrs_out != NULL)
        *prev_state_addrs_out = prev_state_addrs;
    if (prev_layer_addrs_out != NULL)
        *prev_layer_addrs_out = prev_layer_addrs;
}

void
llvm_zone_sync_snapshot_previous_state(ASTNode *stmt,
                                       LLVMClassTypeEntry *decl_cls,
                                       LLVMValueRef sync_fn,
                                       LLVMGenCtx *ctx,
                                       LLVMValueRef *prev_state_addrs,
                                       LLVMValueRef *prev_layer_addrs)
{
    for (size_t i = 0; i < stmt->data.zone_decl.state_count; i++) {
        ASTNode *state = stmt->data.zone_decl.states[i];
        const char *state_name;
        int field_idx;
        LLVMValueRef self_ptr;
        LLVMValueRef state_ptr;
        LLVMValueRef state_val;
        if (prev_state_addrs[i] == NULL || state == NULL || state->type != AST_ZONE_STATE
            || state->data.zone_state.state_name == NULL)
            continue;
        state_name = state->data.zone_state.state_name;
        {
            char field_name[256];
            if (!llvm_zone_frontier_field_name(field_name, sizeof(field_name),
                    "state", state_name))
                continue;
            field_idx = llvm_class_field_index(decl_cls, field_name);
        }
        if (field_idx < 0)
            continue;
        self_ptr = LLVMGetParam(sync_fn, 0);
        state_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
            self_ptr, (unsigned)field_idx, llvm_tmp_name(ctx));
        state_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
            state_ptr, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, state_val, prev_state_addrs[i]);
    }
    for (size_t i = 0; i < stmt->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = stmt->data.zone_decl.layer_slots[i];
        char field_name[256];
        int field_idx;
        LLVMValueRef self_ptr;
        LLVMValueRef layer_ptr;
        LLVMValueRef layer_val;
        if (prev_layer_addrs[i] == NULL || slot == NULL || slot->type != AST_ZONE_LAYER_SLOT
            || slot->data.zone_layer_slot.slot_name == NULL)
            continue;
        if (!llvm_zone_frontier_field_name(field_name, sizeof(field_name),
                "layer_active", slot->data.zone_layer_slot.slot_name))
            continue;
        field_idx = llvm_class_field_index(decl_cls, field_name);
        if (field_idx < 0)
            continue;
        self_ptr = LLVMGetParam(sync_fn, 0);
        layer_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
            self_ptr, (unsigned)field_idx, llvm_tmp_name(ctx));
        layer_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
            layer_ptr, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, layer_val, prev_layer_addrs[i]);
    }
}

void
llvm_zone_sync_reset_state_and_layers(ASTNode *stmt,
                                      LLVMClassTypeEntry *decl_cls,
                                      LLVMValueRef sync_fn,
                                      LLVMGenCtx *ctx)
{
    for (size_t i = 0; i < stmt->data.zone_decl.state_count; i++) {
        ASTNode *state = stmt->data.zone_decl.states[i];
        const char *state_name;
        int field_idx;
        LLVMValueRef self_ptr;
        LLVMValueRef state_ptr;
        if (state == NULL || state->type != AST_ZONE_STATE
            || state->data.zone_state.state_name == NULL)
            continue;
        state_name = state->data.zone_state.state_name;
        {
            char field_name[256];
            if (!llvm_zone_frontier_field_name(field_name, sizeof(field_name),
                    "state", state_name))
                continue;
            field_idx = llvm_class_field_index(decl_cls, field_name);
        }
        if (field_idx < 0)
            continue;
        self_ptr = LLVMGetParam(sync_fn, 0);
        state_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
            self_ptr, (unsigned)field_idx, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder,
            LLVMConstInt(ctx->type_i1, 0, 0), state_ptr);
    }
    for (size_t i = 0; i < stmt->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = stmt->data.zone_decl.layer_slots[i];
        int field_idx;
        LLVMValueRef self_ptr;
        LLVMValueRef pool_ptr;
        LLVMTypeRef pool_ty;
        LLVMValueRef items_ptr;
        LLVMValueRef active_ptr;
        LLVMValueRef count_ptr;
        LLVMValueRef cap_ptr;
        LLVMTypeRef i8_ty;

        if (slot == NULL || slot->type != AST_ZONE_LAYER_SLOT
            || !slot->data.zone_layer_slot.is_pool
            || slot->data.zone_layer_slot.slot_name == NULL)
            continue;

        field_idx = llvm_class_field_index(decl_cls,
            slot->data.zone_layer_slot.slot_name);
        if (field_idx < 0)
            continue;

        self_ptr = LLVMGetParam(sync_fn, 0);
        pool_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
            self_ptr, (unsigned)field_idx, llvm_tmp_name(ctx));
        pool_ty = decl_cls->fields[field_idx].field_type;
        i8_ty = LLVMInt8TypeInContext(ctx->context);

        items_ptr = LLVMBuildStructGEP2(ctx->builder, pool_ty, pool_ptr, 0, llvm_tmp_name(ctx));
        active_ptr = LLVMBuildStructGEP2(ctx->builder, pool_ty, pool_ptr, 1, llvm_tmp_name(ctx));
        count_ptr = LLVMBuildStructGEP2(ctx->builder, pool_ty, pool_ptr, 2, llvm_tmp_name(ctx));
        cap_ptr = LLVMBuildStructGEP2(ctx->builder, pool_ty, pool_ptr, 3, llvm_tmp_name(ctx));

        LLVMBuildStore(ctx->builder,
            LLVMConstNull(LLVMStructGetTypeAtIndex(pool_ty, 0)), items_ptr);
        LLVMBuildStore(ctx->builder,
            LLVMConstNull(LLVMStructGetTypeAtIndex(pool_ty, 1)), active_ptr);
        LLVMBuildStore(ctx->builder, LLVMConstInt(i8_ty, 0, 0), count_ptr);
        LLVMBuildStore(ctx->builder,
            LLVMConstInt(i8_ty,
                slot->data.zone_layer_slot.pool_capacity > 0
                    ? (unsigned)slot->data.zone_layer_slot.pool_capacity : 1,
                0),
            cap_ptr);
    }
    for (size_t i = 0; i < stmt->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = stmt->data.zone_decl.layer_slots[i];
        char field_name[256];
        int field_idx;
        LLVMValueRef self_ptr;
        LLVMValueRef layer_ptr;
        if (slot == NULL || slot->type != AST_ZONE_LAYER_SLOT
            || slot->data.zone_layer_slot.slot_name == NULL)
            continue;
        if (!llvm_zone_frontier_field_name(field_name, sizeof(field_name),
                "layer_active", slot->data.zone_layer_slot.slot_name))
            continue;
        field_idx = llvm_class_field_index(decl_cls, field_name);
        if (field_idx < 0)
            continue;
        self_ptr = LLVMGetParam(sync_fn, 0);
        layer_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
            self_ptr, (unsigned)field_idx, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder,
            LLVMConstInt(ctx->type_i1, 0, 0), layer_ptr);
    }
}

void
llvm_zone_sync_update_frontier_continue(ASTNode *stmt,
                                        LLVMClassTypeEntry *decl_cls,
                                        LLVMValueRef sync_fn,
                                        LLVMGenCtx *ctx,
                                        LLVMValueRef *prev_state_addrs,
                                        LLVMValueRef *prev_layer_addrs,
                                        LLVMValueRef frontier_continue_addr)
{
    for (size_t i = 0; i < stmt->data.zone_decl.state_count; i++) {
        ASTNode *state = stmt->data.zone_decl.states[i];
        const char *state_name;
        int field_idx;
        LLVMValueRef self_ptr;
        LLVMValueRef state_ptr;
        LLVMValueRef current_val;
        LLVMValueRef prev_val;
        LLVMValueRef changed_val;
        LLVMValueRef pending_val;
        if (prev_state_addrs[i] == NULL || state == NULL || state->type != AST_ZONE_STATE
            || state->data.zone_state.state_name == NULL)
            continue;
        state_name = state->data.zone_state.state_name;
        {
            char field_name[256];
            if (!llvm_zone_frontier_field_name(field_name, sizeof(field_name),
                    "state", state_name))
                continue;
            field_idx = llvm_class_field_index(decl_cls, field_name);
        }
        if (field_idx < 0)
            continue;
        self_ptr = LLVMGetParam(sync_fn, 0);
        state_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
            self_ptr, (unsigned)field_idx, llvm_tmp_name(ctx));
        current_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
            state_ptr, llvm_tmp_name(ctx));
        prev_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
            prev_state_addrs[i], llvm_tmp_name(ctx));
        changed_val = LLVMBuildICmp(ctx->builder, LLVMIntNE, current_val, prev_val,
            llvm_tmp_name(ctx));
        pending_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
            frontier_continue_addr, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder,
            LLVMBuildOr(ctx->builder, pending_val, changed_val, llvm_tmp_name(ctx)),
            frontier_continue_addr);
    }
    for (size_t i = 0; i < stmt->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = stmt->data.zone_decl.layer_slots[i];
        char field_name[256];
        int field_idx;
        LLVMValueRef self_ptr;
        LLVMValueRef layer_ptr;
        LLVMValueRef current_val;
        LLVMValueRef prev_val;
        LLVMValueRef changed_val;
        LLVMValueRef pending_val;
        if (prev_layer_addrs[i] == NULL || slot == NULL || slot->type != AST_ZONE_LAYER_SLOT
            || slot->data.zone_layer_slot.slot_name == NULL)
            continue;
        if (!llvm_zone_frontier_field_name(field_name, sizeof(field_name),
                "layer_active", slot->data.zone_layer_slot.slot_name))
            continue;
        field_idx = llvm_class_field_index(decl_cls, field_name);
        if (field_idx < 0)
            continue;
        self_ptr = LLVMGetParam(sync_fn, 0);
        layer_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
            self_ptr, (unsigned)field_idx, llvm_tmp_name(ctx));
        current_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
            layer_ptr, llvm_tmp_name(ctx));
        prev_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
            prev_layer_addrs[i], llvm_tmp_name(ctx));
        changed_val = LLVMBuildICmp(ctx->builder, LLVMIntNE, current_val, prev_val,
            llvm_tmp_name(ctx));
        pending_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
            frontier_continue_addr, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder,
            LLVMBuildOr(ctx->builder, pending_val, changed_val, llvm_tmp_name(ctx)),
            frontier_continue_addr);
    }
}

#endif /* PGY_LLVM_ENABLED */
