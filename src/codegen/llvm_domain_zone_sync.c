#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"
#include "llvm_domain_decl_parts_helpers.h"
#include "domain_frontier_policy.h"
#include "llvm_domain_zone_bind_helpers.h"
#include "llvm_domain_sync_frontier.h"
#include "llvm_domain_projection_value_helpers.h"
#include "llvm_domain_projection_sync_body_helpers.h"
#include "llvm_domain_zone_sync_internal.h"
#include "parser/ast_api.h"

static bool
llvm_zone_sync_field_name(char *out,
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
        (unsigned long long)pgy_domain_zone_frontier_pass_limit(stmt), 0);
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

    llvm_zone_sync_emit_action_causes(stmt, decl_cls, sync_fn, ctx);

    size_t state_count = 0;
    ASTNode **states = ast_zone_states(stmt, &state_count);
    size_t apply_count = 0;
    ASTNode **applies = ast_zone_applies(stmt, &apply_count);
    for (size_t i = 0; i < apply_count; i++) {
        ASTNode *apply = applies[i];
        const char *state_name = apply != NULL ? apply->data.zone_apply.state_name : NULL;
        if (state_name == NULL && apply != NULL) {
            for (size_t j = 0; j < state_count; j++) {
                ASTNode *state = states[j];
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
            if (!llvm_zone_sync_field_name(field_name, sizeof(field_name),
                    "state", state_name))
                continue;
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
                    for (size_t j = 0; j < state_count; j++) {
                        ASTNode *state = states[j];
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
                    if (!llvm_zone_sync_field_name(layer_field,
                            sizeof(layer_field), "layer_active", layer_name))
                        continue;
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
                        for (size_t j = 0; j < state_count; j++) {
                            ASTNode *state = states[j];
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
            if (!llvm_zone_sync_field_name(layer_field, sizeof(layer_field),
                    "layer_active", apply->data.zone_apply.effect_slot_name))
                continue;
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

    size_t maintained_effect_count = 0;
    ASTNode **maintained_effects =
        ast_zone_maintained_effects(stmt, &maintained_effect_count);
    for (size_t i = 0; i < maintained_effect_count; i++) {
        ASTNode *maintain = maintained_effects[i];
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
            if (!llvm_zone_sync_field_name(layer_field, sizeof(layer_field),
                    "layer_active",
                    maintain->data.zone_maintain_effect.effect_slot_name))
                continue;
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
        for (size_t j = 0; j < state_count; j++) {
            ASTNode *state = states[j];
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
            if (!llvm_zone_sync_field_name(field_name, sizeof(field_name),
                    "state", state_name))
                continue;
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

    size_t maintained_state_count = 0;
    ASTNode **maintained_states =
        ast_zone_maintained_states(stmt, &maintained_state_count);
    for (size_t i = 0; i < maintained_state_count; i++) {
        ASTNode *maintain = maintained_states[i];
        if (maintain != NULL && maintain->data.zone_maintain_state.state_name != NULL) {
            char field_name[256];
            int field_idx;
            LLVMValueRef self_ptr;
            LLVMValueRef state_ptr;
            if (!llvm_zone_sync_field_name(field_name, sizeof(field_name),
                    "state", maintain->data.zone_maintain_state.state_name))
                continue;
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
            for (size_t j = 0; j < state_count; j++) {
                ASTNode *state = states[j];
                if (state != NULL && state->type == AST_ZONE_STATE
                    && state->data.zone_state.state_name != NULL
                    && strcmp(state->data.zone_state.state_name,
                              maintain->data.zone_maintain_state.state_name) == 0) {
                    char layer_field[256];
                    int layer_idx;
                    LLVMValueRef layer_ptr;
                    if (!llvm_zone_sync_field_name(layer_field,
                            sizeof(layer_field), "layer_active",
                            state->data.zone_state.layer_slot_name))
                        continue;
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

    llvm_zone_sync_emit_detach_clauses(stmt, decl_cls, sync_fn, ctx);

    llvm_zone_sync_emit_relation_clauses(stmt, decl_cls, sync_fn, ctx);

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
    llvm_emit_frontier_overflow_abort(ctx,
        PGY_FRONTIER_REASON_ZONE_OVERFLOW);

    LLVMPositionBuilderAtEnd(ctx->builder, frontier_exit_bb);
    LLVMBuildRetVoid(ctx->builder);
    llvm_scope_pop(ctx);
    llvm_finish_domain_sync_emit(ctx, saved_fn, saved_ret, saved_host_decl);
}

#endif /* PGY_LLVM_ENABLED */
