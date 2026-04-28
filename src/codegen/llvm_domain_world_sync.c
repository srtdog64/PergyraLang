#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"
#include "llvm_domain_world_sync_internal.h"

void
llvm_emit_world_sync(ASTNode *stmt, const char *decl_name,
                     LLVMClassTypeEntry *decl_cls, LLVMValueRef sync_fn,
                     LLVMGenCtx *ctx)
{
    LLVMValueRef saved_fn;
    LLVMTypeRef saved_ret;
    ASTNode *saved_host_decl;
    LLVMBasicBlockRef bb;

    if (stmt == NULL || stmt->type != AST_WORLD_DECL || decl_name == NULL
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
        LLVMValueRef derived_dirty_addr = llvm_create_entry_alloca(ctx, ctx->type_i1,
            "world.derived_dirty.addr");
        LLVMValueRef needs_derived_addr = llvm_create_entry_alloca(ctx, ctx->type_i1,
            "world.needs_derived.addr");
        int derived_idx = llvm_class_field_index(decl_cls, "__world_derived_dirty");
        LLVMValueRef derived_ptr = NULL;
        LLVMValueRef derived_val = LLVMConstInt(ctx->type_i1, 0, 0);
        size_t zone_count = stmt->data.world_decl.zone_count;
        /* Per-zone "previously active" pointer cache ??populated during
         * world sync emission and consumed once before this function
         * returns.  Never escapes. */
        LLVMValueRef *prev_active_addrs = pgy_arena_calloc(&ctx->scratch,
            (zone_count > 0 ? zone_count : 1) * sizeof(LLVMValueRef));

        LLVMBuildStore(ctx->builder, LLVMGetParam(sync_fn, 0), sa);
        llvm_scope_declare(ctx, "self", sa, self_ptr_t);
        llvm_register_var_class(ctx, "self", decl_name);
        llvm_scope_declare(ctx, "__world_derived_dirty_local", derived_dirty_addr, ctx->type_i1);

        if (derived_idx >= 0) {
            derived_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                LLVMGetParam(sync_fn, 0), (unsigned)derived_idx, llvm_tmp_name(ctx));
            derived_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
                derived_ptr, llvm_tmp_name(ctx));
        }
        LLVMBuildStore(ctx->builder, derived_val, derived_dirty_addr);

        /* world command pass: reset */
        for (size_t i = 0; i < zone_count; i++) {
            ASTNode *zone = stmt->data.world_decl.zones[i];
            char active_field[256];
            char prev_name[256];
            int active_idx;
            LLVMValueRef self_ptr;
            LLVMValueRef active_ptr;
            LLVMValueRef prev_addr;
            LLVMValueRef prev_val;
            if (zone == NULL || zone->type != AST_WORLD_ZONE
                || zone->data.world_zone.slot_name == NULL)
                continue;
            snprintf(active_field, sizeof(active_field), "__zone_active_%s",
                zone->data.world_zone.slot_name);
            active_idx = llvm_class_field_index(decl_cls, active_field);
            self_ptr = LLVMGetParam(sync_fn, 0);
            if (active_idx < 0)
                continue;
            active_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                self_ptr, (unsigned)active_idx, llvm_tmp_name(ctx));
            snprintf(prev_name, sizeof(prev_name), "world.prev_active.%s",
                zone->data.world_zone.slot_name);
            prev_addr = llvm_create_entry_alloca(ctx, ctx->type_i1, prev_name);
            prev_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
                active_ptr, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, prev_val, prev_addr);
            prev_active_addrs[i] = prev_addr;
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0), active_ptr);
        }

        /* world command pass: directives */
        llvm_world_sync_emit_directives(stmt, decl_cls, sync_fn, ctx);

        for (size_t i = 0; i < zone_count; i++) {
            ASTNode *zone = stmt->data.world_decl.zones[i];
            const char *slot_name;
            char active_field[256];
            char dirty_field[256];
            int active_idx;
            int dirty_idx;
            LLVMValueRef self_ptr;
            LLVMValueRef active_ptr;
            LLVMValueRef dirty_ptr;
            LLVMValueRef active_val;
            LLVMValueRef prev_val;
            LLVMValueRef changed_val;
            if (zone == NULL || zone->type != AST_WORLD_ZONE
                || zone->data.world_zone.slot_name == NULL
                || prev_active_addrs[i] == NULL)
                continue;
            slot_name = zone->data.world_zone.slot_name;
            snprintf(active_field, sizeof(active_field), "__zone_active_%s", slot_name);
            snprintf(dirty_field, sizeof(dirty_field), "__zone_dirty_%s", slot_name);
            active_idx = llvm_class_field_index(decl_cls, active_field);
            dirty_idx = llvm_class_field_index(decl_cls, dirty_field);
            if (active_idx < 0 || dirty_idx < 0)
                continue;
            self_ptr = LLVMGetParam(sync_fn, 0);
            active_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                self_ptr, (unsigned)active_idx, llvm_tmp_name(ctx));
            dirty_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                self_ptr, (unsigned)dirty_idx, llvm_tmp_name(ctx));
            active_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
                active_ptr, llvm_tmp_name(ctx));
            prev_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
                prev_active_addrs[i], llvm_tmp_name(ctx));
            changed_val = LLVMBuildICmp(ctx->builder, LLVMIntNE, active_val, prev_val,
                llvm_tmp_name(ctx));
            {
                LLVMValueRef prev_dirty = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
                    dirty_ptr, llvm_tmp_name(ctx));
                changed_val = LLVMBuildOr(ctx->builder, prev_dirty, changed_val,
                    llvm_tmp_name(ctx));
            }
            LLVMBuildStore(ctx->builder, changed_val, dirty_ptr);
            {
                LLVMValueRef derived_dirty_val = LLVMBuildOr(ctx->builder,
                    LLVMBuildLoad2(ctx->builder, ctx->type_i1, derived_dirty_addr,
                        llvm_tmp_name(ctx)),
                    changed_val, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder, derived_dirty_val, derived_dirty_addr);
                if (derived_ptr != NULL)
                    LLVMBuildStore(ctx->builder, derived_dirty_val, derived_ptr);
            }
        }

        {
            LLVMValueRef frontier_pass_addr = llvm_create_entry_alloca(ctx, ctx->type_i32,
                "world.frontier.pass.addr");
            LLVMValueRef frontier_continue_addr = llvm_create_entry_alloca(ctx, ctx->type_i1,
                "world.frontier.continue.addr");
            LLVMValueRef pass_addr = llvm_create_entry_alloca(ctx, ctx->type_i32,
                "world.derived.pass.addr");
            LLVMValueRef continue_addr = llvm_create_entry_alloca(ctx, ctx->type_i1,
                "world.derived.continue.addr");
            LLVMValueRef frontier_limit_val = LLVMConstInt(ctx->type_i32,
                (unsigned long long)(zone_count + stmt->data.world_decl.state_count + 1), 0);
            LLVMValueRef limit_val = LLVMConstInt(ctx->type_i32,
                (unsigned long long)(stmt->data.world_decl.state_count + 1), 0);
            LLVMBasicBlockRef frontier_check_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn,
                "world.frontier.check");
            LLVMBasicBlockRef frontier_body_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn,
                "world.frontier.body");
            LLVMBasicBlockRef frontier_done_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn,
                "world.frontier.done");
            LLVMBasicBlockRef frontier_overflow_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn,
                "world.frontier.overflow");
            LLVMBasicBlockRef frontier_exit_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn,
                "world.frontier.exit");
            LLVMBasicBlockRef derived_init_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn,
                "world.derived.init");
            LLVMBasicBlockRef loop_check_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn,
                "world.derived.check");
            LLVMBasicBlockRef loop_body_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn,
                "world.derived.body");
            LLVMBasicBlockRef overflow_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn,
                "world.derived.overflow");
            LLVMBasicBlockRef finalize_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn,
                "world.derived.finalize");
            LLVMBasicBlockRef done_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn,
                "world.derived.done");
            LLVMBasicBlockRef derived_exit_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn,
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

            /* world zone sync pass */
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

            LLVMPositionBuilderAtEnd(ctx->builder, finalize_bb);
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0), derived_dirty_addr);
            if (derived_ptr != NULL)
                LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0), derived_ptr);
            LLVMBuildBr(ctx->builder, derived_exit_bb);
            LLVMPositionBuilderAtEnd(ctx->builder, derived_exit_bb);
            {
                LLVMValueRef pending_val = derived_ptr != NULL
                    ? LLVMBuildLoad2(ctx->builder, ctx->type_i1, derived_ptr, llvm_tmp_name(ctx))
                    : LLVMBuildLoad2(ctx->builder, ctx->type_i1, derived_dirty_addr,
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
        }

        /* prev_active_addrs is ctx->scratch-owned. */
    }

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
