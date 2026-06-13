/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM backend intent effect provenance helpers.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_intent_internal.h"
#include "llvm_inventory_decl_lookup.h"

static ASTNode *
llvm_find_zone_decl_by_name(LLVMGenCtx *ctx, const char *zone_type_name)
{
    if (ctx == NULL || zone_type_name == NULL)
        return NULL;

    return llvm_find_decl_in_active_inventory(ctx, AST_ZONE_DECL,
                                             zone_type_name);
}

static bool
llvm_intent_effect_field_name(char *out,
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
llvm_emit_intent_step_mark_caused_effect(LLVMGenCtx *ctx,
                                         const char *zone_type_name,
                                         const char *zone_alias,
                                         const char *causes_effect)
{
    ASTNode *zone_decl;
    const char *zone_name;
    LLVMClassTypeEntry *zone_cls;
    LLVMVarEntry zone_var;
    bool has_zone_var;
    LLVMValueRef zone_ptr;
    LLVMHostedZoneLayerSlotView layer_view;
    size_t state_count = 0;
    ASTNode **states = NULL;

    if (ctx == NULL || zone_type_name == NULL || zone_alias == NULL
        || causes_effect == NULL) {
        return;
    }

    zone_decl = llvm_find_zone_decl_by_name(ctx, zone_type_name);
    zone_name = llvm_decl_node_name(zone_decl);
    zone_cls = llvm_lookup_class(ctx, zone_type_name);
    has_zone_var = llvm_scope_lookup_snapshot(ctx, zone_alias, &zone_var);
    if (zone_decl == NULL || zone_name == NULL || zone_cls == NULL
        || !has_zone_var
        || LLVMGetTypeKind(zone_var.type) != LLVMPointerTypeKind) {
        return;
    }

    zone_ptr = LLVMBuildLoad2(ctx->builder, zone_var.type,
        zone_var.alloca, llvm_tmp_name(ctx));

    layer_view = llvm_hosted_zone_layer_slot_view_from_decl(ctx, zone_name, zone_decl);
    if (llvm_hosted_zone_layer_slot_view_missing_mir_metadata(&layer_view)) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing zone layer-slot metadata for intent effect emission");
        return;
    }
    states = ast_zone_states(zone_decl, &state_count);

    for (size_t i = 0; i < layer_view.count; i++) {
        const char *layer_name;
        const char *layer_type;
        char epoch_field[256];
        char cause_field[256];
        int epoch_idx;
        int cause_idx;

        layer_name = llvm_hosted_zone_layer_slot_view_name(&layer_view, i);
        layer_type = llvm_hosted_zone_layer_slot_view_type_name(&layer_view, i);
        if (llvm_hosted_zone_layer_slot_view_is_relation(&layer_view, i)
            || layer_name == NULL
            || layer_type == NULL
            || strcmp(layer_type, causes_effect) != 0) {
            continue;
        }

        if (!llvm_intent_effect_field_name(epoch_field, sizeof(epoch_field),
                "layer_epoch", layer_name)) {
            llvm_set_error(ctx, "intent effect layer epoch field name is too long");
            return;
        }
        if (!llvm_intent_effect_field_name(cause_field, sizeof(cause_field),
                "layer_cause", layer_name)) {
            llvm_set_error(ctx, "intent effect layer cause field name is too long");
            return;
        }
        epoch_idx = llvm_class_field_index(zone_cls, epoch_field);
        cause_idx = llvm_class_field_index(zone_cls, cause_field);
        if (epoch_idx >= 0) {
            LLVMValueRef epoch_ptr = LLVMBuildStructGEP2(ctx->builder,
                zone_cls->struct_type, zone_ptr, (unsigned)epoch_idx,
                llvm_tmp_name(ctx));
            LLVMValueRef epoch_val = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
                epoch_ptr, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder,
                LLVMBuildAdd(ctx->builder, epoch_val,
                    LLVMConstInt(ctx->type_i32, 1, 0), llvm_tmp_name(ctx)),
                epoch_ptr);
        }
        if (cause_idx >= 0) {
            LLVMValueRef cause_ptr = LLVMBuildStructGEP2(ctx->builder,
                zone_cls->struct_type, zone_ptr, (unsigned)cause_idx,
                llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i32, 11, 0), cause_ptr);
        }

        for (size_t j = 0; j < state_count; j++) {
            ASTNode *state = states[j];
            char state_epoch_field[256];
            char state_cause_field[256];
            int state_epoch_idx;
            int state_cause_idx;
            if (state == NULL || state->type != AST_ZONE_STATE
                || ast_zone_state_is_relation(state)
                || ast_zone_state_name(state) == NULL
                || ast_zone_state_layer_slot_name(state) == NULL
                || strcmp(ast_zone_state_layer_slot_name(state), layer_name) != 0) {
                continue;
            }
            if (!llvm_intent_effect_field_name(state_epoch_field,
                    sizeof(state_epoch_field), "state_epoch",
                    ast_zone_state_name(state))) {
                llvm_set_error(ctx,
                    "intent effect state epoch field name is too long");
                return;
            }
            if (!llvm_intent_effect_field_name(state_cause_field,
                    sizeof(state_cause_field), "state_cause",
                    ast_zone_state_name(state))) {
                llvm_set_error(ctx,
                    "intent effect state cause field name is too long");
                return;
            }
            state_epoch_idx = llvm_class_field_index(zone_cls, state_epoch_field);
            state_cause_idx = llvm_class_field_index(zone_cls, state_cause_field);
            if (state_epoch_idx >= 0) {
                LLVMValueRef state_epoch_ptr = LLVMBuildStructGEP2(ctx->builder,
                    zone_cls->struct_type, zone_ptr, (unsigned)state_epoch_idx,
                    llvm_tmp_name(ctx));
                LLVMValueRef state_epoch_val = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
                    state_epoch_ptr, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder,
                    LLVMBuildAdd(ctx->builder, state_epoch_val,
                        LLVMConstInt(ctx->type_i32, 1, 0), llvm_tmp_name(ctx)),
                    state_epoch_ptr);
            }
            if (state_cause_idx >= 0) {
                LLVMValueRef state_cause_ptr = LLVMBuildStructGEP2(ctx->builder,
                    zone_cls->struct_type, zone_ptr, (unsigned)state_cause_idx,
                    llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i32, 11, 0), state_cause_ptr);
            }
        }
    }
}

#endif
