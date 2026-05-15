#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"
#include "llvm_domain_zone_sync_internal.h"
#include "parser/ast_api.h"

static bool
llvm_zone_sync_clause_field_name(char *out,
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
llvm_zone_sync_emit_action_causes(ASTNode *stmt,
                                  LLVMClassTypeEntry *decl_cls,
                                  LLVMValueRef sync_fn,
                                  LLVMGenCtx *ctx)
{
    if (stmt == NULL || decl_cls == NULL || sync_fn == NULL || ctx == NULL)
        return;

    size_t layer_slot_count = 0;
    ASTNode **layer_slots = ast_zone_layer_slots(stmt, &layer_slot_count);
    size_t state_count = 0;
    ASTNode **states = ast_zone_states(stmt, &state_count);

    for (size_t i = 0; i < layer_slot_count; i++) {
        ASTNode *slot = layer_slots[i];
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
            || ast_zone_layer_slot_is_relation(slot)
            || ast_zone_layer_slot_name(slot) == NULL) {
            continue;
        }

        layer_name = ast_zone_layer_slot_name(slot);
        if (!llvm_zone_sync_clause_field_name(cause_field, sizeof(cause_field),
                "layer_cause", layer_name))
            continue;
        if (!llvm_zone_sync_clause_field_name(active_field, sizeof(active_field),
                "layer_active", layer_name))
            continue;
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
        action_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn,
            "zone.action.cause");
        next_bb = LLVMAppendBasicBlockInContext(ctx->context, sync_fn,
            "zone.action.next");
        LLVMBuildCondBr(ctx->builder, is_action, action_bb, next_bb);
        LLVMPositionBuilderAtEnd(ctx->builder, action_bb);
        {
            LLVMValueRef active_ptr = LLVMBuildStructGEP2(ctx->builder,
                decl_cls->struct_type, self_ptr, (unsigned)active_idx,
                llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0), active_ptr);
        }
        for (size_t j = 0; j < state_count; j++) {
            ASTNode *state = states[j];
            char state_field[256];
            int state_idx;
            LLVMValueRef state_ptr;
            if (state == NULL || state->type != AST_ZONE_STATE
                || ast_zone_state_is_relation(state)
                || ast_zone_state_name(state) == NULL
                || ast_zone_state_layer_slot_name(state) == NULL
                || strcmp(ast_zone_state_layer_slot_name(state), layer_name) != 0) {
                continue;
            }
            if (!llvm_zone_sync_clause_field_name(state_field, sizeof(state_field),
                    "state", ast_zone_state_name(state)))
                continue;
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
}

void
llvm_zone_sync_emit_detach_clauses(ASTNode *stmt,
                                   LLVMClassTypeEntry *decl_cls,
                                   LLVMValueRef sync_fn,
                                   LLVMGenCtx *ctx)
{
    if (stmt == NULL || decl_cls == NULL || sync_fn == NULL || ctx == NULL)
        return;

    size_t detach_count = 0;
    ASTNode **detaches = ast_zone_detaches(stmt, &detach_count);
    size_t state_count = 0;
    ASTNode **states = ast_zone_states(stmt, &state_count);

    for (size_t i = 0; i < detach_count; i++) {
        ASTNode *detach = detaches[i];
        const char *state_name = detach != NULL ? ast_zone_directive_state_name(detach) : NULL;
        if (state_name == NULL && detach != NULL) {
            for (size_t j = 0; j < state_count; j++) {
                ASTNode *state = states[j];
                if (state != NULL && state->type == AST_ZONE_STATE
                    && !ast_zone_state_is_relation(state)
                    && ast_zone_state_layer_slot_name(state) != NULL
                    && ast_zone_state_left_or_target_slot_name(state) != NULL
                    && ast_zone_effect_slot_name(detach) != NULL
                    && ast_zone_effect_target_slot_name(detach) != NULL
                    && strcmp(ast_zone_state_layer_slot_name(state),
                              ast_zone_effect_slot_name(detach)) == 0
                    && strcmp(ast_zone_state_left_or_target_slot_name(state),
                              ast_zone_effect_target_slot_name(detach)) == 0) {
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
            if (!llvm_zone_sync_clause_field_name(field_name, sizeof(field_name),
                    "state", state_name))
                continue;
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
                const char *layer_name = ast_zone_effect_slot_name(detach);
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
                    if (!llvm_zone_sync_clause_field_name(layer_field,
                            sizeof(layer_field), "layer_active", layer_name))
                        continue;
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
        } else if (detach != NULL && ast_zone_effect_slot_name(detach) != NULL) {
            char layer_field[256];
            int layer_idx;
            LLVMValueRef self_ptr = LLVMGetParam(sync_fn, 0);
            LLVMValueRef layer_ptr;
            if (!llvm_zone_sync_clause_field_name(layer_field, sizeof(layer_field),
                    "layer_active", ast_zone_effect_slot_name(detach)))
                continue;
            layer_idx = llvm_class_field_index(decl_cls, layer_field);
            if (layer_idx >= 0) {
                layer_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
                    self_ptr, (unsigned)layer_idx, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder,
                    LLVMConstInt(ctx->type_i1, 0, 0), layer_ptr);
                llvm_stamp_domain_provenance(ctx, decl_cls, self_ptr, "layer",
                    ast_zone_effect_slot_name(detach),
                    PGY_PROP_CAUSE_DETACH);
            }
        }
    }
}

#endif /* PGY_LLVM_ENABLED */
