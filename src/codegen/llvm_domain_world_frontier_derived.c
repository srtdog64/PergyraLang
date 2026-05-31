#ifdef PGY_LLVM_ENABLED
#include "llvm_domain_world_frontier_internal.h"
#include "llvm_domain_world_sync_internal.h"

void
llvm_world_frontier_emit_derived_state_pass(ASTNode *stmt,
                                            LLVMClassTypeEntry *decl_cls,
                                            LLVMValueRef sync_fn,
                                            ASTNode **states,
                                            size_t state_count,
                                            LLVMValueRef continue_addr,
                                            LLVMValueRef changed_any_addr,
                                            LLVMBasicBlockRef loop_check_bb,
                                            LLVMGenCtx *ctx)
{
    for (size_t i = 0; i < state_count; i++) {
        ASTNode *state = states[i];
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
            || ast_world_state_name(state) == NULL)
            continue;
        slot_name = ast_world_state_zone_slot_name(state);
        if (!llvm_world_frontier_field_name(state_field, sizeof(state_field),
                "zone_state", ast_world_state_name(state)))
            continue;
        state_idx = llvm_class_field_index(decl_cls, state_field);
        if (state_idx < 0)
            continue;
        self_ptr = LLVMGetParam(sync_fn, 0);
        state_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
            self_ptr, (unsigned)state_idx, llvm_tmp_name(ctx));
        prev_state_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
            state_ptr, llvm_tmp_name(ctx));
        if (slot_name != NULL) {
            if (!llvm_world_frontier_field_name(active_field,
                    sizeof(active_field), "zone_active", slot_name))
                continue;
            active_idx = llvm_class_field_index(decl_cls, active_field);
            if (active_idx >= 0) {
                active_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                    self_ptr, (unsigned)active_idx, llvm_tmp_name(ctx));
                active_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
                    active_ptr, llvm_tmp_name(ctx));
            }
        }
        derived_val = active_val;

        if (ast_world_state_source_kind(state) == WORLD_STATE_SOURCE_ALL
            || ast_world_state_source_kind(state) == WORLD_STATE_SOURCE_ANY) {
            derived_val = LLVMConstInt(ctx->type_i1,
                ast_world_state_source_kind(state) == WORLD_STATE_SOURCE_ALL ? 1 : 0, 0);
            for (size_t input_i = 0; input_i < ast_world_state_input_count(state); input_i++) {
                const char *input_name = ast_world_state_input_name(state, input_i);
                int input_idx = -1;
                LLVMValueRef input_ptr;
                LLVMValueRef input_val;
                if (input_name == NULL)
                    continue;
                if (llvm_world_sync_has_zone_slot(ctx, stmt, input_name)) {
                    char input_field[256];
                    if (!llvm_world_frontier_field_name(input_field,
                            sizeof(input_field), "zone_active", input_name))
                        continue;
                    input_idx = llvm_class_field_index(decl_cls, input_field);
                } else {
                    char input_field[256];
                    if (!llvm_world_frontier_field_name(input_field,
                            sizeof(input_field), "zone_state", input_name))
                        continue;
                    input_idx = llvm_class_field_index(decl_cls, input_field);
                }
                if (input_idx < 0)
                    continue;
                input_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                    self_ptr, (unsigned)input_idx, llvm_tmp_name(ctx));
                input_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
                    input_ptr, llvm_tmp_name(ctx));
                if (ast_world_state_source_kind(state) == WORLD_STATE_SOURCE_ALL)
                    derived_val = LLVMBuildAnd(ctx->builder, derived_val, input_val,
                        llvm_tmp_name(ctx));
                else
                    derived_val = LLVMBuildOr(ctx->builder, derived_val, input_val,
                        llvm_tmp_name(ctx));
            }
        }

        if (ast_world_state_source_kind(state) != WORLD_STATE_SOURCE_ZONE
            && ast_world_state_source_kind(state) != WORLD_STATE_SOURCE_ALL
            && ast_world_state_source_kind(state) != WORLD_STATE_SOURCE_ANY
            && ast_world_state_detail_name(state) != NULL) {
            int zone_idx = llvm_class_field_index(decl_cls, slot_name);
            LLVMClassTypeEntry *zone_cls = NULL;
            if (zone_idx >= 0) {
                LLVMTypeRef zone_field_ty =
                    llvm_class_field_type_at_index(decl_cls, zone_idx);
                zone_cls = llvm_lookup_class_by_struct_type(ctx, zone_field_ty);
            }
            if (zone_cls != NULL && zone_idx >= 0) {
                char detail_field[256];
                int detail_idx = -1;
                LLVMValueRef zone_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                    self_ptr, (unsigned)zone_idx, llvm_tmp_name(ctx));
                LLVMValueRef detail_ptr;
                LLVMValueRef detail_val;

                switch (ast_world_state_source_kind(state)) {
                case WORLD_STATE_SOURCE_PROJECTION:
                    if (!llvm_world_frontier_field_name(detail_field,
                            sizeof(detail_field), "projection_ready",
                            ast_world_state_detail_name(state)))
                        detail_field[0] = '\0';
                    break;
                case WORLD_STATE_SOURCE_LAYER:
                    if (!llvm_world_frontier_field_name(detail_field,
                            sizeof(detail_field), "layer_active",
                            ast_world_state_detail_name(state)))
                        detail_field[0] = '\0';
                    break;
                case WORLD_STATE_SOURCE_STATE:
                    if (!llvm_world_frontier_field_name(detail_field,
                            sizeof(detail_field), "state",
                            ast_world_state_detail_name(state)))
                        detail_field[0] = '\0';
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
            ast_world_state_name(state), PGY_PROP_CAUSE_WORLD_DERIVED);
    }
    LLVMBuildBr(ctx->builder, loop_check_bb);
}

#endif /* PGY_LLVM_ENABLED */
