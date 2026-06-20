/*
 * Copyright (c) 2026 Pergyra Language Project
 * LLVM hosted zone state metadata view.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

#include <string.h>

#include "../compiler/mir_decl_headers.h"
#include "../parser/ast_api.h"

LLVMHostedZoneStateView
llvm_hosted_zone_state_view_from_decl(const LLVMGenCtx *ctx,
                                      const char *host_name,
                                      ASTNode *decl)
{
    LLVMHostedZoneStateView view;
    ASTNode **compat_states = NULL;
    size_t compat_count = 0;
    const MIRDeclHeader *header = NULL;

    if (decl != NULL && decl->type == AST_ZONE_DECL)
        compat_states = ast_zone_states(decl, &compat_count);

    view.decl_header = NULL;
    view.ast_compat_states = compat_states;
    view.ast_compat_count = compat_count;
    view.count = compat_count;
    view.uses_mir_metadata = false;
    view.requires_mir_metadata = llvm_active_has_mir(ctx)
        && compat_count > 0;

    header = llvm_find_host_decl_header_in_context(ctx, host_name);
    if (header != NULL
        && mir_decl_header_ast_type_or(header, AST_PROGRAM)
            == AST_ZONE_DECL) {
        view.decl_header = header;
        view.count = mir_decl_header_zone_state_count(header);
        view.uses_mir_metadata = true;
    }

    return view;
}

bool
llvm_hosted_zone_state_view_missing_mir_metadata(
    const LLVMHostedZoneStateView *view)
{
    return view != NULL
        && view->requires_mir_metadata
        && (!view->uses_mir_metadata
            || view->count != view->ast_compat_count)
        && view->ast_compat_count > 0;
}

const MIRDeclZoneState *
llvm_hosted_zone_state_view_metadata(const LLVMHostedZoneStateView *view,
                                     size_t index)
{
    if (view == NULL || !view->uses_mir_metadata
        || view->decl_header == NULL || index >= view->count) {
        return NULL;
    }
    return mir_decl_header_zone_state(view->decl_header, index);
}

const char *
llvm_hosted_zone_state_view_name(const LLVMHostedZoneStateView *view,
                                 size_t index)
{
    const MIRDeclZoneState *state =
        llvm_hosted_zone_state_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return NULL;
    if (state != NULL)
        return mir_decl_zone_state_name(state);
    if (view->requires_mir_metadata)
        return NULL;
    if (view->ast_compat_states != NULL
        && view->ast_compat_states[index] != NULL) {
        return ast_zone_state_name(view->ast_compat_states[index]);
    }
    return NULL;
}

const char *
llvm_hosted_zone_state_view_layer_slot_name(
    const LLVMHostedZoneStateView *view,
    size_t index)
{
    const MIRDeclZoneState *state =
        llvm_hosted_zone_state_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return NULL;
    if (state != NULL)
        return mir_decl_zone_state_layer_slot_name(state);
    if (view->requires_mir_metadata)
        return NULL;
    if (view->ast_compat_states != NULL
        && view->ast_compat_states[index] != NULL) {
        return ast_zone_state_layer_slot_name(view->ast_compat_states[index]);
    }
    return NULL;
}

const char *
llvm_hosted_zone_state_view_left_or_target_slot_name(
    const LLVMHostedZoneStateView *view,
    size_t index)
{
    const MIRDeclZoneState *state =
        llvm_hosted_zone_state_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return NULL;
    if (state != NULL)
        return mir_decl_zone_state_left_or_target_slot_name(state);
    if (view->requires_mir_metadata)
        return NULL;
    if (view->ast_compat_states != NULL
        && view->ast_compat_states[index] != NULL) {
        return ast_zone_state_left_or_target_slot_name(
            view->ast_compat_states[index]);
    }
    return NULL;
}

const char *
llvm_hosted_zone_state_view_right_slot_name(
    const LLVMHostedZoneStateView *view,
    size_t index)
{
    const MIRDeclZoneState *state =
        llvm_hosted_zone_state_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return NULL;
    if (state != NULL)
        return mir_decl_zone_state_right_slot_name(state);
    if (view->requires_mir_metadata)
        return NULL;
    if (view->ast_compat_states != NULL
        && view->ast_compat_states[index] != NULL) {
        return ast_zone_state_right_slot_name(view->ast_compat_states[index]);
    }
    return NULL;
}

bool
llvm_hosted_zone_state_view_is_relation(const LLVMHostedZoneStateView *view,
                                        size_t index)
{
    const MIRDeclZoneState *state =
        llvm_hosted_zone_state_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return false;
    if (state != NULL)
        return mir_decl_zone_state_is_relation(state);
    if (view->requires_mir_metadata)
        return false;
    return view->ast_compat_states != NULL
        && ast_zone_state_is_relation(view->ast_compat_states[index]);
}

bool
llvm_hosted_zone_state_view_rows_complete(
    const LLVMHostedZoneStateView *view)
{
    if (view == NULL || !view->uses_mir_metadata)
        return true;
    for (size_t i = 0; i < view->count; i++) {
        const char *state_name =
            llvm_hosted_zone_state_view_name(view, i);
        const char *state_layer =
            llvm_hosted_zone_state_view_layer_slot_name(view, i);
        const char *state_target =
            llvm_hosted_zone_state_view_left_or_target_slot_name(view, i);
        const char *state_right =
            llvm_hosted_zone_state_view_right_slot_name(view, i);
        if (state_name == NULL || state_layer == NULL
            || state_target == NULL
            || (llvm_hosted_zone_state_view_is_relation(view, i)
                && state_right == NULL)) {
            return false;
        }
    }
    return true;
}

bool
llvm_hosted_zone_state_view_find_name(
    const LLVMHostedZoneStateView *view,
    const char *state_name,
    size_t *index_out)
{
    if (view == NULL || state_name == NULL)
        return false;
    for (size_t i = 0; i < view->count; i++) {
        const char *candidate =
            llvm_hosted_zone_state_view_name(view, i);
        if (candidate != NULL && strcmp(candidate, state_name) == 0) {
            if (index_out != NULL)
                *index_out = i;
            return true;
        }
    }
    return false;
}

bool
llvm_hosted_zone_state_view_find_effect_state(
    const LLVMHostedZoneStateView *view,
    const char *layer_name,
    const char *target_name,
    size_t *index_out)
{
    if (view == NULL || layer_name == NULL || target_name == NULL)
        return false;
    for (size_t i = 0; i < view->count; i++) {
        const char *state_layer;
        const char *state_target;

        if (llvm_hosted_zone_state_view_is_relation(view, i))
            continue;
        state_layer = llvm_hosted_zone_state_view_layer_slot_name(view, i);
        state_target =
            llvm_hosted_zone_state_view_left_or_target_slot_name(view, i);
        if (state_layer != NULL && state_target != NULL
            && strcmp(state_layer, layer_name) == 0
            && strcmp(state_target, target_name) == 0) {
            if (index_out != NULL)
                *index_out = i;
            return true;
        }
    }
    return false;
}

bool
llvm_hosted_zone_state_view_find_relation_state(
    const LLVMHostedZoneStateView *view,
    const char *layer_name,
    const char *left_name,
    const char *right_name,
    size_t *index_out)
{
    if (view == NULL || layer_name == NULL
        || left_name == NULL || right_name == NULL) {
        return false;
    }
    for (size_t i = 0; i < view->count; i++) {
        const char *state_layer;
        const char *state_left;
        const char *state_right;

        if (!llvm_hosted_zone_state_view_is_relation(view, i))
            continue;
        state_layer = llvm_hosted_zone_state_view_layer_slot_name(view, i);
        state_left =
            llvm_hosted_zone_state_view_left_or_target_slot_name(view, i);
        state_right = llvm_hosted_zone_state_view_right_slot_name(view, i);
        if (state_layer != NULL && state_left != NULL && state_right != NULL
            && strcmp(state_layer, layer_name) == 0
            && strcmp(state_left, left_name) == 0
            && strcmp(state_right, right_name) == 0) {
            if (index_out != NULL)
                *index_out = i;
            return true;
        }
    }
    return false;
}

#endif
