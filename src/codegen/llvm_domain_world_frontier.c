#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"
#include "llvm_domain_sync_frontier.h"
#include "llvm_domain_world_sync_internal.h"
#include "llvm_inventory_decl_lookup.h"
#include "domain_frontier_policy.h"

static ASTNode *
llvm_world_frontier_lookup_zone(void *ctx, const char *zone_name)
{
    return llvm_find_decl_in_active_inventory(
        (LLVMGenCtx *)ctx, AST_ZONE_DECL, zone_name);
}

void
llvm_world_sync_emit_frontier(ASTNode *stmt, LLVMClassTypeEntry *decl_cls,
                              LLVMValueRef sync_fn,
                              LLVMValueRef derived_dirty_addr,
                              LLVMValueRef needs_derived_addr,
                              LLVMValueRef derived_ptr,
                              LLVMGenCtx *ctx)
{
    LLVMValueRef frontier_pass_addr;
    LLVMValueRef frontier_continue_addr;
    LLVMValueRef pass_addr;
    LLVMValueRef continue_addr;
    LLVMValueRef changed_any_addr;
    LLVMValueRef frontier_limit_val;
    LLVMValueRef limit_val;
    LLVMBasicBlockRef frontier_check_bb;
    LLVMBasicBlockRef frontier_body_bb;
    LLVMBasicBlockRef frontier_done_bb;
    LLVMBasicBlockRef frontier_overflow_bb;
    LLVMBasicBlockRef frontier_exit_bb;
    LLVMBasicBlockRef derived_init_bb;
    LLVMBasicBlockRef loop_check_bb;
    LLVMBasicBlockRef loop_body_bb;
    LLVMBasicBlockRef overflow_bb;
    LLVMBasicBlockRef finalize_bb;
    LLVMBasicBlockRef done_bb;
    LLVMBasicBlockRef derived_exit_bb;
    size_t zone_count;

    if (stmt == NULL || stmt->type != AST_WORLD_DECL || decl_cls == NULL
        || sync_fn == NULL || derived_dirty_addr == NULL
        || needs_derived_addr == NULL || ctx == NULL)
        return;

    zone_count = stmt->data.world_decl.zone_count;
    frontier_pass_addr = llvm_create_entry_alloca(ctx, ctx->type_i32,
        "world.frontier.pass.addr");
    frontier_continue_addr = llvm_create_entry_alloca(ctx, ctx->type_i1,
        "world.frontier.continue.addr");
    pass_addr = llvm_create_entry_alloca(ctx, ctx->type_i32,
        "world.derived.pass.addr");
    continue_addr = llvm_create_entry_alloca(ctx, ctx->type_i1,
        "world.derived.continue.addr");
    changed_any_addr = llvm_create_entry_alloca(ctx, ctx->type_i1,
        "world.derived.changed_any.addr");
    frontier_limit_val = LLVMConstInt(ctx->type_i32,
        (unsigned long long)pgy_domain_world_transitive_frontier_pass_limit(
            stmt, pgy_domain_world_embedded_frontier_count(
                stmt, llvm_world_frontier_lookup_zone, ctx)), 0);
    limit_val = LLVMConstInt(ctx->type_i32,
        (unsigned long long)pgy_domain_world_derived_frontier_pass_limit(stmt), 0);
    frontier_check_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn,
        "world.frontier.check");
    frontier_body_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn,
        "world.frontier.body");
    frontier_done_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn,
        "world.frontier.done");
    frontier_overflow_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn,
        "world.frontier.overflow");
    frontier_exit_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn,
        "world.frontier.exit");
    derived_init_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn,
        "world.derived.init");
    loop_check_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn,
        "world.derived.check");
    loop_body_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn,
        "world.derived.body");
    overflow_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn,
        "world.derived.overflow");
    finalize_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn,
        "world.derived.finalize");
    done_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn,
        "world.derived.done");
    derived_exit_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn,
        "world.derived.exit");

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
    LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0), changed_any_addr);
    {
        LLVMValueRef pass_val = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
            frontier_pass_addr, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder,
            LLVMBuildAdd(ctx->builder, pass_val,
                LLVMConstInt(ctx->type_i32, 1, 0), llvm_tmp_name(ctx)),
            frontier_pass_addr);
    }
    if (derived_ptr != NULL) {
        LLVMBuildStore(ctx->builder,
            LLVMBuildLoad2(ctx->builder, ctx->type_i1, derived_ptr, llvm_tmp_name(ctx)),
            needs_derived_addr);
    } else {
        LLVMBuildStore(ctx->builder,
            LLVMBuildLoad2(ctx->builder, ctx->type_i1, derived_dirty_addr,
                llvm_tmp_name(ctx)),
            needs_derived_addr);
    }

    for (size_t i = 0; i < zone_count; i++) {
        ASTNode *zone = stmt->data.world_decl.zones[i];
        int zone_idx;
        int dirty_idx;
        int seen_idx;
        char dirty_field[256];
        char seen_field[256];
        LLVMValueRef self_ptr;
        LLVMValueRef dirty_ptr;
        LLVMValueRef dirty_val;
        LLVMBasicBlockRef sync_bb;
        LLVMBasicBlockRef cont_bb;
        if (zone == NULL || zone->type != AST_WORLD_ZONE
            || zone->data.world_zone.slot_name == NULL)
            continue;
        zone_idx = llvm_class_field_index(decl_cls, zone->data.world_zone.slot_name);
        snprintf(dirty_field, sizeof(dirty_field), "__zone_dirty_%s",
            zone->data.world_zone.slot_name);
        snprintf(seen_field, sizeof(seen_field), "__zone_seen_generation_%s",
            zone->data.world_zone.slot_name);
        dirty_idx = llvm_class_field_index(decl_cls, dirty_field);
        seen_idx = llvm_class_field_index(decl_cls, seen_field);
        self_ptr = LLVMGetParam(sync_fn, 0);
        if (zone_idx < 0 || dirty_idx < 0 || zone->data.world_zone.zone_type == NULL)
            continue;
        {
            LLVMClassTypeEntry *zone_cls = llvm_lookup_class(ctx, zone->data.world_zone.zone_type);
            char sync_name[256];
            LLVMFuncEntry *zone_sync;
            snprintf(sync_name, sizeof(sync_name), "%s_sync",
                zone->data.world_zone.zone_type);
            zone_sync = llvm_lookup_function(ctx, sync_name);
            if (zone_cls == NULL || zone_sync == NULL)
                continue;
            dirty_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                self_ptr, (unsigned)dirty_idx, llvm_tmp_name(ctx));
            if (seen_idx >= 0) {
                int generation_idx = llvm_class_field_index(zone_cls, "__sync_generation");
                if (generation_idx >= 0) {
                    LLVMValueRef zone_ptr = LLVMBuildStructGEP2(ctx->builder,
                        decl_cls->struct_type, self_ptr, (unsigned)zone_idx,
                        llvm_tmp_name(ctx));
                    LLVMValueRef generation_ptr = LLVMBuildStructGEP2(ctx->builder,
                        zone_cls->struct_type, zone_ptr, (unsigned)generation_idx,
                        llvm_tmp_name(ctx));
                    LLVMValueRef seen_ptr = LLVMBuildStructGEP2(ctx->builder,
                        decl_cls->struct_type, self_ptr, (unsigned)seen_idx,
                        llvm_tmp_name(ctx));
                    LLVMValueRef generation_val = LLVMBuildLoad2(ctx->builder,
                        ctx->type_i32, generation_ptr, llvm_tmp_name(ctx));
                    LLVMValueRef seen_val = LLVMBuildLoad2(ctx->builder,
                        ctx->type_i32, seen_ptr, llvm_tmp_name(ctx));
                    LLVMValueRef generation_changed = LLVMBuildICmp(ctx->builder,
                        LLVMIntNE, generation_val, seen_val, llvm_tmp_name(ctx));
                    LLVMValueRef dirty_prev = LLVMBuildLoad2(ctx->builder,
                        ctx->type_i1, dirty_ptr, llvm_tmp_name(ctx));
                    LLVMValueRef dirty_next = LLVMBuildOr(ctx->builder,
                        dirty_prev, generation_changed, llvm_tmp_name(ctx));
                    LLVMBuildStore(ctx->builder, dirty_next, dirty_ptr);
                    if (derived_ptr != NULL) {
                        LLVMValueRef derived_next = LLVMBuildOr(ctx->builder,
                            LLVMBuildLoad2(ctx->builder, ctx->type_i1,
                                derived_ptr, llvm_tmp_name(ctx)),
                            generation_changed, llvm_tmp_name(ctx));
                        LLVMBuildStore(ctx->builder, derived_next, derived_ptr);
                    }
                }
            }
            dirty_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
                dirty_ptr, llvm_tmp_name(ctx));
            sync_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn, "world.zone.sync");
            cont_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn, "world.zone.cont");
            LLVMBuildCondBr(ctx->builder, dirty_val, sync_bb, cont_bb);
            LLVMPositionBuilderAtEnd(ctx->builder, sync_bb);
            {
                LLVMValueRef zone_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                    self_ptr, (unsigned)zone_idx, llvm_tmp_name(ctx));
                LLVMValueRef args[] = { zone_ptr };
                LLVMBuildCall2(ctx->builder, zone_sync->fn_type, zone_sync->fn, args, 1, "");
                if (seen_idx >= 0) {
                    int generation_idx = llvm_class_field_index(zone_cls, "__sync_generation");
                    if (generation_idx >= 0) {
                        LLVMValueRef generation_ptr = LLVMBuildStructGEP2(ctx->builder,
                            zone_cls->struct_type, zone_ptr, (unsigned)generation_idx,
                            llvm_tmp_name(ctx));
                        LLVMValueRef seen_ptr = LLVMBuildStructGEP2(ctx->builder,
                            decl_cls->struct_type, self_ptr, (unsigned)seen_idx,
                            llvm_tmp_name(ctx));
                        LLVMValueRef generation_val = LLVMBuildLoad2(ctx->builder,
                            ctx->type_i32, generation_ptr, llvm_tmp_name(ctx));
                        LLVMBuildStore(ctx->builder, generation_val, seen_ptr);
                    }
                }
            }
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0), dirty_ptr);
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0), needs_derived_addr);
            LLVMBuildBr(ctx->builder, cont_bb);
            LLVMPositionBuilderAtEnd(ctx->builder, cont_bb);
        }
    }

    LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i32, 0, 0), pass_addr);
    LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0), continue_addr);
    {
        LLVMValueRef needs_derived = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
            needs_derived_addr, llvm_tmp_name(ctx));
        LLVMBuildCondBr(ctx->builder, needs_derived, derived_init_bb, done_bb);
    }

    LLVMPositionBuilderAtEnd(ctx->builder, derived_init_bb);
    LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i32, 0, 0), pass_addr);
    LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0), continue_addr);
    LLVMBuildBr(ctx->builder, loop_check_bb);

    LLVMPositionBuilderAtEnd(ctx->builder, loop_check_bb);
    {
        LLVMValueRef continue_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
            continue_addr, llvm_tmp_name(ctx));
        LLVMValueRef pass_val = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
            pass_addr, llvm_tmp_name(ctx));
        LLVMValueRef under_limit = LLVMBuildICmp(ctx->builder, LLVMIntULT,
            pass_val, limit_val, llvm_tmp_name(ctx));
        LLVMValueRef loop_cond = LLVMBuildAnd(ctx->builder, continue_val,
            under_limit, llvm_tmp_name(ctx));
        LLVMBuildCondBr(ctx->builder, loop_cond, loop_body_bb, done_bb);
    }

    LLVMPositionBuilderAtEnd(ctx->builder, loop_body_bb);
    LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0), continue_addr);
    {
        LLVMValueRef pass_val = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
            pass_addr, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder,
            LLVMBuildAdd(ctx->builder, pass_val,
                LLVMConstInt(ctx->type_i32, 1, 0), llvm_tmp_name(ctx)),
            pass_addr);
    }
    for (size_t i = 0; i < stmt->data.world_decl.state_count; i++) {
        ASTNode *state = stmt->data.world_decl.states[i];
        const char *slot_name;
        char state_field[256];
        char active_field[256];
        int state_idx;
        int active_idx = -1;
        LLVMValueRef self_ptr;
        LLVMValueRef state_ptr;
        LLVMValueRef prev_state_val;
        LLVMValueRef active_ptr = NULL;
        LLVMValueRef active_val = LLVMConstInt(ctx->type_i1, 0, 0);
        LLVMValueRef derived_val = NULL;
        LLVMValueRef changed_val;
        if (state == NULL || state->type != AST_WORLD_STATE
            || state->data.world_state.state_name == NULL)
            continue;
        slot_name = state->data.world_state.zone_slot_name;
        snprintf(state_field, sizeof(state_field), "__zone_state_%s",
            state->data.world_state.state_name);
        state_idx = llvm_class_field_index(decl_cls, state_field);
        if (state_idx < 0)
            continue;
        self_ptr = LLVMGetParam(sync_fn, 0);
        state_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
            self_ptr, (unsigned)state_idx, llvm_tmp_name(ctx));
        prev_state_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
            state_ptr, llvm_tmp_name(ctx));
        if (slot_name != NULL) {
            snprintf(active_field, sizeof(active_field), "__zone_active_%s", slot_name);
            active_idx = llvm_class_field_index(decl_cls, active_field);
            if (active_idx >= 0) {
                active_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                    self_ptr, (unsigned)active_idx, llvm_tmp_name(ctx));
                active_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
                    active_ptr, llvm_tmp_name(ctx));
            }
        }
        derived_val = active_val;

        if (state->data.world_state.source_kind == WORLD_STATE_SOURCE_ALL
            || state->data.world_state.source_kind == WORLD_STATE_SOURCE_ANY) {
            derived_val = LLVMConstInt(ctx->type_i1,
                state->data.world_state.source_kind == WORLD_STATE_SOURCE_ALL ? 1 : 0, 0);
            for (size_t input_i = 0; input_i < state->data.world_state.input_count; input_i++) {
                const char *input_name = state->data.world_state.input_names[input_i];
                int input_idx = -1;
                LLVMValueRef input_ptr;
                LLVMValueRef input_val;
                if (input_name == NULL)
                    continue;
                if (llvm_world_sync_has_zone_slot(stmt, input_name)) {
                    char input_field[256];
                    snprintf(input_field, sizeof(input_field), "__zone_active_%s", input_name);
                    input_idx = llvm_class_field_index(decl_cls, input_field);
                } else {
                    char input_field[256];
                    snprintf(input_field, sizeof(input_field), "__zone_state_%s", input_name);
                    input_idx = llvm_class_field_index(decl_cls, input_field);
                }
                if (input_idx < 0)
                    continue;
                input_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                    self_ptr, (unsigned)input_idx, llvm_tmp_name(ctx));
                input_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
                    input_ptr, llvm_tmp_name(ctx));
                if (state->data.world_state.source_kind == WORLD_STATE_SOURCE_ALL)
                    derived_val = LLVMBuildAnd(ctx->builder, derived_val, input_val,
                        llvm_tmp_name(ctx));
                else
                    derived_val = LLVMBuildOr(ctx->builder, derived_val, input_val,
                        llvm_tmp_name(ctx));
            }
        }

        if (state->data.world_state.source_kind != WORLD_STATE_SOURCE_ZONE
            && state->data.world_state.source_kind != WORLD_STATE_SOURCE_ALL
            && state->data.world_state.source_kind != WORLD_STATE_SOURCE_ANY
            && state->data.world_state.detail_name != NULL) {
            int zone_idx = llvm_class_field_index(decl_cls, slot_name);
            LLVMClassTypeEntry *zone_cls = NULL;
            if (zone_idx >= 0) {
                LLVMTypeRef zone_field_ty = decl_cls->fields[zone_idx].field_type;
                zone_cls = llvm_lookup_class_by_struct_type(ctx, zone_field_ty);
            }
            if (zone_cls != NULL && zone_idx >= 0) {
                char detail_field[256];
                int detail_idx = -1;
                LLVMValueRef zone_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                    self_ptr, (unsigned)zone_idx, llvm_tmp_name(ctx));
                LLVMValueRef detail_ptr;
                LLVMValueRef detail_val;

                switch (state->data.world_state.source_kind) {
                case WORLD_STATE_SOURCE_PROJECTION:
                    snprintf(detail_field, sizeof(detail_field), "__projection_ready_%s",
                        state->data.world_state.detail_name);
                    break;
                case WORLD_STATE_SOURCE_LAYER:
                    snprintf(detail_field, sizeof(detail_field), "__layer_active_%s",
                        state->data.world_state.detail_name);
                    break;
                case WORLD_STATE_SOURCE_STATE:
                    snprintf(detail_field, sizeof(detail_field), "__state_%s",
                        state->data.world_state.detail_name);
                    break;
                case WORLD_STATE_SOURCE_ZONE:
                default:
                    detail_field[0] = '\0';
                    break;
                }

                if (detail_field[0] != '\0')
                    detail_idx = llvm_class_field_index(zone_cls, detail_field);
                if (detail_idx >= 0) {
                    detail_ptr = LLVMBuildStructGEP2(ctx->builder, zone_cls->struct_type,
                        zone_ptr, (unsigned)detail_idx, llvm_tmp_name(ctx));
                    detail_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
                        detail_ptr, llvm_tmp_name(ctx));
                    derived_val = LLVMBuildAnd(ctx->builder, active_val, detail_val,
                        llvm_tmp_name(ctx));
                }
            }
        }

        LLVMBuildStore(ctx->builder, derived_val, state_ptr);
        changed_val = LLVMBuildICmp(ctx->builder, LLVMIntNE, prev_state_val,
            derived_val, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder,
            LLVMBuildOr(ctx->builder,
                LLVMBuildLoad2(ctx->builder, ctx->type_i1, continue_addr, llvm_tmp_name(ctx)),
                changed_val, llvm_tmp_name(ctx)),
            continue_addr);
        LLVMBuildStore(ctx->builder,
            LLVMBuildOr(ctx->builder,
                LLVMBuildLoad2(ctx->builder, ctx->type_i1, changed_any_addr,
                    llvm_tmp_name(ctx)),
                changed_val, llvm_tmp_name(ctx)),
            changed_any_addr);
        llvm_stamp_domain_provenance(ctx, decl_cls, self_ptr, "zone_state",
            state->data.world_state.state_name, PGY_PROP_CAUSE_WORLD_DERIVED);
    }
    LLVMBuildBr(ctx->builder, loop_check_bb);

    LLVMPositionBuilderAtEnd(ctx->builder, done_bb);
    {
        LLVMValueRef continue_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
            continue_addr, llvm_tmp_name(ctx));
        LLVMBuildCondBr(ctx->builder, continue_val, overflow_bb, finalize_bb);
    }

    LLVMPositionBuilderAtEnd(ctx->builder, overflow_bb);
    llvm_emit_frontier_overflow_abort(ctx,
        "world derived recompute exceeded bounded pass limit");

    LLVMPositionBuilderAtEnd(ctx->builder, finalize_bb);
    LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0), derived_dirty_addr);
    if (derived_ptr != NULL)
        LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0), derived_ptr);
    LLVMBuildBr(ctx->builder, derived_exit_bb);
    LLVMPositionBuilderAtEnd(ctx->builder, derived_exit_bb);
    {
        LLVMValueRef pending_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
            changed_any_addr, llvm_tmp_name(ctx));
        LLVMValueRef dirty_pending = derived_ptr != NULL
            ? LLVMBuildLoad2(ctx->builder, ctx->type_i1, derived_ptr, llvm_tmp_name(ctx))
            : LLVMBuildLoad2(ctx->builder, ctx->type_i1, derived_dirty_addr,
                llvm_tmp_name(ctx));
        pending_val = LLVMBuildOr(ctx->builder, pending_val, dirty_pending,
            llvm_tmp_name(ctx));
        for (size_t i = 0; i < zone_count; i++) {
            ASTNode *zone = stmt->data.world_decl.zones[i];
            char dirty_field[256];
            int dirty_idx;
            LLVMValueRef self_ptr;
            LLVMValueRef dirty_ptr;
            LLVMValueRef dirty_val;
            if (zone == NULL || zone->type != AST_WORLD_ZONE
                || zone->data.world_zone.slot_name == NULL)
                continue;
            snprintf(dirty_field, sizeof(dirty_field), "__zone_dirty_%s",
                zone->data.world_zone.slot_name);
            dirty_idx = llvm_class_field_index(decl_cls, dirty_field);
            if (dirty_idx < 0)
                continue;
            self_ptr = LLVMGetParam(sync_fn, 0);
            dirty_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                self_ptr, (unsigned)dirty_idx, llvm_tmp_name(ctx));
            dirty_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
                dirty_ptr, llvm_tmp_name(ctx));
            pending_val = LLVMBuildOr(ctx->builder, pending_val, dirty_val,
                llvm_tmp_name(ctx));
        }
        LLVMBuildStore(ctx->builder, pending_val, frontier_continue_addr);
    }
    LLVMBuildBr(ctx->builder, frontier_check_bb);

    LLVMPositionBuilderAtEnd(ctx->builder, frontier_done_bb);
    {
        LLVMValueRef continue_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
            frontier_continue_addr, llvm_tmp_name(ctx));
        LLVMBuildCondBr(ctx->builder, continue_val, frontier_overflow_bb, frontier_exit_bb);
    }

    LLVMPositionBuilderAtEnd(ctx->builder, frontier_overflow_bb);
    llvm_emit_frontier_overflow_abort(ctx,
        "world frontier recompute exceeded bounded pass limit");

    LLVMPositionBuilderAtEnd(ctx->builder, frontier_exit_bb);
}

#endif /* PGY_LLVM_ENABLED */
