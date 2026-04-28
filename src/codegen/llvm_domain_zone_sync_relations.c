#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"
#include "llvm_domain_zone_bind_helpers.h"
#include "llvm_domain_zone_sync_internal.h"

void
llvm_zone_sync_emit_relation_clauses(ASTNode *stmt,
                                     LLVMClassTypeEntry *decl_cls,
                                     LLVMValueRef sync_fn,
                                     LLVMGenCtx *ctx)
{
    (void)llvm_zone_bind_effect_layer;

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
}

#endif /* PGY_LLVM_ENABLED */
