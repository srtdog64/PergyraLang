/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_constructor_calls.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../compiler/mir_decl_headers.h"
#include "host_decl_compat.h"
#include "llvm_backend_type_map_internal.h"
#include "llvm_inventory_decl_lookup.h"
#include "llvm_internal_api.h"
#include "parser/ast_api.h"

static const char kLlvmMirSharedFieldMetadataMissing[] =
    "<mir-shared-field-metadata>";

static LLVMValueRef
llvm_constructor_error(ASTNode *node, LLVMGenCtx *ctx, const char *message)
{
    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "%s",
            message != NULL ? message
                : "LLVM constructor call could not be lowered");
    }
    return NULL;
}

static ASTNode *
llvm_class_constructor_field_type_at(LLVMGenCtx *ctx,
                                     const char *callee_name,
                                     size_t index)
{
    const MIRDeclHeader *header;
    const MIRDeclField *field;
    ASTNode *mir_type;
    ASTNode *class_decl;
    PgyHostClassFieldsCompatView field_view;

    if (ctx == NULL || callee_name == NULL)
        return NULL;

    header = llvm_find_host_decl_header_in_context(ctx, callee_name);
    field = mir_decl_header_field(header, index);
    mir_type = llvm_mir_decl_field_type(field);
    if (mir_type != NULL)
        return mir_type;

    class_decl = llvm_find_decl_in_active_inventory(
        ctx, AST_CLASS_DECL, callee_name);
    if (class_decl == NULL)
        return NULL;
    field_view = pgy_host_class_fields_compat_view_from_decl(class_decl);
    if (field_view.fields == NULL || index >= field_view.count
        || field_view.fields[index] == NULL)
        return NULL;
    return field_view.fields[index]->type;
}

static bool
llvm_constructor_field_is_channel(LLVMGenCtx *ctx, ASTNode *field_type)
{
    char *expected_type;
    bool is_channel;

    if (field_type == NULL)
        return false;
    if (field_type->type == AST_CHANNEL_TYPE)
        return true;
    expected_type = llvm_render_type_name_in_ctx(ctx, field_type);
    is_channel = pgy_classify_type(expected_type) == PGY_TK_CHANNEL;
    free(expected_type);
    return is_channel;
}

static const char *
llvm_class_constructor_find_channel_field(LLVMGenCtx *ctx, ASTNode *class_decl)
{
    PgyHostClassFieldsCompatView class_fields;

    if (ctx == NULL || class_decl == NULL)
        return NULL;
    class_fields = pgy_host_class_fields_compat_view_from_decl(class_decl);
    for (size_t i = 0;
         class_fields.fields != NULL && i < class_fields.count; i++) {
        ClassField *field = class_fields.fields[i];
        if (field != NULL
            && llvm_constructor_field_is_channel(ctx, field->type)) {
            return field->name;
        }
    }
    return NULL;
}

static const char *
llvm_constructor_find_shared_channel_field(
    LLVMGenCtx *ctx,
    const LLVMHostedSharedFieldView *view)
{
    for (size_t i = 0; view != NULL && i < view->count; i++) {
        ASTNode *field_type = llvm_hosted_shared_field_view_type(view, i);
        if (llvm_constructor_field_is_channel(ctx, field_type))
            return llvm_hosted_shared_field_view_name(view, i);
    }
    return NULL;
}

static const char *
llvm_constructor_find_slot_channel_field(LLVMGenCtx *ctx,
                                         ASTNode **slots,
                                         size_t count)
{
    for (size_t i = 0; slots != NULL && i < count; i++) {
        ASTNode *slot = slots[i];
        if (slot != NULL
            && llvm_constructor_field_is_channel(
                ctx, ast_domain_slot_type(slot))) {
            return ast_domain_slot_name(slot);
        }
    }
    return NULL;
}

