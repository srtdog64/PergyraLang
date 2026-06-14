#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"
#include "llvm_domain_decl_parts_helpers.h"
#include "domain_frontier_policy.h"
#include "domain_frontier_graph.h"
#include "llvm_domain_zone_bind_lowering.h"
#include "llvm_domain_sync_frontier.h"
#include "llvm_domain_projection_value_helpers.h"
#include "llvm_domain_projection_sync_body_helpers.h"
#include "llvm_domain_zone_sync_internal.h"
#include "llvm_internal_api.h"
#include "llvm_inventory_decl_lookup.h"
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
    LLVMTypeRef saved_function_ret;
    const char *saved_return_type_name;
    ASTNode *saved_return_callable_type;
    ASTNode *saved_host_decl;
    LLVMBasicBlockRef saved_bb;
    LLVMLexicalRegistrySnapshot lexical_snapshot;
    LLVMBasicBlockRef bb;
    size_t state_count = 0;
    ASTNode **states = NULL;
    LLVMHostedZoneLayerSlotView layer_view;

    if (stmt == NULL || stmt->type != AST_ZONE_DECL || decl_name == NULL
        || decl_cls == NULL || sync_fn == NULL || ctx == NULL)
        return;

    saved_fn = ctx->current_function;
    saved_ret = ctx->current_ret_type;
    saved_function_ret = ctx->current_function_ret_type;
    saved_return_type_name = ctx->current_return_type_name;
    saved_return_callable_type = ctx->current_return_callable_type;
    saved_bb = LLVMGetInsertBlock(ctx->builder);
    lexical_snapshot = llvm_lexical_registry_snapshot(ctx);
    saved_host_decl = llvm_bind_current_host_decl(ctx, stmt);
    bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn, "entry");
    LLVMPositionBuilderAtEnd(ctx->builder, bb);
    ctx->current_function = sync_fn;
    ctx->current_ret_type = ctx->type_void;
    ctx->current_function_ret_type = ctx->type_void;
    ctx->current_return_type_name = NULL;
    ctx->current_return_callable_type = NULL;

    llvm_scope_push(ctx);
    {
        LLVMTypeRef self_ptr_t = LLVMPointerType(decl_cls->struct_type, 0);
        LLVMValueRef sa = llvm_create_entry_alloca(ctx, self_ptr_t, "self.addr");
        LLVMBuildStore(ctx->builder, LLVMGetParam(sync_fn, 0), sa);
        llvm_scope_declare(ctx, "self", sa, self_ptr_t);
        llvm_register_var_class(ctx, "self", decl_name);
    }
    states = ast_zone_states(stmt, &state_count);
    layer_view = llvm_hosted_zone_layer_slot_view_from_decl(ctx, decl_name, stmt);
    if (llvm_hosted_zone_layer_slot_view_missing_mir_metadata(&layer_view)) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing zone frontier layer-slot metadata for '%s'",
            decl_name);
        LLVMBuildRetVoid(ctx->builder);
        llvm_scope_pop(ctx);
        llvm_lexical_registry_restore(ctx, lexical_snapshot);
        llvm_finish_domain_sync_emit(ctx, saved_fn, saved_ret,
            saved_function_ret, saved_return_type_name,
            saved_return_callable_type,
            saved_host_decl, saved_bb);
        return;
    }
    llvm_emit_sync_generation_increment(ctx, decl_cls, LLVMGetParam(sync_fn, 0));
    LLVMValueRef frontier_pass_addr = llvm_create_entry_alloca(ctx, ctx->type_i32,
        "zone.frontier.pass.addr");
    LLVMValueRef frontier_continue_addr = llvm_create_entry_alloca(ctx, ctx->type_i1,
        "zone.frontier.continue.addr");
    LLVMValueRef frontier_limit_val = LLVMConstInt(ctx->type_i32,
        (unsigned long long)pgy_codegen_zone_frontier_graph_pass_limit(stmt,
            decl_name,
            pgy_domain_zone_frontier_pass_limit_from_counts(
                state_count, layer_view.count)), 0);
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

    size_t apply_count = 0;
    ASTNode **applies = ast_zone_applies(stmt, &apply_count);
    for (size_t i = 0; i < apply_count; i++) {
        ASTNode *apply = applies[i];
        const char *state_name = apply != NULL ? ast_zone_directive_state_name(apply) : NULL;
        if (state_name == NULL && apply != NULL) {
            for (size_t j = 0; j < state_count; j++) {
                ASTNode *state = states[j];
                if (state != NULL && state->type == AST_ZONE_STATE
                    && !ast_zone_state_is_relation(state)
                    && ast_zone_state_layer_slot_name(state) != NULL
                    && ast_zone_state_left_or_target_slot_name(state) != NULL
                    && ast_zone_effect_slot_name(apply) != NULL
                    && ast_zone_effect_target_slot_name(apply) != NULL
                    && strcmp(ast_zone_state_layer_slot_name(state),
                              ast_zone_effect_slot_name(apply)) == 0
                    && strcmp(ast_zone_state_left_or_target_slot_name(state),
                              ast_zone_effect_target_slot_name(apply)) == 0) {
                    state_name = ast_zone_state_name(state);
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
                const char *layer_name = ast_zone_effect_slot_name(apply);
                if (layer_name == NULL) {
                    for (size_t j = 0; j < state_count; j++) {
                        ASTNode *state = states[j];
                        if (state != NULL && state->type == AST_ZONE_STATE
                            && !ast_zone_state_is_relation(state)
                            && ast_zone_state_name(state) != NULL
                            && strcmp(ast_zone_state_name(state), state_name) == 0) {
                            layer_name = ast_zone_state_layer_slot_name(state);
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
                    if (ast_zone_effect_target_slot_name(apply) != NULL) {
                        llvm_zone_bind_effect_layer(stmt, decl_cls, sync_fn, ctx,
                            layer_name, ast_zone_effect_target_slot_name(apply));
                    } else {
                        for (size_t j = 0; j < state_count; j++) {
                            ASTNode *state = states[j];
                            if (state != NULL && state->type == AST_ZONE_STATE
                                && !ast_zone_state_is_relation(state)
                                && ast_zone_state_name(state) != NULL
                                && strcmp(ast_zone_state_name(state), state_name) == 0
                                && ast_zone_state_left_or_target_slot_name(state) != NULL) {
                                llvm_zone_bind_effect_layer(stmt, decl_cls, sync_fn, ctx,
                                    layer_name,
                                    ast_zone_state_left_or_target_slot_name(state));
                                break;
                            }
                        }
                    }
                }
            }
        } else if (apply != NULL
                   && ast_zone_effect_slot_name(apply) != NULL
                   && ast_zone_effect_target_slot_name(apply) != NULL) {
            char layer_field[256];
            int layer_idx;
            LLVMValueRef self_ptr = LLVMGetParam(sync_fn, 0);
            LLVMValueRef layer_ptr;
            if (!llvm_zone_sync_field_name(layer_field, sizeof(layer_field),
                    "layer_active", ast_zone_effect_slot_name(apply)))
                continue;
            layer_idx = llvm_class_field_index(decl_cls, layer_field);
            if (layer_idx >= 0) {
                layer_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                    self_ptr, (unsigned)layer_idx, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder,
                    LLVMConstInt(ctx->type_i1, 1, 0), layer_ptr);
                llvm_stamp_domain_provenance(ctx, decl_cls, self_ptr, "layer",
                    ast_zone_effect_slot_name(apply),
                    PGY_PROP_CAUSE_APPLY);
            }
            llvm_zone_bind_effect_layer(stmt, decl_cls, sync_fn, ctx,
                ast_zone_effect_slot_name(apply),
                ast_zone_effect_target_slot_name(apply));
        }
    }

    size_t maintained_effect_count = 0;
    ASTNode **maintained_effects =
        ast_zone_maintained_effects(stmt, &maintained_effect_count);
    for (size_t i = 0; i < maintained_effect_count; i++) {
        ASTNode *maintain = maintained_effects[i];
        if (maintain == NULL
            || ast_zone_effect_slot_name(maintain) == NULL
            || ast_zone_effect_target_slot_name(maintain) == NULL) {
            continue;
        }
        {
            char layer_field[256];
            int layer_idx;
            LLVMValueRef self_ptr = LLVMGetParam(sync_fn, 0);
            LLVMValueRef layer_ptr;
            if (!llvm_zone_sync_field_name(layer_field, sizeof(layer_field),
                    "layer_active",
                    ast_zone_effect_slot_name(maintain)))
                continue;
            layer_idx = llvm_class_field_index(decl_cls, layer_field);
            if (layer_idx >= 0) {
                layer_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                    self_ptr, (unsigned)layer_idx, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder,
                    LLVMConstInt(ctx->type_i1, 1, 0), layer_ptr);
                llvm_stamp_domain_provenance(ctx, decl_cls, self_ptr, "layer",
                    ast_zone_effect_slot_name(maintain),
                    PGY_PROP_CAUSE_MAINTAIN);
            }
            if (ast_zone_relation_left_slot_name(maintain) != NULL
                && ast_zone_relation_right_slot_name(maintain) != NULL) {
                llvm_zone_bind_relation_layer(stmt, decl_cls, sync_fn, ctx,
                    ast_zone_relation_slot_name(maintain),
                    ast_zone_relation_left_slot_name(maintain),
                    ast_zone_relation_right_slot_name(maintain));
            }
        }
        llvm_zone_bind_effect_layer(stmt, decl_cls, sync_fn, ctx,
            ast_zone_effect_slot_name(maintain),
            ast_zone_effect_target_slot_name(maintain));
        for (size_t j = 0; j < state_count; j++) {
            ASTNode *state = states[j];
            const char *state_name;
            char field_name[256];
            int field_idx;
            LLVMValueRef self_ptr;
            LLVMValueRef state_ptr;
            if (state == NULL || state->type != AST_ZONE_STATE
                || ast_zone_state_is_relation(state)
                || ast_zone_state_layer_slot_name(state) == NULL
                || ast_zone_state_left_or_target_slot_name(state) == NULL
                || strcmp(ast_zone_state_layer_slot_name(state),
                          ast_zone_effect_slot_name(maintain)) != 0
                || strcmp(ast_zone_state_left_or_target_slot_name(state),
                          ast_zone_effect_target_slot_name(maintain)) != 0) {
                continue;
            }
            state_name = ast_zone_state_name(state);
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
        if (maintain != NULL && ast_zone_directive_state_name(maintain) != NULL) {
            char field_name[256];
            int field_idx;
            LLVMValueRef self_ptr;
            LLVMValueRef state_ptr;
            if (!llvm_zone_sync_field_name(field_name, sizeof(field_name),
                    "state", ast_zone_directive_state_name(maintain)))
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
                ast_zone_directive_state_name(maintain),
                PGY_PROP_CAUSE_MAINTAIN);
            for (size_t j = 0; j < state_count; j++) {
                ASTNode *state = states[j];
                if (state != NULL && state->type == AST_ZONE_STATE
                    && ast_zone_state_name(state) != NULL
                    && strcmp(ast_zone_state_name(state),
                              ast_zone_directive_state_name(maintain)) == 0) {
                    char layer_field[256];
                    int layer_idx;
                    LLVMValueRef layer_ptr;
                    if (!llvm_zone_sync_field_name(layer_field,
                            sizeof(layer_field), "layer_active",
                            ast_zone_state_layer_slot_name(state)))
                        continue;
                    layer_idx = llvm_class_field_index(decl_cls, layer_field);
                    if (layer_idx >= 0) {
                        layer_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                            self_ptr, (unsigned)layer_idx, llvm_tmp_name(ctx));
                        LLVMBuildStore(ctx->builder,
                            LLVMConstInt(ctx->type_i1, 1, 0), layer_ptr);
                        llvm_stamp_domain_provenance(ctx, decl_cls, self_ptr,
                            "layer", ast_zone_state_layer_slot_name(state),
                            PGY_PROP_CAUSE_MAINTAIN);
                    }
                    if (!ast_zone_state_is_relation(state)) {
                        llvm_zone_bind_effect_layer(stmt, decl_cls, sync_fn, ctx,
                            ast_zone_state_layer_slot_name(state),
                            ast_zone_state_left_or_target_slot_name(state));
                    } else if (ast_zone_state_right_slot_name(state) != NULL) {
                        llvm_zone_bind_relation_layer(stmt, decl_cls, sync_fn, ctx,
                            ast_zone_state_layer_slot_name(state),
                            ast_zone_state_left_or_target_slot_name(state),
                            ast_zone_state_right_slot_name(state));
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
    llvm_lexical_registry_restore(ctx, lexical_snapshot);
    llvm_finish_domain_sync_emit(ctx, saved_fn, saved_ret,
        saved_function_ret, saved_return_type_name,
        saved_return_callable_type,
        saved_host_decl, saved_bb);
}

#endif /* PGY_LLVM_ENABLED */
