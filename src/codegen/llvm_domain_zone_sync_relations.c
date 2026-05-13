#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"
#include "llvm_domain_zone_bind_helpers.h"
#include "llvm_domain_zone_sync_internal.h"
#include "parser/ast_api.h"

static bool
llvm_zone_relation_sync_field_name(char *out,
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
llvm_zone_sync_emit_relation_clauses(ASTNode *stmt,
                                     LLVMClassTypeEntry *decl_cls,
                                     LLVMValueRef sync_fn,
                                     LLVMGenCtx *ctx)
{
    size_t state_count = 0;
    ASTNode **states = ast_zone_states(stmt, &state_count);
    size_t link_count = 0;
    ASTNode **links = ast_zone_links(stmt, &link_count);
    size_t maintained_relation_count = 0;
    ASTNode **maintained_relations =
        ast_zone_maintained_relations(stmt, &maintained_relation_count);
    size_t unlink_count = 0;
    ASTNode **unlinks = ast_zone_unlinks(stmt, &unlink_count);

    for (size_t i = 0; i < link_count; i++) {
        ASTNode *link = links[i];
        const char *state_name = link != NULL ? link->data.zone_link.state_name : NULL;
        if (state_name == NULL && link != NULL) {
            for (size_t j = 0; j < state_count; j++) {
                ASTNode *state = states[j];
                if (state != NULL && state->type == AST_ZONE_STATE
                    && ast_zone_state_is_relation(state)
                    && ast_zone_state_layer_slot_name(state) != NULL
                    && ast_zone_state_left_or_target_slot_name(state) != NULL
                    && ast_zone_state_right_slot_name(state) != NULL
                    && link->data.zone_link.relation_slot_name != NULL
                    && link->data.zone_link.left_slot_name != NULL
                    && link->data.zone_link.right_slot_name != NULL
                    && strcmp(ast_zone_state_layer_slot_name(state),
                              link->data.zone_link.relation_slot_name) == 0
                    && strcmp(ast_zone_state_left_or_target_slot_name(state),
                              link->data.zone_link.left_slot_name) == 0
                    && strcmp(ast_zone_state_right_slot_name(state),
                              link->data.zone_link.right_slot_name) == 0) {
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
            if (!llvm_zone_relation_sync_field_name(field_name,
                    sizeof(field_name), "state", state_name))
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
                state_name, PGY_PROP_CAUSE_LINK);
            if (link != NULL) {
                const char *layer_name = link->data.zone_link.relation_slot_name;
                if (layer_name == NULL) {
                    for (size_t j = 0; j < state_count; j++) {
                        ASTNode *state = states[j];
                        if (state != NULL && state->type == AST_ZONE_STATE
                            && ast_zone_state_is_relation(state)
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
                    if (!llvm_zone_relation_sync_field_name(layer_field,
                            sizeof(layer_field), "layer_active", layer_name))
                        continue;
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
                        for (size_t j = 0; j < state_count; j++) {
                            ASTNode *state = states[j];
                            if (state != NULL && state->type == AST_ZONE_STATE
                                && ast_zone_state_is_relation(state)
                                && ast_zone_state_name(state) != NULL
                                && strcmp(ast_zone_state_name(state), state_name) == 0
                                && ast_zone_state_left_or_target_slot_name(state) != NULL
                                && ast_zone_state_right_slot_name(state) != NULL) {
                                llvm_zone_bind_relation_layer(stmt, decl_cls, sync_fn, ctx,
                                    layer_name,
                                    ast_zone_state_left_or_target_slot_name(state),
                                    ast_zone_state_right_slot_name(state));
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
            if (!llvm_zone_relation_sync_field_name(layer_field,
                    sizeof(layer_field), "layer_active",
                    link->data.zone_link.relation_slot_name))
                continue;
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

    for (size_t i = 0; i < maintained_relation_count; i++) {
        ASTNode *maintain = maintained_relations[i];
        if (maintain != NULL && maintain->data.zone_maintain_relation.relation_slot_name != NULL) {
            char layer_field[256];
            int layer_idx;
            LLVMValueRef self_ptr = LLVMGetParam(sync_fn, 0);
            LLVMValueRef layer_ptr;
            if (!llvm_zone_relation_sync_field_name(layer_field,
                    sizeof(layer_field), "layer_active",
                    maintain->data.zone_maintain_relation.relation_slot_name))
                continue;
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
        for (size_t j = 0; j < state_count; j++) {
            ASTNode *state = states[j];
            const char *state_name;
            char field_name[256];
            int field_idx;
            LLVMValueRef self_ptr;
            LLVMValueRef state_ptr;
            if (state == NULL || state->type != AST_ZONE_STATE
                || !ast_zone_state_is_relation(state)
                || ast_zone_state_layer_slot_name(state) == NULL
                || ast_zone_state_left_or_target_slot_name(state) == NULL
                || ast_zone_state_right_slot_name(state) == NULL
                || maintain->data.zone_maintain_relation.relation_slot_name == NULL
                || maintain->data.zone_maintain_relation.left_slot_name == NULL
                || maintain->data.zone_maintain_relation.right_slot_name == NULL
                || strcmp(ast_zone_state_layer_slot_name(state),
                          maintain->data.zone_maintain_relation.relation_slot_name) != 0
                || strcmp(ast_zone_state_left_or_target_slot_name(state),
                          maintain->data.zone_maintain_relation.left_slot_name) != 0
                || strcmp(ast_zone_state_right_slot_name(state),
                          maintain->data.zone_maintain_relation.right_slot_name) != 0)
                continue;
            state_name = ast_zone_state_name(state);
            if (!llvm_zone_relation_sync_field_name(field_name,
                    sizeof(field_name), "state", state_name))
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

    for (size_t i = 0; i < unlink_count; i++) {
        ASTNode *unlink = unlinks[i];
        const char *state_name = unlink != NULL ? unlink->data.zone_unlink.state_name : NULL;
        if (state_name == NULL && unlink != NULL) {
            for (size_t j = 0; j < state_count; j++) {
                ASTNode *state = states[j];
                if (state != NULL && state->type == AST_ZONE_STATE
                    && ast_zone_state_is_relation(state)
                    && ast_zone_state_layer_slot_name(state) != NULL
                    && ast_zone_state_left_or_target_slot_name(state) != NULL
                    && ast_zone_state_right_slot_name(state) != NULL
                    && unlink->data.zone_unlink.relation_slot_name != NULL
                    && unlink->data.zone_unlink.left_slot_name != NULL
                    && unlink->data.zone_unlink.right_slot_name != NULL
                    && strcmp(ast_zone_state_layer_slot_name(state),
                              unlink->data.zone_unlink.relation_slot_name) == 0
                    && strcmp(ast_zone_state_left_or_target_slot_name(state),
                              unlink->data.zone_unlink.left_slot_name) == 0
                    && strcmp(ast_zone_state_right_slot_name(state),
                              unlink->data.zone_unlink.right_slot_name) == 0) {
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
            if (!llvm_zone_relation_sync_field_name(field_name,
                    sizeof(field_name), "state", state_name))
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
                state_name, PGY_PROP_CAUSE_UNLINK);
            if (unlink != NULL) {
                const char *layer_name = unlink->data.zone_unlink.relation_slot_name;
                if (layer_name == NULL) {
                    for (size_t j = 0; j < state_count; j++) {
                        ASTNode *state = states[j];
                        if (state != NULL && state->type == AST_ZONE_STATE
                            && ast_zone_state_is_relation(state)
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
                    if (!llvm_zone_relation_sync_field_name(layer_field,
                            sizeof(layer_field), "layer_active", layer_name))
                        continue;
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
            if (!llvm_zone_relation_sync_field_name(layer_field,
                    sizeof(layer_field), "layer_active",
                    unlink->data.zone_unlink.relation_slot_name))
                continue;
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