static bool
llvm_constructor_mir_field_is_channel(LLVMGenCtx *ctx,
                                      const MIRDeclField *field)
{
    ASTNode *type_node;
    const char *type_name;

    if (field == NULL)
        return false;

    type_node = llvm_mir_decl_field_type(field);
    if (llvm_constructor_field_is_channel(ctx, type_node))
        return true;

    type_name = llvm_mir_decl_field_type_name(field);
    return pgy_classify_type(type_name) == PGY_TK_CHANNEL;
}

static const char *
llvm_constructor_find_mir_channel_field(LLVMGenCtx *ctx, ASTNode *decl)
{
    const MIRDeclHeader *header;
    const char *host_name;

    if (ctx == NULL || decl == NULL)
        return NULL;

    host_name = llvm_decl_node_name(decl);
    header = llvm_find_host_decl_header_in_context(ctx, host_name);
    for (size_t i = 0; header != NULL
         && i < mir_decl_header_field_count(header); i++) {
        const MIRDeclField *field = mir_decl_header_field(header, i);
        if (llvm_constructor_mir_field_is_channel(ctx, field))
            return mir_decl_field_name(field);
    }
    return NULL;
}

static const char *
llvm_constructor_fail_shared_metadata_missing(LLVMGenCtx *ctx,
                                              const char *decl_name)
{
    llvm_set_mir_inventory_missing(ctx,
        "MIR-only LLVM path missing shared-field channel metadata for constructor '%s'",
        decl_name != NULL ? decl_name : "(anonymous-domain)");
    return kLlvmMirSharedFieldMetadataMissing;
}

static const char *
llvm_constructor_find_host_channel_field(LLVMGenCtx *ctx, ASTNode *decl)
{
    ASTNode **nodes = NULL;
    size_t count = 0;
    const char *decl_name;
    const char *mir_channel_field;

    if (ctx == NULL || decl == NULL)
        return NULL;

    decl_name = llvm_decl_node_name(decl);

    mir_channel_field = llvm_constructor_find_mir_channel_field(ctx, decl);
    if (mir_channel_field != NULL)
        return mir_channel_field;

    if (decl->type == AST_CLASS_DECL)
        return llvm_class_constructor_find_channel_field(ctx, decl);

    switch (decl->type) {
    case AST_PARTY_DECL:
    case AST_ROSTER_DECL: {
        LLVMHostedSharedFieldView shared =
            llvm_hosted_shared_field_view_from_decl(ctx, decl_name, decl);
        if (llvm_hosted_shared_field_view_missing_mir_metadata(&shared))
            return llvm_constructor_fail_shared_metadata_missing(
                ctx, decl_name);
        return llvm_constructor_find_shared_channel_field(ctx, &shared);
    }
    case AST_RELATION_DECL:
        nodes = ast_relation_slots(decl, &count);
        {
            const char *slot =
                llvm_constructor_find_slot_channel_field(ctx, nodes, count);
            if (slot != NULL)
                return slot;
        }
        {
            LLVMHostedSharedFieldView shared =
                llvm_hosted_shared_field_view_from_decl(ctx, decl_name, decl);
            if (llvm_hosted_shared_field_view_missing_mir_metadata(&shared))
                return llvm_constructor_fail_shared_metadata_missing(
                    ctx, decl_name);
            return llvm_constructor_find_shared_channel_field(ctx, &shared);
        }
    case AST_EFFECT_DECL:
        nodes = ast_effect_slots(decl, &count);
        {
            const char *slot =
                llvm_constructor_find_slot_channel_field(ctx, nodes, count);
            if (slot != NULL)
                return slot;
        }
        {
            LLVMHostedSharedFieldView shared =
                llvm_hosted_shared_field_view_from_decl(ctx, decl_name, decl);
            if (llvm_hosted_shared_field_view_missing_mir_metadata(&shared))
                return llvm_constructor_fail_shared_metadata_missing(
                    ctx, decl_name);
            return llvm_constructor_find_shared_channel_field(ctx, &shared);
        }
    case AST_ZONE_DECL:
        nodes = ast_zone_slots(decl, &count);
        {
            const char *slot =
                llvm_constructor_find_slot_channel_field(ctx, nodes, count);
            if (slot != NULL)
                return slot;
        }
        {
            LLVMHostedSharedFieldView shared =
                llvm_hosted_shared_field_view_from_decl(ctx, decl_name, decl);
            if (llvm_hosted_shared_field_view_missing_mir_metadata(&shared))
                return llvm_constructor_fail_shared_metadata_missing(
                    ctx, decl_name);
            return llvm_constructor_find_shared_channel_field(ctx, &shared);
        }
    case AST_WORLD_DECL: {
        LLVMHostedSharedFieldView shared =
            llvm_hosted_shared_field_view_from_decl(ctx, decl_name, decl);
        if (llvm_hosted_shared_field_view_missing_mir_metadata(&shared))
            return llvm_constructor_fail_shared_metadata_missing(
                ctx, decl_name);
        return llvm_constructor_find_shared_channel_field(ctx, &shared);
    }
    default:
        return NULL;
    }
}

