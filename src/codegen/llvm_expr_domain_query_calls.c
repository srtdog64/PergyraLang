#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_domain_query_calls.h"

#include <stdio.h>
#include <string.h>

#include "llvm_internal_api.h"
#include "llvm_inventory_decl_lookup.h"
#include "parser/ast_api.h"

static LLVMValueRef
llvm_load_current_bool_field(LLVMGenCtx *ctx, LLVMClassTypeEntry *cls,
                             int field_idx)
{
    LLVMValueRef base_ptr;
    LLVMValueRef gep;

    if (cls == NULL || field_idx < 0)
        return llvm_domain_query_false(ctx);

    base_ptr = llvm_current_self_base_ptr(ctx, cls);
    if (base_ptr == NULL)
        return llvm_domain_query_false(ctx);

    gep = LLVMBuildStructGEP2(ctx->builder, cls->struct_type, base_ptr,
        (unsigned)field_idx, llvm_tmp_name(ctx));
    return LLVMBuildLoad2(ctx->builder, ctx->type_i1, gep, llvm_tmp_name(ctx));
}

static bool
llvm_domain_query_field_name(LLVMGenCtx *ctx, char *out, size_t out_size,
                             const char *prefix, const char *name,
                             const char *what)
{
    int written;

    if (out == NULL || out_size == 0 || prefix == NULL || name == NULL)
        return false;

    written = snprintf(out, out_size, "%s%s", prefix, name);
    if (written >= 0 && (size_t)written < out_size)
        return true;

    llvm_set_error(ctx, "%s is too long", what != NULL ? what : "domain query field name");
    return false;
}

static bool
llvm_emit_has_projection_query(ASTNode *node, LLVMGenCtx *ctx, LLVMValueRef *out)
{
    ASTNode *host_decl = llvm_current_host_decl(ctx);
    const char *host_name = llvm_decl_node_name(host_decl);
    ASTNode *decl = (host_decl != NULL
        && (host_decl->type == AST_RELATION_DECL
            || host_decl->type == AST_EFFECT_DECL
            || host_decl->type == AST_ZONE_DECL))
        ? host_decl : NULL;
    LLVMClassTypeEntry *cls = host_name != NULL ? llvm_lookup_class(ctx, host_name) : NULL;
    const char *slot_name = llvm_call_name_or_string_arg(node, 0);
    int field_idx;

    if (out == NULL)
        return true;
    if (decl == NULL || cls == NULL || slot_name == NULL) {
        *out = llvm_domain_query_false(ctx);
        return true;
    }

    {
        char field_name[256];
        if (!llvm_domain_query_field_name(ctx, field_name, sizeof(field_name),
                "__projection_ready_", slot_name, "projection query field name")) {
            *out = NULL;
            return true;
        }
        field_idx = llvm_class_field_index(cls, field_name);
    }
    *out = llvm_load_current_bool_field(ctx, cls, field_idx);
    return true;
}

static bool
llvm_emit_has_layer_query(ASTNode *node, LLVMGenCtx *ctx, LLVMValueRef *out)
{
    ASTNode *host_decl = llvm_current_host_decl(ctx);
    const char *host_name = llvm_decl_node_name(host_decl);
    ASTNode *zone_decl = host_decl != NULL && host_decl->type == AST_ZONE_DECL
        ? host_decl : NULL;
    LLVMClassTypeEntry *cls = host_name != NULL ? llvm_lookup_class(ctx, host_name) : NULL;
    const char *layer_name = llvm_call_name_or_string_arg(node, 0);
    int field_idx;

    if (out == NULL)
        return true;
    if (zone_decl == NULL || cls == NULL || layer_name == NULL) {
        *out = llvm_domain_query_false(ctx);
        return true;
    }

    {
        char field_name[256];
        if (!llvm_domain_query_field_name(ctx, field_name, sizeof(field_name),
                "__layer_active_", layer_name, "layer query field name")) {
            *out = NULL;
            return true;
        }
        field_idx = llvm_class_field_index(cls, field_name);
    }
    *out = llvm_load_current_bool_field(ctx, cls, field_idx);
    return true;
}

