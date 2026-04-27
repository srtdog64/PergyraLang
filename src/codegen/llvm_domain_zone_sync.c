#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"
#include "llvm_domain_decl_parts_helpers.h"
#include "llvm_domain_zone_bind_helpers.h"
#include "llvm_domain_projection_value_helpers.h"
#include "llvm_domain_projection_sync_body_helpers.h"
void
llvm_emit_zone_sync(ASTNode *stmt, const char *decl_name,
                    LLVMClassTypeEntry *decl_cls, LLVMValueRef sync_fn,
                    LLVMGenCtx *ctx)
{
    LLVMValueRef saved_fn;
    LLVMTypeRef saved_ret;
    ASTNode *saved_host_decl;
    LLVMBasicBlockRef bb;

    if (stmt == NULL || stmt->type != AST_ZONE_DECL || decl_name == NULL
        || decl_cls == NULL || sync_fn == NULL || ctx == NULL)
        return;

    saved_fn = ctx->current_function;
    saved_ret = ctx->current_ret_type;
    saved_host_decl = llvm_bind_current_host_decl(ctx, stmt);
    bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn, "entry");
    LLVMPositionBuilderAtEnd(ctx->builder, bb);
    ctx->current_function = sync_fn;
    ctx->current_ret_type = ctx->type_void;

    llvm_scope_push(ctx);
    {
        LLVMTypeRef self_ptr_t = LLVMPointerType(decl_cls->struct_type, 0);
        LLVMValueRef sa = llvm_create_entry_alloca(ctx, self_ptr_t, "self.addr");
        LLVMBuildStore(ctx->builder, LLVMGetParam(sync_fn, 0), sa);
        llvm_scope_declare(ctx, "self", sa, self_ptr_t);
        llvm_register_var_class(ctx, "self", decl_name);
    }
    {
        int generation_idx = llvm_class_field_index(decl_cls, "__sync_generation");
        if (generation_idx >= 0) {
            LLVMValueRef self_ptr = LLVMGetParam(sync_fn, 0);
            LLVMValueRef generation_ptr = LLVMBuildStructGEP2(ctx->builder,
                decl_cls->struct_type, self_ptr, (unsigned)generation_idx,
                llvm_tmp_name(ctx));
            LLVMValueRef generation_val = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
                generation_ptr, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder,
                LLVMBuildAdd(ctx->builder, generation_val,
                    LLVMConstInt(ctx->type_i32, 1, 0), llvm_tmp_name(ctx)),
                generation_ptr);
        }
    }
    LLVMValueRef frontier_pass_addr = llvm_create_entry_alloca(ctx, ctx->type_i32,
        "zone.frontier.pass.addr");
    LLVMValueRef frontier_continue_addr = llvm_create_entry_alloca(ctx, ctx->type_i1,
        "zone.frontier.continue.addr");
    LLVMValueRef frontier_limit_val = LLVMConstInt(ctx->type_i32,
        (unsigned long long)(stmt->data.zone_decl.state_count
            + stmt->data.zone_decl.layer_slot_count + 1), 0);
    LLVMBasicBlockRef frontier_check_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn,
        "zone.frontier.check");
    LLVMBasicBlockRef frontier_body_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn,
        "zone.frontier.body");
    LLVMBasicBlockRef frontier_done_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn,
        "zone.frontier.done");
    LLVMBasicBlockRef frontier_overflow_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn,
        "zone.frontier.overflow");
    LLVMBasicBlockRef frontier_exit_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn,
        "zone.frontier.exit");
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
        snprintf(prev_name, sizeof(prev_name), "zone.prev_state.%s",
            state->data.zone_state.state_name);
        prev_state_addrs[i] = llvm_create_entry_alloca(ctx, ctx->type_i1, prev_name);
    }
    for (size_t i = 0; i < stmt->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = stmt->data.zone_decl.layer_slots[i];
        char prev_name[256];
        if (slot == NULL || slot->type != AST_ZONE_LAYER_SLOT
            || slot->data.zone_layer_slot.slot_name == NULL)
            continue;
        snprintf(prev_name, sizeof(prev_name), "zone.prev_layer.%s",
            slot->data.zone_layer_slot.slot_name);
        prev_layer_addrs[i] = llvm_create_entry_alloca(ctx, ctx->type_i1, prev_name);
    }

    LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i32, 0, 0), frontier_pass_addr);
    LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0), frontier_continue_addr);
    LLVMBuildBr(ctx->builder, frontier_check_bb);

    LLVMPositionBuilderAtEnd(ctx->builder, frontier_check_bb);
    {
        LLVMValueRef continue_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
            frontier_continue_addr, llvm_tmp_name(ctx));
        LLVMValueRef pass_val = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
            frontier_pass_addr, llvm_tmp_name(ctx));
        LLVMValueRef under_limit = LLVMBuildICmp(ctx->builder, LLVMIntULT,
            pass_val, frontier_limit_val, llvm_tmp_name(ctx));
        LLVMValueRef loop_cond = LLVMBuildAnd(ctx->builder, continue_val,
            under_limit, llvm_tmp_name(ctx));
        LLVMBuildCondBr(ctx->builder, loop_cond, frontier_body_bb, frontier_done_bb);
    }

    LLVMPositionBuilderAtEnd(ctx->builder, frontier_body_bb);
    LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0), frontier_continue_addr);
    LLVMBuildStore(ctx->builder,
        LLVMBuildAdd(ctx->builder,
            LLVMBuildLoad2(ctx->builder, ctx->type_i32, frontier_pass_addr, llvm_tmp_name(ctx)),
            LLVMConstInt(ctx->type_i32, 1, 0),
            llvm_tmp_name(ctx)),
        frontier_pass_addr);

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
            snprintf(field_name, sizeof(field_name), "__state_%s", state_name);
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
        snprintf(field_name, sizeof(field_name), "__layer_active_%s",
            slot->data.zone_layer_slot.slot_name);
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
            snprintf(field_name, sizeof(field_name), "__state_%s", state_name);
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
        snprintf(field_name, sizeof(field_name), "__layer_active_%s",
            slot->data.zone_layer_slot.slot_name);
        field_idx = llvm_class_field_index(decl_cls, field_name);
        if (field_idx < 0)
            continue;
        self_ptr = LLVMGetParam(sync_fn, 0);
        layer_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
            self_ptr, (unsigned)field_idx, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder,
            LLVMConstInt(ctx->type_i1, 0, 0), layer_ptr);
    }

    llvm_emit_domain_projection_sync_body(stmt, decl_cls, sync_fn, ctx);

    for (size_t i = 0; i < stmt->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = stmt->data.zone_decl.layer_slots[i];
        const char *layer_name;
        char cause_field[256];
        char active_field[256];
        int cause_idx;
        int active_idx;
        LLVMValueRef self_ptr;
        LLVMValueRef cause_ptr;
        LLVMValueRef cause_val;
        LLVMValueRef is_action;
        LLVMBasicBlockRef action_bb;
        LLVMBasicBlockRef next_bb;

        if (slot == NULL || slot->type != AST_ZONE_LAYER_SLOT
            || slot->data.zone_layer_slot.is_relation
            || slot->data.zone_layer_slot.slot_name == NULL) {
            continue;
        }

        layer_name = slot->data.zone_layer_slot.slot_name;
        snprintf(cause_field, sizeof(cause_field), "__layer_cause_%s", layer_name);
        snprintf(active_field, sizeof(active_field), "__layer_active_%s", layer_name);
        cause_idx = llvm_class_field_index(decl_cls, cause_field);
        active_idx = llvm_class_field_index(decl_cls, active_field);
        if (cause_idx < 0 || active_idx < 0)
            continue;

        self_ptr = LLVMGetParam(sync_fn, 0);
        cause_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
            self_ptr, (unsigned)cause_idx, llvm_tmp_name(ctx));
        cause_val = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
            cause_ptr, llvm_tmp_name(ctx));
        is_action = LLVMBuildICmp(ctx->builder, LLVMIntEQ, cause_val,
            LLVMConstInt(ctx->type_i32, PGY_PROP_CAUSE_ACTION, 0),
            llvm_tmp_name(ctx));
        action_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn, "zone.action.cause");
        next_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn, "zone.action.next");
        LLVMBuildCondBr(ctx->builder, is_action, action_bb, next_bb);
        LLVMPositionBuilderAtEnd(ctx->builder, action_bb);
        {
            LLVMValueRef active_ptr = LLVMBuildStructGEP2(ctx->builder,
                decl_cls->struct_type, self_ptr, (unsigned)active_idx,
                llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0), active_ptr);
        }
        for (size_t j = 0; j < stmt->data.zone_decl.state_count; j++) {
            ASTNode *state = stmt->data.zone_decl.states[j];
            char state_field[256];
            int state_idx;
            LLVMValueRef state_ptr;
            if (state == NULL || state->type != AST_ZONE_STATE
                || state->data.zone_state.is_relation
                || state->data.zone_state.state_name == NULL
                || state->data.zone_state.layer_slot_name == NULL
                || strcmp(state->data.zone_state.layer_slot_name, layer_name) != 0) {
                continue;
            }
            snprintf(state_field, sizeof(state_field), "__state_%s",
                state->data.zone_state.state_name);
            state_idx = llvm_class_field_index(decl_cls, state_field);
            if (state_idx < 0)
                continue;
            state_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                self_ptr, (unsigned)state_idx, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0), state_ptr);
        }
        LLVMBuildBr(ctx->builder, next_bb);
        LLVMPositionBuilderAtEnd(ctx->builder, next_bb);
    }

    for (size_t i = 0; i < stmt->data.zone_decl.apply_count; i++) {
        ASTNode *apply = stmt->data.zone_decl.applies[i];
        const char *state_name = apply != NULL ? apply->data.zone_apply.state_name : NULL;
        if (state_name == NULL && apply != NULL) {
            for (size_t j = 0; j < stmt->data.zone_decl.state_count; j++) {
                ASTNode *state = stmt->data.zone_decl.states[j];
                if (state != NULL && state->type == AST_ZONE_STATE
                    && !state->data.zone_state.is_relation
                    && state->data.zone_state.layer_slot_name != NULL
                    && state->data.zone_state.left_or_target_slot_name != NULL
                    && apply->data.zone_apply.effect_slot_name != NULL
                    && apply->data.zone_apply.target_slot_name != NULL
                    && strcmp(state->data.zone_state.layer_slot_name,
                              apply->data.zone_apply.effect_slot_name) == 0
                    && strcmp(state->data.zone_state.left_or_target_slot_name,
                              apply->data.zone_apply.target_slot_name) == 0) {
                    state_name = state->data.zone_state.state_name;
                    break;
                }
            }
        }
        if (state_name != NULL) {
            char field_name[256];
            int field_idx;
            LLVMValueRef self_ptr;
            LLVMValueRef state_ptr;
            snprintf(field_name, sizeof(field_name), "__state_%s", state_name);
            field_idx = llvm_class_field_index(decl_cls, field_name);
            if (field_idx < 0)
                continue;
            self_ptr = LLVMGetParam(sync_fn, 0);
            state_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                self_ptr, (unsigned)field_idx, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder,
                LLVMConstInt(ctx->type_i1, 1, 0), state_ptr);
            llvm_stamp_domain_provenance(ctx, decl_cls, self_ptr, "state",
                state_name, PGY_PROP_CAUSE_APPLY);
            if (apply != NULL) {
                const char *layer_name = apply->data.zone_apply.effect_slot_name;
                if (layer_name == NULL) {
                    for (size_t j = 0; j < stmt->data.zone_decl.state_count; j++) {
                        ASTNode *state = stmt->data.zone_decl.states[j];
                        if (state != NULL && state->type == AST_ZONE_STATE
                            && !state->data.zone_state.is_relation
                            && state->data.zone_state.state_name != NULL
                            && strcmp(state->data.zone_state.state_name, state_name) == 0) {
                            layer_name = state->data.zone_state.layer_slot_name;
                            break;
                        }
                    }
                }
                if (layer_name != NULL) {
                    char layer_field[256];
                    int layer_idx;
                    LLVMValueRef layer_ptr;
                    snprintf(layer_field, sizeof(layer_field), "__layer_active_%s", layer_name);
                    layer_idx = llvm_class_field_index(decl_cls, layer_field);
                    if (layer_idx >= 0) {
                        layer_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                            self_ptr, (unsigned)layer_idx, llvm_tmp_name(ctx));
                        LLVMBuildStore(ctx->builder,
                            LLVMConstInt(ctx->type_i1, 1, 0), layer_ptr);
                        llvm_stamp_domain_provenance(ctx, decl_cls, self_ptr,
                            "layer", layer_name, PGY_PROP_CAUSE_APPLY);
                    }
                    if (apply->data.zone_apply.target_slot_name != NULL) {
                        llvm_zone_bind_effect_layer(stmt, decl_cls, sync_fn, ctx,
                            layer_name, apply->data.zone_apply.target_slot_name);
                    } else {
                        for (size_t j = 0; j < stmt->data.zone_decl.state_count; j++) {
                            ASTNode *state = stmt->data.zone_decl.states[j];
                            if (state != NULL && state->type == AST_ZONE_STATE
                                && !state->data.zone_state.is_relation
                                && state->data.zone_state.state_name != NULL
                                && strcmp(state->data.zone_state.state_name, state_name) == 0
                                && state->data.zone_state.left_or_target_slot_name != NULL) {
                                llvm_zone_bind_effect_layer(stmt, decl_cls, sync_fn, ctx,
                                    layer_name,
                                    state->data.zone_state.left_or_target_slot_name);
                                break;
                            }
                        }
                    }
                }
            }
        } else if (apply != NULL
                   && apply->data.zone_apply.effect_slot_name != NULL
                   && apply->data.zone_apply.target_slot_name != NULL) {
            char layer_field[256];
            int layer_idx;
            LLVMValueRef self_ptr = LLVMGetParam(sync_fn, 0);
            LLVMValueRef layer_ptr;
            snprintf(layer_field, sizeof(layer_field), "__layer_active_%s",
                apply->data.zone_apply.effect_slot_name);
            layer_idx = llvm_class_field_index(decl_cls, layer_field);
            if (layer_idx >= 0) {
                layer_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                    self_ptr, (unsigned)layer_idx, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder,
                    LLVMConstInt(ctx->type_i1, 1, 0), layer_ptr);
                llvm_stamp_domain_provenance(ctx, decl_cls, self_ptr, "layer",
                    apply->data.zone_apply.effect_slot_name,
                    PGY_PROP_CAUSE_APPLY);
            }
            llvm_zone_bind_effect_layer(stmt, decl_cls, sync_fn, ctx,
                apply->data.zone_apply.effect_slot_name,
                apply->data.zone_apply.target_slot_name);
        }
    }

    for (size_t i = 0; i < stmt->data.zone_decl.maintained_effect_count; i++) {
        ASTNode *maintain = stmt->data.zone_decl.maintained_effects[i];
        if (maintain == NULL
            || maintain->data.zone_maintain_effect.effect_slot_name == NULL
            || maintain->data.zone_maintain_effect.target_slot_name == NULL) {
            continue;
        }
        {
            char layer_field[256];
            int layer_idx;
            LLVMValueRef self_ptr = LLVMGetParam(sync_fn, 0);
            LLVMValueRef layer_ptr;
            snprintf(layer_field, sizeof(layer_field), "__layer_active_%s",
                maintain->data.zone_maintain_effect.effect_slot_name);
            layer_idx = llvm_class_field_index(decl_cls, layer_field);
            if (layer_idx >= 0) {
                layer_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                    self_ptr, (unsigned)layer_idx, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder,
                    LLVMConstInt(ctx->type_i1, 1, 0), layer_ptr);
                llvm_stamp_domain_provenance(ctx, decl_cls, self_ptr, "layer",
                    maintain->data.zone_maintain_effect.effect_slot_name,
                    PGY_PROP_CAUSE_MAINTAIN);
            }
            if (maintain->data.zone_maintain_relation.left_slot_name != NULL
                && maintain->data.zone_maintain_relation.right_slot_name != NULL) {
                llvm_zone_bind_relation_layer(stmt, decl_cls, sync_fn, ctx,
                    maintain->data.zone_maintain_relation.relation_slot_name,
                    maintain->data.zone_maintain_relation.left_slot_name,
                    maintain->data.zone_maintain_relation.right_slot_name);
            }
        }
        llvm_zone_bind_effect_layer(stmt, decl_cls, sync_fn, ctx,
            maintain->data.zone_maintain_effect.effect_slot_name,
            maintain->data.zone_maintain_effect.target_slot_name);
        for (size_t j = 0; j < stmt->data.zone_decl.state_count; j++) {
            ASTNode *state = stmt->data.zone_decl.states[j];
            const char *state_name;
            char field_name[256];
            int field_idx;
            LLVMValueRef self_ptr;
            LLVMValueRef state_ptr;
            if (state == NULL || state->type != AST_ZONE_STATE
                || state->data.zone_state.is_relation
                || state->data.zone_state.layer_slot_name == NULL
                || state->data.zone_state.left_or_target_slot_name == NULL
                || strcmp(state->data.zone_state.layer_slot_name,
                          maintain->data.zone_maintain_effect.effect_slot_name) != 0
                || strcmp(state->data.zone_state.left_or_target_slot_name,
                          maintain->data.zone_maintain_effect.target_slot_name) != 0) {
                continue;
            }
            state_name = state->data.zone_state.state_name;
            snprintf(field_name, sizeof(field_name), "__state_%s", state_name);
            field_idx = llvm_class_field_index(decl_cls, field_name);
            if (field_idx < 0)
                continue;
            self_ptr = LLVMGetParam(sync_fn, 0);
            state_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                self_ptr, (unsigned)field_idx, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder,
                LLVMConstInt(ctx->type_i1, 1, 0), state_ptr);
            llvm_stamp_domain_provenance(ctx, decl_cls, self_ptr, "state",
                state_name, PGY_PROP_CAUSE_MAINTAIN);
        }
    }

    for (size_t i = 0; i < stmt->data.zone_decl.maintained_state_count; i++) {
        ASTNode *maintain = stmt->data.zone_decl.maintained_states[i];
        if (maintain != NULL && maintain->data.zone_maintain_state.state_name != NULL) {
            char field_name[256];
            int field_idx;
            LLVMValueRef self_ptr;
            LLVMValueRef state_ptr;
            snprintf(field_name, sizeof(field_name), "__state_%s",
                maintain->data.zone_maintain_state.state_name);
            field_idx = llvm_class_field_index(decl_cls, field_name);
            if (field_idx < 0)
                continue;
            self_ptr = LLVMGetParam(sync_fn, 0);
            state_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                self_ptr, (unsigned)field_idx, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder,
                LLVMConstInt(ctx->type_i1, 1, 0), state_ptr);
            llvm_stamp_domain_provenance(ctx, decl_cls, self_ptr, "state",
                maintain->data.zone_maintain_state.state_name,
                PGY_PROP_CAUSE_MAINTAIN);
            for (size_t j = 0; j < stmt->data.zone_decl.state_count; j++) {
                ASTNode *state = stmt->data.zone_decl.states[j];
                if (state != NULL && state->type == AST_ZONE_STATE
                    && state->data.zone_state.state_name != NULL
                    && strcmp(state->data.zone_state.state_name,
                              maintain->data.zone_maintain_state.state_name) == 0) {
                    char layer_field[256];
                    int layer_idx;
                    LLVMValueRef layer_ptr;
                    snprintf(layer_field, sizeof(layer_field), "__layer_active_%s",
                        state->data.zone_state.layer_slot_name);
                    layer_idx = llvm_class_field_index(decl_cls, layer_field);
                    if (layer_idx >= 0) {
                        layer_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                            self_ptr, (unsigned)layer_idx, llvm_tmp_name(ctx));
                        LLVMBuildStore(ctx->builder,
                            LLVMConstInt(ctx->type_i1, 1, 0), layer_ptr);
                        llvm_stamp_domain_provenance(ctx, decl_cls, self_ptr,
                            "layer", state->data.zone_state.layer_slot_name,
                            PGY_PROP_CAUSE_MAINTAIN);
                    }
                    if (!state->data.zone_state.is_relation) {
                        llvm_zone_bind_effect_layer(stmt, decl_cls, sync_fn, ctx,
                            state->data.zone_state.layer_slot_name,
                            state->data.zone_state.left_or_target_slot_name);
                    } else if (state->data.zone_state.right_slot_name != NULL) {
                        llvm_zone_bind_relation_layer(stmt, decl_cls, sync_fn, ctx,
                            state->data.zone_state.layer_slot_name,
                            state->data.zone_state.left_or_target_slot_name,
                            state->data.zone_state.right_slot_name);
                    }
                    break;
                }
            }
        }
    }

    for (size_t i = 0; i < stmt->data.zone_decl.detach_count; i++) {
        ASTNode *detach = stmt->data.zone_decl.detaches[i];
        const char *state_name = detach != NULL ? detach->data.zone_detach.state_name : NULL;
        if (state_name == NULL && detach != NULL) {
            for (size_t j = 0; j < stmt->data.zone_decl.state_count; j++) {
                ASTNode *state = stmt->data.zone_decl.states[j];
                if (state != NULL && state->type == AST_ZONE_STATE
                    && !state->data.zone_state.is_relation
                    && state->data.zone_state.layer_slot_name != NULL
                    && state->data.zone_state.left_or_target_slot_name != NULL
                    && detach->data.zone_detach.effect_slot_name != NULL
                    && detach->data.zone_detach.target_slot_name != NULL
                    && strcmp(state->data.zone_state.layer_slot_name,
                              detach->data.zone_detach.effect_slot_name) == 0
                    && strcmp(state->data.zone_state.left_or_target_slot_name,
                              detach->data.zone_detach.target_slot_name) == 0) {
                    state_name = state->data.zone_state.state_name;
                    break;
                }
            }
        }
        if (state_name != NULL) {
            char field_name[256];
            int field_idx;
            LLVMValueRef self_ptr;
            LLVMValueRef state_ptr;
            snprintf(field_name, sizeof(field_name), "__state_%s", state_name);
            field_idx = llvm_class_field_index(decl_cls, field_name);
            if (field_idx < 0)
                continue;
            self_ptr = LLVMGetParam(sync_fn, 0);
            state_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                self_ptr, (unsigned)field_idx, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder,
                LLVMConstInt(ctx->type_i1, 0, 0), state_ptr);
            llvm_stamp_domain_provenance(ctx, decl_cls, self_ptr, "state",
                state_name, PGY_PROP_CAUSE_DETACH);
            if (detach != NULL) {
                const char *layer_name = detach->data.zone_detach.effect_slot_name;
                if (layer_name == NULL) {
                    for (size_t j = 0; j < stmt->data.zone_decl.state_count; j++) {
                        ASTNode *state = stmt->data.zone_decl.states[j];
                        if (state != NULL && state->type == AST_ZONE_STATE
                            && !state->data.zone_state.is_relation
                            && state->data.zone_state.state_name != NULL
                            && strcmp(state->data.zone_state.state_name, state_name) == 0) {
                            layer_name = state->data.zone_state.layer_slot_name;
                            break;
                        }
                    }
                }
                if (layer_name != NULL) {
                    char layer_field[256];
                    int layer_idx;
                    LLVMValueRef layer_ptr;
                    snprintf(layer_field, sizeof(layer_field), "__layer_active_%s", layer_name);
                    layer_idx = llvm_class_field_index(decl_cls, layer_field);
                    if (layer_idx >= 0) {
                        layer_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                            self_ptr, (unsigned)layer_idx, llvm_tmp_name(ctx));
                        LLVMBuildStore(ctx->builder,
                            LLVMConstInt(ctx->type_i1, 0, 0), layer_ptr);
                        llvm_stamp_domain_provenance(ctx, decl_cls, self_ptr,
                            "layer", layer_name, PGY_PROP_CAUSE_DETACH);
                    }
                }
            }
        } else if (detach != NULL && detach->data.zone_detach.effect_slot_name != NULL) {
            char layer_field[256];
            int layer_idx;
            LLVMValueRef self_ptr = LLVMGetParam(sync_fn, 0);
            LLVMValueRef layer_ptr;
            snprintf(layer_field, sizeof(layer_field), "__layer_active_%s",
                detach->data.zone_detach.effect_slot_name);
            layer_idx = llvm_class_field_index(decl_cls, layer_field);
            if (layer_idx >= 0) {
                layer_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                    self_ptr, (unsigned)layer_idx, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder,
                    LLVMConstInt(ctx->type_i1, 0, 0), layer_ptr);
                llvm_stamp_domain_provenance(ctx, decl_cls, self_ptr, "layer",
                    detach->data.zone_detach.effect_slot_name,
                    PGY_PROP_CAUSE_DETACH);
            }
        }
    }

    for (size_t i = 0; i < stmt->data.zone_decl.link_count; i++) {
        ASTNode *link = stmt->data.zone_decl.links[i];
        const char *state_name = link != NULL ? link->data.zone_link.state_name : NULL;
        if (state_name == NULL && link != NULL) {
            for (size_t j = 0; j < stmt->data.zone_decl.state_count; j++) {
                ASTNode *state = stmt->data.zone_decl.states[j];
                if (state != NULL && state->type == AST_ZONE_STATE
                    && state->data.zone_state.is_relation
                    && state->data.zone_state.layer_slot_name != NULL
                    && state->data.zone_state.left_or_target_slot_name != NULL
                    && state->data.zone_state.right_slot_name != NULL
                    && link->data.zone_link.relation_slot_name != NULL
                    && link->data.zone_link.left_slot_name != NULL
                    && link->data.zone_link.right_slot_name != NULL
                    && strcmp(state->data.zone_state.layer_slot_name,
                              link->data.zone_link.relation_slot_name) == 0
                    && strcmp(state->data.zone_state.left_or_target_slot_name,
                              link->data.zone_link.left_slot_name) == 0
                    && strcmp(state->data.zone_state.right_slot_name,
                              link->data.zone_link.right_slot_name) == 0) {
                    state_name = state->data.zone_state.state_name;
                    break;
                }
            }
        }
        if (state_name != NULL) {
            char field_name[256];
            int field_idx;
            LLVMValueRef self_ptr;
            LLVMValueRef state_ptr;
            snprintf(field_name, sizeof(field_name), "__state_%s", state_name);
            field_idx = llvm_class_field_index(decl_cls, field_name);
            if (field_idx < 0)
                continue;
            self_ptr = LLVMGetParam(sync_fn, 0);
            state_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                self_ptr, (unsigned)field_idx, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder,
                LLVMConstInt(ctx->type_i1, 1, 0), state_ptr);
            llvm_stamp_domain_provenance(ctx, decl_cls, self_ptr, "state",
                state_name, PGY_PROP_CAUSE_LINK);
            if (link != NULL) {
                const char *layer_name = link->data.zone_link.relation_slot_name;
                if (layer_name == NULL) {
                    for (size_t j = 0; j < stmt->data.zone_decl.state_count; j++) {
                        ASTNode *state = stmt->data.zone_decl.states[j];
                        if (state != NULL && state->type == AST_ZONE_STATE
                            && state->data.zone_state.is_relation
                            && state->data.zone_state.state_name != NULL
                            && strcmp(state->data.zone_state.state_name, state_name) == 0) {
                            layer_name = state->data.zone_state.layer_slot_name;
                            break;
                        }
                    }
                }
                if (layer_name != NULL) {
                    char layer_field[256];
                    int layer_idx;
                    LLVMValueRef layer_ptr;
                    snprintf(layer_field, sizeof(layer_field), "__layer_active_%s", layer_name);
                    layer_idx = llvm_class_field_index(decl_cls, layer_field);
                    if (layer_idx >= 0) {
                        layer_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                            self_ptr, (unsigned)layer_idx, llvm_tmp_name(ctx));
                        LLVMBuildStore(ctx->builder,
                            LLVMConstInt(ctx->type_i1, 1, 0), layer_ptr);
                        llvm_stamp_domain_provenance(ctx, decl_cls, self_ptr,
                            "layer", layer_name, PGY_PROP_CAUSE_LINK);
                    }
                    if (link->data.zone_link.left_slot_name != NULL
                        && link->data.zone_link.right_slot_name != NULL) {
                        llvm_zone_bind_relation_layer(stmt, decl_cls, sync_fn, ctx,
                            layer_name,
                            link->data.zone_link.left_slot_name,
                            link->data.zone_link.right_slot_name);
                    } else {
                        for (size_t j = 0; j < stmt->data.zone_decl.state_count; j++) {
                            ASTNode *state = stmt->data.zone_decl.states[j];
                            if (state != NULL && state->type == AST_ZONE_STATE
                                && state->data.zone_state.is_relation
                                && state->data.zone_state.state_name != NULL
                                && strcmp(state->data.zone_state.state_name, state_name) == 0
                                && state->data.zone_state.left_or_target_slot_name != NULL
                                && state->data.zone_state.right_slot_name != NULL) {
                                llvm_zone_bind_relation_layer(stmt, decl_cls, sync_fn, ctx,
                                    layer_name,
                                    state->data.zone_state.left_or_target_slot_name,
                                    state->data.zone_state.right_slot_name);
                                break;
                            }
                        }
                    }
                }
            }
        } else if (link != NULL
                   && link->data.zone_link.relation_slot_name != NULL
                   && link->data.zone_link.left_slot_name != NULL
                   && link->data.zone_link.right_slot_name != NULL) {
            char layer_field[256];
            int layer_idx;
            LLVMValueRef self_ptr = LLVMGetParam(sync_fn, 0);
            LLVMValueRef layer_ptr;
            snprintf(layer_field, sizeof(layer_field), "__layer_active_%s",
                link->data.zone_link.relation_slot_name);
            layer_idx = llvm_class_field_index(decl_cls, layer_field);
            if (layer_idx >= 0) {
                layer_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                    self_ptr, (unsigned)layer_idx, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder,
                    LLVMConstInt(ctx->type_i1, 1, 0), layer_ptr);
                llvm_stamp_domain_provenance(ctx, decl_cls, self_ptr, "layer",
                    link->data.zone_link.relation_slot_name,
                    PGY_PROP_CAUSE_LINK);
            }
            llvm_zone_bind_relation_layer(stmt, decl_cls, sync_fn, ctx,
                link->data.zone_link.relation_slot_name,
                link->data.zone_link.left_slot_name,
                link->data.zone_link.right_slot_name);
        }
    }

    for (size_t i = 0; i < stmt->data.zone_decl.maintained_relation_count; i++) {
        ASTNode *maintain = stmt->data.zone_decl.maintained_relations[i];
        if (maintain != NULL && maintain->data.zone_maintain_relation.relation_slot_name != NULL) {
            char layer_field[256];
            int layer_idx;
            LLVMValueRef self_ptr = LLVMGetParam(sync_fn, 0);
            LLVMValueRef layer_ptr;
            snprintf(layer_field, sizeof(layer_field), "__layer_active_%s",
                maintain->data.zone_maintain_relation.relation_slot_name);
            layer_idx = llvm_class_field_index(decl_cls, layer_field);
            if (layer_idx >= 0) {
                layer_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                    self_ptr, (unsigned)layer_idx, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder,
                    LLVMConstInt(ctx->type_i1, 1, 0), layer_ptr);
                llvm_stamp_domain_provenance(ctx, decl_cls, self_ptr, "layer",
                    maintain->data.zone_maintain_relation.relation_slot_name,
                    PGY_PROP_CAUSE_MAINTAIN);
            }
        }
        if (maintain == NULL)
            continue;
        for (size_t j = 0; j < stmt->data.zone_decl.state_count; j++) {
            ASTNode *state = stmt->data.zone_decl.states[j];
            const char *state_name;
            char field_name[256];
            int field_idx;
            LLVMValueRef self_ptr;
            LLVMValueRef state_ptr;
            if (state == NULL || state->type != AST_ZONE_STATE
                || !state->data.zone_state.is_relation
                || state->data.zone_state.layer_slot_name == NULL
                || state->data.zone_state.left_or_target_slot_name == NULL
                || state->data.zone_state.right_slot_name == NULL
                || maintain->data.zone_maintain_relation.relation_slot_name == NULL
                || maintain->data.zone_maintain_relation.left_slot_name == NULL
                || maintain->data.zone_maintain_relation.right_slot_name == NULL
                || strcmp(state->data.zone_state.layer_slot_name,
                          maintain->data.zone_maintain_relation.relation_slot_name) != 0
                || strcmp(state->data.zone_state.left_or_target_slot_name,
                          maintain->data.zone_maintain_relation.left_slot_name) != 0
                || strcmp(state->data.zone_state.right_slot_name,
                          maintain->data.zone_maintain_relation.right_slot_name) != 0)
                continue;
            state_name = state->data.zone_state.state_name;
            snprintf(field_name, sizeof(field_name), "__state_%s", state_name);
            field_idx = llvm_class_field_index(decl_cls, field_name);
            if (field_idx < 0)
                continue;
            self_ptr = LLVMGetParam(sync_fn, 0);
            state_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                self_ptr, (unsigned)field_idx, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder,
                LLVMConstInt(ctx->type_i1, 1, 0), state_ptr);
            llvm_stamp_domain_provenance(ctx, decl_cls, self_ptr, "state",
                state_name, PGY_PROP_CAUSE_MAINTAIN);
        }
    }

    for (size_t i = 0; i < stmt->data.zone_decl.unlink_count; i++) {
        ASTNode *unlink = stmt->data.zone_decl.unlinks[i];
        const char *state_name = unlink != NULL ? unlink->data.zone_unlink.state_name : NULL;
        if (state_name == NULL && unlink != NULL) {
            for (size_t j = 0; j < stmt->data.zone_decl.state_count; j++) {
                ASTNode *state = stmt->data.zone_decl.states[j];
                if (state != NULL && state->type == AST_ZONE_STATE
                    && state->data.zone_state.is_relation
                    && state->data.zone_state.layer_slot_name != NULL
                    && state->data.zone_state.left_or_target_slot_name != NULL
                    && state->data.zone_state.right_slot_name != NULL
                    && unlink->data.zone_unlink.relation_slot_name != NULL
                    && unlink->data.zone_unlink.left_slot_name != NULL
                    && unlink->data.zone_unlink.right_slot_name != NULL
                    && strcmp(state->data.zone_state.layer_slot_name,
                              unlink->data.zone_unlink.relation_slot_name) == 0
                    && strcmp(state->data.zone_state.left_or_target_slot_name,
                              unlink->data.zone_unlink.left_slot_name) == 0
                    && strcmp(state->data.zone_state.right_slot_name,
                              unlink->data.zone_unlink.right_slot_name) == 0) {
                    state_name = state->data.zone_state.state_name;
                    break;
                }
            }
        }
        if (state_name != NULL) {
            char field_name[256];
            int field_idx;
            LLVMValueRef self_ptr;
            LLVMValueRef state_ptr;
            snprintf(field_name, sizeof(field_name), "__state_%s", state_name);
            field_idx = llvm_class_field_index(decl_cls, field_name);
            if (field_idx < 0)
                continue;
            self_ptr = LLVMGetParam(sync_fn, 0);
            state_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                self_ptr, (unsigned)field_idx, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder,
                LLVMConstInt(ctx->type_i1, 0, 0), state_ptr);
            llvm_stamp_domain_provenance(ctx, decl_cls, self_ptr, "state",
                state_name, PGY_PROP_CAUSE_UNLINK);
            if (unlink != NULL) {
                const char *layer_name = unlink->data.zone_unlink.relation_slot_name;
                if (layer_name == NULL) {
                    for (size_t j = 0; j < stmt->data.zone_decl.state_count; j++) {
                        ASTNode *state = stmt->data.zone_decl.states[j];
                        if (state != NULL && state->type == AST_ZONE_STATE
                            && state->data.zone_state.is_relation
                            && state->data.zone_state.state_name != NULL
                            && strcmp(state->data.zone_state.state_name, state_name) == 0) {
                            layer_name = state->data.zone_state.layer_slot_name;
                            break;
                        }
                    }
                }
                if (layer_name != NULL) {
                    char layer_field[256];
                    int layer_idx;
                    LLVMValueRef layer_ptr;
                    snprintf(layer_field, sizeof(layer_field), "__layer_active_%s", layer_name);
                    layer_idx = llvm_class_field_index(decl_cls, layer_field);
                    if (layer_idx >= 0) {
                        layer_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                            self_ptr, (unsigned)layer_idx, llvm_tmp_name(ctx));
                        LLVMBuildStore(ctx->builder,
                            LLVMConstInt(ctx->type_i1, 0, 0), layer_ptr);
                        llvm_stamp_domain_provenance(ctx, decl_cls, self_ptr,
                            "layer", layer_name, PGY_PROP_CAUSE_UNLINK);
                    }
                }
            }
        } else if (unlink != NULL && unlink->data.zone_unlink.relation_slot_name != NULL) {
            char layer_field[256];
            int layer_idx;
            LLVMValueRef self_ptr = LLVMGetParam(sync_fn, 0);
            LLVMValueRef layer_ptr;
            snprintf(layer_field, sizeof(layer_field), "__layer_active_%s",
                unlink->data.zone_unlink.relation_slot_name);
            layer_idx = llvm_class_field_index(decl_cls, layer_field);
            if (layer_idx >= 0) {
                layer_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                    self_ptr, (unsigned)layer_idx, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder,
                    LLVMConstInt(ctx->type_i1, 0, 0), layer_ptr);
            }
        }
    }

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
            snprintf(field_name, sizeof(field_name), "__state_%s", state_name);
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
        snprintf(field_name, sizeof(field_name), "__layer_active_%s",
            slot->data.zone_layer_slot.slot_name);
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

    LLVMBuildBr(ctx->builder, frontier_check_bb);

    LLVMPositionBuilderAtEnd(ctx->builder, frontier_done_bb);
    {
        LLVMValueRef continue_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
            frontier_continue_addr, llvm_tmp_name(ctx));
        LLVMBuildCondBr(ctx->builder, continue_val, frontier_overflow_bb, frontier_exit_bb);
    }

    LLVMPositionBuilderAtEnd(ctx->builder, frontier_overflow_bb);
    {
        LLVMTypeRef abort_ft = LLVMFunctionType(ctx->type_void, NULL, 0, 0);
        LLVMFuncEntry *abort_fn = llvm_lookup_or_create_function(ctx, "abort",
            abort_ft, ctx->type_void);
        if (abort_fn != NULL) {
            LLVMBuildCall2(ctx->builder, abort_fn->fn_type, abort_fn->fn,
                NULL, 0, "");
        }
        LLVMBuildUnreachable(ctx->builder);
    }

    LLVMPositionBuilderAtEnd(ctx->builder, frontier_exit_bb);
    LLVMBuildRetVoid(ctx->builder);
    llvm_scope_pop(ctx);
    ctx->current_function = saved_fn;
    ctx->current_ret_type = saved_ret;
    llvm_restore_current_host_decl(ctx, saved_host_decl);

    if (saved_fn != NULL) {
        LLVMBasicBlockRef last = LLVMGetLastBasicBlock(saved_fn);
        if (last != NULL)
            LLVMPositionBuilderAtEnd(ctx->builder, last);
    }
}

#endif /* PGY_LLVM_ENABLED */