static LLVMValueRef
llvm_emit_constructor_field_arg(ASTNode *node,
                                LLVMGenCtx *ctx,
                                ASTNode *field_type,
                                const char *field_name,
                                ASTNode *arg)
{
    char *expected_type;
    const char *saved_expected_type_name;
    LLVMValueRef value;

    if (field_type == NULL)
        return llvm_emit_expression(arg, ctx);

    expected_type = llvm_render_type_name_in_ctx(ctx, field_type);
    if (pgy_classify_type(expected_type) == PGY_TK_CHANNEL) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_PROVIDE_MOVABLE_HANDLE,
            "LLVM backend: Channel field '%s' cannot be aggregate-constructed or default-initialized until movable channel-handle lowering is available",
            field_name != NULL ? field_name : "<field>");
        free(expected_type);
        return NULL;
    }

    saved_expected_type_name = ctx->expected_type_name;
    ctx->expected_type_name = expected_type;
    value = llvm_emit_expression(arg, ctx);
    ctx->expected_type_name = saved_expected_type_name;
    free(expected_type);
    return value;
}

static LLVMValueRef
llvm_emit_enum_variant_constructor(ASTNode *node, LLVMGenCtx *ctx,
                                   const char *callee_name)
{
    LLVMEnumVariantEntry *variant = llvm_lookup_enum_variant(ctx, callee_name);
    if (variant == NULL)
        return NULL;

    ASTNode *enum_decl = llvm_find_enum_decl(ctx, variant->enum_name);
    LLVMClassTypeEntry *enum_cls = llvm_lookup_class(ctx, variant->enum_name);
    if (enum_decl == NULL || enum_cls == NULL)
        return llvm_constructor_error(node, ctx,
            "LLVM enum variant constructor requires enum declaration and class metadata");

    size_t variant_index = (size_t)variant->value;
    size_t param_count = ast_enum_variant_param_count(enum_decl, variant_index);
    LLVMValueRef enum_val = LLVMGetUndef(enum_cls->struct_type);
    enum_val = LLVMBuildInsertValue(ctx->builder, enum_val,
        LLVMConstInt(ctx->type_i32, (unsigned long long)variant->value, 0),
        0, llvm_tmp_name(ctx));

    if (param_count > 0) {
        int field_idx = llvm_class_field_index(enum_cls, callee_name);
        if (field_idx > 0) {
            /*
             * `field_idx` is the LLVM struct field position (e.g. 2 for the
             * second variant when a no-payload variant precedes it).
             * `enum_cls->fields[]` is a parallel array of LLVMClassField
             * entries with its own array index. Read the payload type by
             * iterating until we find the entry whose .index matches.
             */
            LLVMTypeRef payload_ty = NULL;
            for (int fi = 0; fi < enum_cls->field_count; fi++) {
                if (enum_cls->fields[fi].index == field_idx) {
                    payload_ty = enum_cls->fields[fi].field_type;
                    break;
                }
            }
            if (payload_ty == NULL) {
                return llvm_constructor_error(node, ctx,
                    "LLVM enum variant payload type metadata not found for field index");
            }
            LLVMValueRef payload = LLVMGetUndef(payload_ty);
            LLVMClassTypeEntry *payload_cls = llvm_lookup_class_by_type(ctx, payload_ty);

            for (size_t i = 0; i < param_count
                && i < ast_call_arg_count(node); i++) {
                LLVMValueRef arg = llvm_emit_expression(
                    ast_call_argument(node, i), ctx);
                if (arg == NULL)
                    return llvm_constructor_error(node, ctx,
                        "LLVM enum variant constructor could not lower payload argument");
                if (payload_cls != NULL
                    && i < (size_t)payload_cls->field_count
                    && payload_cls->fields[i].field_type != LLVMTypeOf(arg)) {
                    LLVMTypeRef target_ty = payload_cls->fields[i].field_type;
                    if ((target_ty == ctx->type_i32 || target_ty == ctx->type_i64)
                        && (LLVMTypeOf(arg) == ctx->type_i32
                            || LLVMTypeOf(arg) == ctx->type_i64)) {
                        arg = (LLVMGetIntTypeWidth(target_ty)
                            > LLVMGetIntTypeWidth(LLVMTypeOf(arg)))
                            ? LLVMBuildSExt(ctx->builder, arg, target_ty,
                                llvm_tmp_name(ctx))
                            : LLVMBuildTrunc(ctx->builder, arg, target_ty,
                                llvm_tmp_name(ctx));
                    }
                }
                payload = LLVMBuildInsertValue(ctx->builder, payload, arg,
                    (unsigned)i, llvm_tmp_name(ctx));
            }
            enum_val = LLVMBuildInsertValue(ctx->builder, enum_val, payload,
                (unsigned)field_idx, llvm_tmp_name(ctx));
        }
    }

    return enum_val;
}

