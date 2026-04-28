#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"
#include "llvm_domain_decl_parts_helpers.h"
#include "llvm_domain_zone_bind_helpers.h"
#include "llvm_domain_projection_value_helpers.h"
#include "llvm_domain_projection_sync_body_helpers.h"
#include "llvm_domain_sync_frontier.h"
#include "llvm_domain_zone_sync_internal.h"
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
    llvm_emit_sync_generation_increment(ctx, decl_cls, LLVMGetParam(sync_fn, 0));
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
    LLVMValueRef *prev_state_addrs = NULL;
    LLVMValueRef *prev_layer_addrs = NULL;
    llvm_zone_sync_alloc_previous_state(stmt, ctx,
        &prev_state_addrs, &prev_layer_addrs);

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

    llvm_zone_sync_snapshot_previous_state(stmt, decl_cls, sync_fn, ctx,
        prev_state_addrs, prev_layer_addrs);

    llvm_zone_sync_reset_state_and_layers(stmt, decl_cls, sync_fn, ctx);

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

    llvm_zone_sync_update_frontier_continue(stmt, decl_cls, sync_fn, ctx,
        prev_state_addrs, prev_layer_addrs, frontier_continue_addr);

    LLVMBuildBr(ctx->builder, frontier_check_bb);

    LLVMPositionBuilderAtEnd(ctx->builder, frontier_done_bb);
    {
        LLVMValueRef continue_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
            frontier_continue_addr, llvm_tmp_name(ctx));
        LLVMBuildCondBr(ctx->builder, continue_val, frontier_overflow_bb, frontier_exit_bb);
    }

    LLVMPositionBuilderAtEnd(ctx->builder, frontier_overflow_bb);
    llvm_emit_frontier_overflow_abort(ctx);

    LLVMPositionBuilderAtEnd(ctx->builder, frontier_exit_bb);
    LLVMBuildRetVoid(ctx->builder);
    llvm_scope_pop(ctx);
    llvm_finish_domain_sync_emit(ctx, saved_fn, saved_ret, saved_host_decl);
}

#endif /* PGY_LLVM_ENABLED */
