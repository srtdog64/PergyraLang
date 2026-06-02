#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_domain_query_calls.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "host_decl_compat.h"
#include "llvm_internal_api.h"
#include "llvm_inventory_decl_lookup.h"
#include "parser/ast_api.h"

typedef enum LLVMDomainQueryOp {
    LLVM_DOMAIN_QUERY_OP_NONE = 0,
    LLVM_DOMAIN_QUERY_OP_HAS_LAYER,
    LLVM_DOMAIN_QUERY_OP_HAS_PROJECTION,
    LLVM_DOMAIN_QUERY_OP_HAS_STATE,
    LLVM_DOMAIN_QUERY_OP_HAS_ZONE,
    LLVM_DOMAIN_QUERY_OP_HAS_ZONE_LAYER,
    LLVM_DOMAIN_QUERY_OP_HAS_ZONE_PROJECTION,
    LLVM_DOMAIN_QUERY_OP_HAS_ZONE_STATE,
} LLVMDomainQueryOp;

typedef struct LLVMDomainQuerySpec {
    const char *name;
    size_t min_argc;
    size_t max_argc;
    LLVMDomainQueryOp op;
} LLVMDomainQuerySpec;

static int
llvm_domain_query_spec_compare(const void *key, const void *entry)
{
    const char *name = *(const char * const *)key;
    const LLVMDomainQuerySpec *spec = (const LLVMDomainQuerySpec *)entry;

    return strcmp(name, spec->name);
}

static LLVMDomainQueryOp
llvm_domain_query_lookup(const char *callee_name, size_t argc)
{
    static const LLVMDomainQuerySpec kLLVMDomainQuerySpecs[] = {
        { "HasLayer", 1, 1, LLVM_DOMAIN_QUERY_OP_HAS_LAYER },
        { "HasProjection", 1, 1, LLVM_DOMAIN_QUERY_OP_HAS_PROJECTION },
        { "HasState", 1, (size_t)-1, LLVM_DOMAIN_QUERY_OP_HAS_STATE },
        { "HasZone", 1, 1, LLVM_DOMAIN_QUERY_OP_HAS_ZONE },
        { "HasZoneLayer", 2, 2, LLVM_DOMAIN_QUERY_OP_HAS_ZONE_LAYER },
        { "HasZoneProjection", 2, 2, LLVM_DOMAIN_QUERY_OP_HAS_ZONE_PROJECTION },
        { "HasZoneState", 2, 2, LLVM_DOMAIN_QUERY_OP_HAS_ZONE_STATE },
    };
    const LLVMDomainQuerySpec *match;

    if (callee_name == NULL)
        return LLVM_DOMAIN_QUERY_OP_NONE;

    match = (const LLVMDomainQuerySpec *)bsearch(&callee_name,
        kLLVMDomainQuerySpecs,
        sizeof(kLLVMDomainQuerySpecs) / sizeof(kLLVMDomainQuerySpecs[0]),
        sizeof(kLLVMDomainQuerySpecs[0]), llvm_domain_query_spec_compare);
    if (match == NULL || argc < match->min_argc || argc > match->max_argc)
        return LLVM_DOMAIN_QUERY_OP_NONE;
    return match->op;
}

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
    ASTNode *decl = pgy_host_decl_compat_has_projection_ready_flag(host_decl)
        ? host_decl
        : NULL;
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
        if (ast_world_state_source_kind(state_decl) == WORLD_STATE_SOURCE_ALL
            || ast_world_state_source_kind(state_decl) == WORLD_STATE_SOURCE_ANY) {
            LLVMValueRef result = LLVMConstInt(ctx->type_i1,
                ast_world_state_source_kind(state_decl) == WORLD_STATE_SOURCE_ALL ? 1 : 0, 0);
            base_ptr = llvm_current_self_base_ptr(ctx, cls);
            if (base_ptr == NULL) {
                *out = llvm_domain_query_false(ctx);
                return true;
            }
            for (size_t i = 0; i < ast_world_state_input_count(state_decl); i++) {
                const char *input_name = ast_world_state_input_name(state_decl, i);
                int input_idx = -1;
                char field_name[256];
                LLVMValueRef input_ptr;
                LLVMValueRef input_val;
                if (input_name == NULL)
                    continue;
                if (!llvm_domain_query_field_name(ctx, field_name, sizeof(field_name),
                        llvm_world_has_zone_slot(ctx, world_decl, input_name)
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
                if (ast_world_state_source_kind(state_decl) == WORLD_STATE_SOURCE_ALL)
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
    } else if (llvm_world_has_zone_slot(ctx, world_decl, name)) {
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
                                LLVMDomainQueryOp op, LLVMValueRef *out)
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
    const char *resolved_zone_name;
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
    resolved_zone_name = llvm_decl_node_name(zone_decl);
    zone_cls = resolved_zone_name != NULL ? llvm_lookup_class(ctx, resolved_zone_name) : NULL;
    zone_idx = llvm_class_field_index(world_cls, zone_name);
    if (zone_decl == NULL || zone_cls == NULL || zone_idx < 0) {
        *out = llvm_domain_query_false(ctx);
        return true;
    }

    if (op == LLVM_DOMAIN_QUERY_OP_HAS_ZONE_PROJECTION) {
        ASTNode *slot = llvm_find_zone_domain_slot_decl(ctx, zone_decl,
            detail_name);
        if (slot != NULL && !ast_domain_slot_is_subject(slot)) {
            char field_name[256];
            if (!llvm_domain_query_field_name(ctx, field_name, sizeof(field_name),
                    "__projection_ready_", detail_name, "zone projection query field name")) {
                *out = NULL;
                return true;
            }
            field_idx = llvm_class_field_index(zone_cls, field_name);
        }
    } else if (op == LLVM_DOMAIN_QUERY_OP_HAS_ZONE_LAYER) {
        if (llvm_find_zone_layer_slot_decl(
                ctx, zone_decl, detail_name) != NULL) {
            char field_name[256];
            if (!llvm_domain_query_field_name(ctx, field_name, sizeof(field_name),
                    "__layer_active_", detail_name, "zone layer query field name")) {
                *out = NULL;
                return true;
            }
            field_idx = llvm_class_field_index(zone_cls, field_name);
        }
    } else if (op == LLVM_DOMAIN_QUERY_OP_HAS_ZONE_STATE) {
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
    size_t arg_count = ast_call_arg_count(node);
    LLVMDomainQueryOp op = llvm_domain_query_lookup(callee_name, arg_count);

    if (op == LLVM_DOMAIN_QUERY_OP_HAS_PROJECTION)
        return llvm_emit_has_projection_query(node, ctx, out);
    if (op == LLVM_DOMAIN_QUERY_OP_HAS_LAYER)
        return llvm_emit_has_layer_query(node, ctx, out);
    if (op == LLVM_DOMAIN_QUERY_OP_HAS_STATE)
        return llvm_emit_has_state_query(node, ctx, out);
    if (op == LLVM_DOMAIN_QUERY_OP_HAS_ZONE)
        return llvm_emit_has_zone_query(node, ctx, out);
    if (op == LLVM_DOMAIN_QUERY_OP_HAS_ZONE_PROJECTION
        || op == LLVM_DOMAIN_QUERY_OP_HAS_ZONE_LAYER
        || op == LLVM_DOMAIN_QUERY_OP_HAS_ZONE_STATE) {
        return llvm_emit_has_zone_detail_query(node, ctx, op, out);
    }
    return false;
}

#endif /* PGY_LLVM_ENABLED */