static void
llvm_emit_class_constructor_shared_defaults(ASTNode *node, LLVMGenCtx *ctx,
                                            LLVMClassTypeEntry *cls,
                                            const LLVMHostedSharedFieldView *view,
                                            LLVMValueRef *object)
{
    if (object == NULL)
        return;

    for (size_t i = 0; view != NULL && i < view->count; i++) {
        ASTNode *shared = llvm_hosted_shared_field_view_source_ast(view, i);
        const char *shared_name =
            llvm_hosted_shared_field_view_name(view, i);
        ASTNode *initializer = shared != NULL
            ? ast_party_shared_initializer(shared) : NULL;
        int field_idx;
        LLVMValueRef init_val;
        if (shared == NULL || shared_name == NULL || initializer == NULL) {
            continue;
        }
        field_idx = llvm_class_field_index(cls, shared_name);
        if (field_idx < 0 || (size_t)field_idx < ast_call_arg_count(node))
            continue;
        init_val = llvm_emit_expression(initializer, ctx);
        if (init_val == NULL)
            continue;
        *object = LLVMBuildInsertValue(ctx->builder, *object, init_val,
            (unsigned)field_idx, llvm_tmp_name(ctx));
    }
}

static void
llvm_emit_class_constructor_projection_dirty(LLVMGenCtx *ctx,
                                             LLVMClassTypeEntry *cls,
                                             ASTNode *relation_decl,
                                             ASTNode *effect_decl,
                                             ASTNode *zone_decl,
                                             LLVMValueRef *object)
{
    ASTNode **slots = NULL;
    size_t slot_count = 0;
    ASTNode **refreshes = NULL;
    size_t refresh_count = 0;

    if (object == NULL
        || (relation_decl == NULL && effect_decl == NULL && zone_decl == NULL)) {
        return;
    }

    if (relation_decl != NULL) {
        slots = ast_relation_slots(relation_decl, &slot_count);
        refreshes = ast_relation_refreshes(relation_decl, &refresh_count);
    } else if (effect_decl != NULL) {
        slots = ast_effect_slots(effect_decl, &slot_count);
        refreshes = ast_effect_refreshes(effect_decl, &refresh_count);
    } else if (zone_decl != NULL) {
        slots = ast_zone_slots(zone_decl, &slot_count);
        refreshes = ast_zone_refreshes(zone_decl, &refresh_count);
    }

    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots != NULL ? slots[i] : NULL;
        const char *slot_name = ast_domain_slot_name(slot);
        bool projection_slot = false;

        if (slot == NULL || slot->type != AST_DOMAIN_SLOT || slot_name == NULL)
            continue;

        if (ast_domain_slot_is_tobject(slot)) {
            projection_slot = true;
        } else {
            for (size_t ri = 0; ri < refresh_count; ri++) {
                ASTNode *refresh = refreshes != NULL ? refreshes[ri] : NULL;
                if (refresh == NULL
                    || refresh->type != AST_ZONE_REFRESH
                    || ast_zone_refresh_object_slot_name(refresh) == NULL) {
                    continue;
                }
                if (strcmp(slot_name, ast_zone_refresh_object_slot_name(refresh)) == 0) {
                    projection_slot = true;
                    break;
                }
            }
        }

        if (projection_slot) {
            char dirty_field[256];
            int dirty_idx;

            snprintf(dirty_field, sizeof(dirty_field), "__projection_dirty_%s", slot_name);
            dirty_idx = llvm_class_field_index(cls, dirty_field);
            if (dirty_idx < 0)
                continue;
            *object = LLVMBuildInsertValue(ctx->builder, *object,
                LLVMConstInt(ctx->type_i1, 1, 0),
                (unsigned)dirty_idx, llvm_tmp_name(ctx));
        }
    }
}