static bool
llvm_emit_has_state_query(ASTNode *node, LLVMGenCtx *ctx, LLVMValueRef *out)
{
    ASTNode *host_decl = llvm_current_host_decl(ctx);
    const char *host_name = llvm_decl_node_name(host_decl);
    ASTNode *zone_decl = host_decl != NULL && host_decl->type == AST_ZONE_DECL
        ? host_decl : NULL;
    LLVMClassTypeEntry *cls = host_name != NULL ? llvm_lookup_class(ctx, host_name) : NULL;
    const char *state_name = llvm_call_name_or_string_arg(node, 0);
    ASTNode *state_decl;
    int field_idx;

    if (out == NULL)
        return true;
    if (zone_decl == NULL || cls == NULL || state_name == NULL) {
        *out = llvm_domain_query_false(ctx);
        return true;
    }

    state_decl = llvm_find_zone_state_decl(ctx, zone_decl, state_name);
    if (state_decl == NULL) {
        *out = llvm_domain_query_false(ctx);
        return true;
    }

    {
        char field_name[256];
        if (!llvm_domain_query_field_name(ctx, field_name, sizeof(field_name),
                "__state_", state_name, "state query field name")) {
            *out = NULL;
            return true;
        }
        field_idx = llvm_class_field_index(cls, field_name);
    }
    *out = llvm_load_current_bool_field(ctx, cls, field_idx);
    return true;
}

static bool
llvm_emit_has_zone_query(ASTNode *node, LLVMGenCtx *ctx, LLVMValueRef *out)
{
    ASTNode *host_decl = llvm_current_host_decl(ctx);
    const char *host_name = llvm_decl_node_name(host_decl);
    ASTNode *world_decl = host_decl != NULL && host_decl->type == AST_WORLD_DECL
        ? host_decl : NULL;
    LLVMClassTypeEntry *cls = host_name != NULL ? llvm_lookup_class(ctx, host_name) : NULL;
    const char *name = llvm_call_name_or_string_arg(node, 0);
    ASTNode *state_decl = NULL;
    int field_idx = -1;
    LLVMValueRef base_ptr;

    if (out == NULL)
        return true;
    if (world_decl == NULL || cls == NULL || name == NULL) {
        *out = llvm_domain_query_false(ctx);
        return true;
    }

    state_decl = llvm_find_world_state_decl(ctx, world_decl, name);
    if (state_decl != NULL) {
        if (state_decl->data.world_state.source_kind == WORLD_STATE_SOURCE_ALL
            || state_decl->data.world_state.source_kind == WORLD_STATE_SOURCE_ANY) {
            LLVMValueRef result = LLVMConstInt(ctx->type_i1,
                state_decl->data.world_state.source_kind == WORLD_STATE_SOURCE_ALL ? 1 : 0, 0);
            base_ptr = llvm_current_self_base_ptr(ctx, cls);
            if (base_ptr == NULL) {
                *out = llvm_domain_query_false(ctx);
                return true;
            }
            for (size_t i = 0; i < state_decl->data.world_state.input_count; i++) {
                const char *input_name = state_decl->data.world_state.input_names[i];
                int input_idx = -1;
                char field_name[256];
                LLVMValueRef input_ptr;
                LLVMValueRef input_val;
                if (input_name == NULL)
                    continue;
                if (!llvm_domain_query_field_name(ctx, field_name, sizeof(field_name),
                        llvm_world_has_zone_slot(world_decl, input_name)
                            ? "__zone_active_" : "__zone_state_",
                        input_name, "world zone query field name")) {
                    *out = NULL;
                    return true;
                }
                input_idx = llvm_class_field_index(cls, field_name);
                if (input_idx < 0)
                    continue;
                input_ptr = LLVMBuildStructGEP2(ctx->builder, cls->struct_type, base_ptr,
                    (unsigned)input_idx, llvm_tmp_name(ctx));
                input_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
                    input_ptr, llvm_tmp_name(ctx));
                if (state_decl->data.world_state.source_kind == WORLD_STATE_SOURCE_ALL)
                    result = LLVMBuildAnd(ctx->builder, result, input_val, llvm_tmp_name(ctx));
                else
                    result = LLVMBuildOr(ctx->builder, result, input_val, llvm_tmp_name(ctx));
            }
            *out = result;
            return true;
        }
        {
            char field_name[256];
            if (!llvm_domain_query_field_name(ctx, field_name, sizeof(field_name),
                    "__zone_state_", name, "world state query field name")) {
                *out = NULL;
                return true;
            }
            field_idx = llvm_class_field_index(cls, field_name);
        }
    } else if (llvm_world_has_zone_slot(world_decl, name)) {
        char field_name[256];
        if (!llvm_domain_query_field_name(ctx, field_name, sizeof(field_name),
                "__zone_active_", name, "world zone query field name")) {
            *out = NULL;
            return true;
        }
        field_idx = llvm_class_field_index(cls, field_name);
    }

    *out = llvm_load_current_bool_field(ctx, cls, field_idx);
    return true;
}

