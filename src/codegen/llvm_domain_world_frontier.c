#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"
#include "llvm_domain_sync_frontier.h"
#include "llvm_domain_world_frontier_internal.h"
#include "llvm_domain_world_sync_internal.h"
#include "llvm_inventory_decl_lookup.h"
#include "../compiler/mir_decl_headers.h"
#include "domain_frontier_policy.h"
#include "domain_frontier_graph.h"

bool
llvm_world_frontier_field_name(char *out,
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

bool
llvm_world_frontier_sync_name(char *out,
                              size_t out_size,
                              const char *zone_type)
{
    int written;

    if (out == NULL || out_size == 0 || zone_type == NULL)
        return false;
    written = snprintf(out, out_size, "%s_sync", zone_type);
    return written >= 0 && (size_t)written < out_size;
}

static size_t
llvm_world_frontier_zone_member_count(void *ctx, const char *zone_name)
{
    LLVMGenCtx *llvm_ctx = (LLVMGenCtx *)ctx;
    ASTNode *zone_decl;
    size_t state_count;
    LLVMHostedZoneStateView state_view;
    LLVMHostedZoneLayerSlotView layer_view;

    if (llvm_ctx == NULL || zone_name == NULL)
        return 0;

    zone_decl = NULL;
    if (!llvm_active_has_mir(llvm_ctx)) {
        zone_decl = llvm_find_decl_in_active_inventory(
            llvm_ctx, AST_ZONE_DECL, zone_name);
        if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL)
            return 0;
    }

    state_view = llvm_hosted_zone_state_view_from_decl(
        llvm_ctx, zone_name, zone_decl);
    if ((llvm_active_has_mir(llvm_ctx) && !state_view.uses_mir_metadata)
        || llvm_hosted_zone_state_view_missing_mir_metadata(&state_view)
        || !llvm_hosted_zone_state_view_rows_complete(&state_view)) {
        llvm_set_mir_inventory_missing(llvm_ctx,
            "MIR-only LLVM path missing embedded zone state metadata for world frontier '%s'",
            zone_name);
        return 0;
    }
    state_count = state_view.count;
    layer_view = llvm_hosted_zone_layer_slot_view_from_decl(
        llvm_ctx, zone_name, zone_decl);
    if ((llvm_active_has_mir(llvm_ctx) && !layer_view.uses_mir_metadata)
        || llvm_hosted_zone_layer_slot_view_missing_mir_metadata(&layer_view)) {
        llvm_set_mir_inventory_missing(llvm_ctx,
            "MIR-only LLVM path missing embedded zone layer-slot metadata for world frontier '%s'",
            zone_name);
        return 0;
    }

    return pgy_frontier_embedded_zone_member_count(
        state_count, layer_view.count);
}

static const char *
llvm_world_frontier_zone_type_name(void *ctx, size_t index)
{
    return llvm_hosted_world_zone_slot_view_type_name(
        (const LLVMHostedWorldZoneSlotView *)ctx,
        index);
}

void
llvm_world_sync_emit_frontier(const MIRDeclHeader *header,
                              ASTNode *stmt, LLVMClassTypeEntry *decl_cls,
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
    size_t zone_count = 0;
    size_t state_count = 0;
    ASTNode **states = NULL;
    size_t embedded_frontier_count;
    LLVMHostedWorldZoneSlotView zone_view;
    const char *world_name;

    if (stmt == NULL || stmt->type != AST_WORLD_DECL || decl_cls == NULL
        || sync_fn == NULL || derived_dirty_addr == NULL
        || needs_derived_addr == NULL || ctx == NULL)
        return;

    world_name = llvm_decl_node_name(stmt);
    zone_view = llvm_hosted_world_zone_slot_view_from_decl(ctx,
        world_name, stmt);
    if (llvm_hosted_world_zone_slot_view_missing_mir_metadata(&zone_view)) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing world zone-slot metadata for '%s'",
            world_name != NULL ? world_name : "<anonymous>");
        return;
    }
    zone_count = zone_view.count;
    if (llvm_active_has_mir(ctx)) {
        if (header == NULL) {
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path missing declaration header for world state frontier '%s'",
                world_name != NULL ? world_name : "<anonymous-world>");
            return;
        }
        state_count = mir_decl_header_world_state_count(header);
        if (state_count != mir_decl_header_world_state_declared_count(header)) {
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path world '%s' has inconsistent world-state metadata count",
                world_name != NULL ? world_name : "<anonymous-world>");
            return;
        }
    } else {
        states = ast_world_states(stmt, &state_count);
    }
    embedded_frontier_count =
        pgy_domain_world_embedded_frontier_count_from_zone_types(
            zone_view.count,
            llvm_world_frontier_zone_type_name,
            &zone_view,
            llvm_world_frontier_zone_member_count,
            ctx);
    if (ctx->has_error)
        return;
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
        (unsigned long long)
            pgy_codegen_world_frontier_graph_pass_limit(stmt,
                llvm_decl_node_name(stmt),
                pgy_domain_world_transitive_frontier_pass_limit_from_counts(
                    zone_count, state_count, embedded_frontier_count)),
        0);
    limit_val = LLVMConstInt(ctx->type_i32,
        (unsigned long long)
            pgy_domain_world_derived_frontier_pass_limit_from_count(state_count),
        0);
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

    llvm_world_frontier_emit_zone_sync_pass(stmt, decl_cls, sync_fn,
        needs_derived_addr, derived_ptr, &zone_view, ctx);

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
    llvm_world_frontier_emit_derived_state_pass(header, stmt, decl_cls, sync_fn,
        states, state_count, continue_addr, changed_any_addr, loop_check_bb, ctx);

    LLVMPositionBuilderAtEnd(ctx->builder, done_bb);
    {
        LLVMValueRef continue_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
            continue_addr, llvm_tmp_name(ctx));
        LLVMBuildCondBr(ctx->builder, continue_val, overflow_bb, finalize_bb);
    }

    LLVMPositionBuilderAtEnd(ctx->builder, overflow_bb);
    llvm_emit_frontier_overflow_abort(ctx,
        PGY_FRONTIER_REASON_WORLD_DERIVED_OVERFLOW);

    LLVMPositionBuilderAtEnd(ctx->builder, finalize_bb);
    LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0), derived_dirty_addr);
    if (derived_ptr != NULL)
        LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0), derived_ptr);
    LLVMBuildBr(ctx->builder, derived_exit_bb);
    LLVMPositionBuilderAtEnd(ctx->builder, derived_exit_bb);
    llvm_world_frontier_emit_pending_zone_dirty(decl_cls, sync_fn,
        derived_dirty_addr, derived_ptr, frontier_continue_addr,
        changed_any_addr, &zone_view, ctx);
    LLVMBuildBr(ctx->builder, frontier_check_bb);

    LLVMPositionBuilderAtEnd(ctx->builder, frontier_done_bb);
    {
        LLVMValueRef continue_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
            frontier_continue_addr, llvm_tmp_name(ctx));
        LLVMBuildCondBr(ctx->builder, continue_val, frontier_overflow_bb, frontier_exit_bb);
    }

    LLVMPositionBuilderAtEnd(ctx->builder, frontier_overflow_bb);
    llvm_emit_frontier_overflow_abort(ctx,
        PGY_FRONTIER_REASON_WORLD_TRANSITIVE_OVERFLOW);

    LLVMPositionBuilderAtEnd(ctx->builder, frontier_exit_bb);
}

#endif /* PGY_LLVM_ENABLED */