static void
llvm_emit_class_constructor_world_dirty(LLVMGenCtx *ctx,
                                        LLVMClassTypeEntry *cls,
                                        ASTNode *world_decl,
                                        LLVMValueRef *object)
{
    if (object == NULL || world_decl == NULL || world_decl->type != AST_WORLD_DECL)
        return;

    int derived_idx = llvm_class_field_index(cls, "__world_derived_dirty");
    if (derived_idx >= 0) {
        *object = LLVMBuildInsertValue(ctx->builder, *object,
            LLVMConstInt(ctx->type_i1, 1, 0),
            (unsigned)derived_idx, llvm_tmp_name(ctx));
    }

    size_t zone_count = 0;
    ASTNode **zones = ast_world_zones(world_decl, &zone_count);
    for (size_t i = 0; i < zone_count; i++) {
        ASTNode *zone = zones[i];
        char dirty_field[256];
        int dirty_idx;
        const char *slot_name = ast_world_zone_slot_name(zone);
        if (slot_name == NULL)
            continue;
        snprintf(dirty_field, sizeof(dirty_field), "__zone_dirty_%s", slot_name);
        dirty_idx = llvm_class_field_index(cls, dirty_field);
        if (dirty_idx < 0)
            continue;
        *object = LLVMBuildInsertValue(ctx->builder, *object,
            LLVMConstInt(ctx->type_i1, 1, 0),
            (unsigned)dirty_idx, llvm_tmp_name(ctx));
    }
}