static bool
llvm_emit_has_zone_detail_query(ASTNode *node, LLVMGenCtx *ctx,
                                const char *callee_name, LLVMValueRef *out)
{
    ASTNode *host_decl = llvm_current_host_decl(ctx);
    const char *host_name = llvm_decl_node_name(host_decl);
    ASTNode *world_decl = (host_decl != NULL && host_decl->type == AST_WORLD_DECL)
        ? host_decl
        : llvm_find_named_domain_decl(ctx, AST_WORLD_DECL, host_name);
    LLVMClassTypeEntry *world_cls = llvm_lookup_class(ctx, host_name);
    const char *zone_name = llvm_call_name_or_string_arg(node, 0);
    const char *detail_name = llvm_call_name_or_string_arg(node, 1);
    ASTNode *zone_decl;
    LLVMClassTypeEntry *zone_cls;
    int zone_idx;
    int field_idx = -1;
    LLVMValueRef world_ptr;
    LLVMValueRef zone_ptr;
    LLVMValueRef gep;

    if (out == NULL)
        return true;
    if (host_name == NULL || world_decl == NULL || world_cls == NULL
        || zone_name == NULL || detail_name == NULL) {
        *out = llvm_domain_query_false(ctx);
        return true;
    }

    zone_decl = llvm_resolve_world_zone_decl(ctx, world_decl, zone_name);
    zone_cls = zone_decl != NULL && ast_zone_name(zone_decl) != NULL
        ? llvm_lookup_class(ctx, ast_zone_name(zone_decl))
        : NULL;
    zone_idx = llvm_class_field_index(world_cls, zone_name);
    if (zone_decl == NULL || zone_cls == NULL || zone_idx < 0) {
        *out = llvm_domain_query_false(ctx);
        return true;
    }

    if (strcmp(callee_name, "HasZoneProjection") == 0) {
        ASTNode *slot = llvm_find_zone_domain_slot_decl(zone_decl, detail_name);
        if (slot != NULL && !ast_domain_slot_is_subject(slot)) {
            char field_name[256];
            if (!llvm_domain_query_field_name(ctx, field_name, sizeof(field_name),
                    "__projection_ready_", detail_name, "zone projection query field name")) {
                *out = NULL;
                return true;
            }
            field_idx = llvm_class_field_index(zone_cls, field_name);
        }
    } else if (strcmp(callee_name, "HasZoneLayer") == 0) {
        if (llvm_find_zone_layer_slot_decl(zone_decl, detail_name) != NULL) {
            char field_name[256];
            if (!llvm_domain_query_field_name(ctx, field_name, sizeof(field_name),
                    "__layer_active_", detail_name, "zone layer query field name")) {
                *out = NULL;
                return true;
            }
            field_idx = llvm_class_field_index(zone_cls, field_name);
        }
    } else if (strcmp(callee_name, "HasZoneState") == 0) {
        if (llvm_find_zone_state_decl(ctx, zone_decl, detail_name) != NULL) {
            char field_name[256];
            if (!llvm_domain_query_field_name(ctx, field_name, sizeof(field_name),
                    "__state_", detail_name, "zone state query field name")) {
                *out = NULL;
                return true;
            }
            field_idx = llvm_class_field_index(zone_cls, field_name);
        }
    }

    if (field_idx < 0) {
        *out = llvm_domain_query_false(ctx);
        return true;
    }
    world_ptr = llvm_current_self_base_ptr(ctx, world_cls);
    if (world_ptr == NULL) {
        *out = llvm_domain_query_false(ctx);
        return true;
    }
    zone_ptr = LLVMBuildStructGEP2(ctx->builder, world_cls->struct_type, world_ptr,
        (unsigned)zone_idx, llvm_tmp_name(ctx));
    gep = LLVMBuildStructGEP2(ctx->builder, zone_cls->struct_type, zone_ptr,
        (unsigned)field_idx, llvm_tmp_name(ctx));
    *out = LLVMBuildLoad2(ctx->builder, ctx->type_i1, gep, llvm_tmp_name(ctx));
    return true;
}

bool
llvm_emit_domain_query_call(ASTNode *node, LLVMGenCtx *ctx,
                            const char *callee_name, LLVMValueRef *out)
{
    if (strcmp(callee_name, "HasProjection") == 0 && node->data.call.arg_count == 1)
        return llvm_emit_has_projection_query(node, ctx, out);
    if (strcmp(callee_name, "HasLayer") == 0 && node->data.call.arg_count == 1)
        return llvm_emit_has_layer_query(node, ctx, out);
    if (strcmp(callee_name, "HasState") == 0 && node->data.call.arg_count >= 1)
        return llvm_emit_has_state_query(node, ctx, out);
    if (strcmp(callee_name, "HasZone") == 0 && node->data.call.arg_count == 1)
        return llvm_emit_has_zone_query(node, ctx, out);
    if ((strcmp(callee_name, "HasZoneProjection") == 0
         || strcmp(callee_name, "HasZoneLayer") == 0
         || strcmp(callee_name, "HasZoneState") == 0)
        && node->data.call.arg_count == 2) {
        return llvm_emit_has_zone_detail_query(node, ctx, callee_name, out);
    }
    return false;
}

#endif /* PGY_LLVM_ENABLED */