static LLVMValueRef
llvm_emit_class_constructor(ASTNode *node, LLVMGenCtx *ctx, const char *callee_name)
{
    LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, callee_name);
    ASTNode *host_decl = llvm_find_domain_constructor_decl(ctx, callee_name);
    if (cls == NULL)
        return NULL;

    {
        const char *channel_field =
            llvm_constructor_find_host_channel_field(ctx,
                host_decl != NULL ? host_decl
                    : llvm_find_decl_in_active_inventory(
                        ctx, AST_CLASS_DECL, callee_name));
        if (channel_field != NULL) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_PROVIDE_MOVABLE_HANDLE,
                "LLVM backend: Channel field '%s' cannot be aggregate-constructed or default-initialized until movable channel-handle lowering is available",
                channel_field);
            return NULL;
        }
    }

    LLVMValueRef object = LLVMConstNull(cls->struct_type);
    for (size_t i = 0; i < ast_call_arg_count(node)
        && i < (size_t)cls->field_count; i++) {
        ASTNode *field_type = llvm_class_constructor_field_type_at(
            ctx, callee_name, i);
        const char *field_name = cls->fields[i].field_name;
        LLVMValueRef arg = llvm_emit_constructor_field_arg(node, ctx,
            field_type, field_name, ast_call_argument(node, i));
        if (arg == NULL)
            return llvm_constructor_error(node, ctx,
                "LLVM class constructor could not lower field argument");
        {
            LLVMTypeRef expected_ty = cls->fields[i].field_type;
            LLVMTypeRef actual_ty = LLVMTypeOf(arg);
            if (expected_ty != actual_ty) {
                if ((expected_ty == ctx->type_i32 || expected_ty == ctx->type_i64)
                    && (actual_ty == ctx->type_i32 || actual_ty == ctx->type_i64)) {
                    arg = (LLVMGetIntTypeWidth(expected_ty)
                        > LLVMGetIntTypeWidth(actual_ty))
                        ? LLVMBuildSExt(ctx->builder, arg, expected_ty,
                            llvm_tmp_name(ctx))
                        : LLVMBuildTrunc(ctx->builder, arg, expected_ty,
                            llvm_tmp_name(ctx));
                } else {
                    llvm_set_error_at_with_hints(ctx, node,
                        PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                        PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                        PGY_FIX_PROVIDE_MOVABLE_HANDLE,
                        "LLVM backend: class '%s' field '%s' aggregate construction type mismatch; runtime-bound types (e.g. Channel) require movable handle lowering instead of inline storage",
                        callee_name,
                        field_name != NULL ? field_name : "<field>");
                    return NULL;
                }
            }
        }
        object = LLVMBuildInsertValue(ctx->builder, object, arg,
            (unsigned)cls->fields[i].index, llvm_tmp_name(ctx));
    }

    ASTNode *relation_decl = host_decl != NULL
        && host_decl->type == AST_RELATION_DECL ? host_decl : NULL;
    ASTNode *effect_decl = host_decl != NULL
        && host_decl->type == AST_EFFECT_DECL ? host_decl : NULL;
    ASTNode *zone_decl = host_decl != NULL
        && host_decl->type == AST_ZONE_DECL ? host_decl : NULL;
    ASTNode *world_decl = host_decl != NULL
        && host_decl->type == AST_WORLD_DECL ? host_decl : NULL;
    const char *host_name = llvm_decl_node_name(host_decl);
    LLVMHostedSharedFieldView shared_view =
        llvm_hosted_shared_field_view_from_decl(ctx, host_name, host_decl);

    if (llvm_hosted_shared_field_view_missing_mir_metadata(&shared_view)) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing shared-field constructor metadata for '%s'",
            host_name != NULL ? host_name : "(anonymous-domain)");
        return NULL;
    }

    llvm_emit_class_constructor_shared_defaults(node, ctx, cls, &shared_view,
        &object);
    llvm_emit_class_constructor_projection_dirty(ctx, cls,
        relation_decl, effect_decl, zone_decl, &object);
    llvm_emit_class_constructor_world_dirty(ctx, cls, world_decl, &object);
    return object;
}

LLVMValueRef
llvm_emit_constructor_call(ASTNode *node, LLVMGenCtx *ctx, const char *callee_name)
{
    LLVMValueRef value = llvm_emit_enum_variant_constructor(node, ctx, callee_name);
    if (value != NULL)
        return value;
    return llvm_emit_class_constructor(node, ctx, callee_name);
}

#endif
