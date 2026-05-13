/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_constructor_calls.h"

#include <stdio.h>
#include <string.h>

#include "llvm_internal_api.h"
#include "parser/ast_api.h"

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
    size_t param_count =
        (enum_decl->data.enum_decl.variant_param_counts != NULL)
        ? enum_decl->data.enum_decl.variant_param_counts[variant_index] : 0;
    LLVMValueRef enum_val = LLVMGetUndef(enum_cls->struct_type);
    enum_val = LLVMBuildInsertValue(ctx->builder, enum_val,
        LLVMConstInt(ctx->type_i32, (unsigned long long)variant->value, 0),
        0, llvm_tmp_name(ctx));

    if (param_count > 0) {
        int field_idx = llvm_class_field_index(enum_cls, callee_name);
        if (field_idx > 0) {
            LLVMTypeRef payload_ty = enum_cls->fields[field_idx].field_type;
            LLVMValueRef payload = LLVMGetUndef(payload_ty);
            LLVMClassTypeEntry *payload_cls = llvm_lookup_class_by_type(ctx, payload_ty);

            for (size_t i = 0; i < param_count && i < node->data.call.arg_count; i++) {
                LLVMValueRef arg = llvm_emit_expression(node->data.call.arguments[i], ctx);
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
                                            ASTNode **shared_fields,
                                            size_t shared_count,
                                            LLVMValueRef *object)
{
    if (object == NULL)
        return;

    for (size_t i = 0; i < shared_count; i++) {
        ASTNode *shared = shared_fields[i];
        int field_idx;
        LLVMValueRef init_val;
        if (shared == NULL || shared->data.party_shared.name == NULL
            || shared->data.party_shared.initializer == NULL) {
            continue;
        }
        field_idx = llvm_class_field_index(cls, shared->data.party_shared.name);
        if (field_idx < 0 || (size_t)field_idx < node->data.call.arg_count)
            continue;
        init_val = llvm_emit_expression(shared->data.party_shared.initializer, ctx);
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
        const char *slot_name = slot != NULL ? slot->data.domain_slot.slot_name : NULL;
        bool projection_slot = false;

        if (slot == NULL || slot->type != AST_DOMAIN_SLOT || slot_name == NULL)
            continue;

        if (slot->data.domain_slot.is_tobject) {
            projection_slot = true;
        } else {
            for (size_t ri = 0; ri < refresh_count; ri++) {
                ASTNode *refresh = refreshes != NULL ? refreshes[ri] : NULL;
                if (refresh == NULL
                    || refresh->type != AST_ZONE_REFRESH
                    || refresh->data.zone_refresh.object_slot_name == NULL) {
                    continue;
                }
                if (strcmp(slot_name, refresh->data.zone_refresh.object_slot_name) == 0) {
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
        const char *slot_name = zone != NULL ? zone->data.world_zone.slot_name : NULL;
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
    if (cls == NULL)
        return NULL;

    LLVMValueRef object = LLVMConstNull(cls->struct_type);
    for (size_t i = 0; i < node->data.call.arg_count && i < (size_t)cls->field_count; i++) {
        LLVMValueRef arg = llvm_emit_expression(node->data.call.arguments[i], ctx);
        if (arg == NULL)
            return llvm_constructor_error(node, ctx,
                "LLVM class constructor could not lower field argument");
        object = LLVMBuildInsertValue(ctx->builder, object, arg,
            (unsigned)cls->fields[i].index, llvm_tmp_name(ctx));
    }

    ASTNode *party_decl = llvm_find_named_domain_decl(ctx, AST_PARTY_DECL, callee_name);
    ASTNode *roster_decl = llvm_find_named_domain_decl(ctx, AST_ROSTER_DECL, callee_name);
    ASTNode *relation_decl = llvm_find_named_domain_decl(ctx, AST_RELATION_DECL, callee_name);
    ASTNode *effect_decl = llvm_find_named_domain_decl(ctx, AST_EFFECT_DECL, callee_name);
    ASTNode *zone_decl = llvm_find_named_domain_decl(ctx, AST_ZONE_DECL, callee_name);
    ASTNode *world_decl = llvm_find_named_domain_decl(ctx, AST_WORLD_DECL, callee_name);
    ASTNode **shared_fields = NULL;
    size_t shared_count = 0;

    if (party_decl != NULL) {
        shared_fields = ast_party_shared_fields(party_decl, &shared_count);
    } else if (roster_decl != NULL) {
        shared_fields = ast_roster_shared_fields(roster_decl, &shared_count);
    } else if (relation_decl != NULL) {
        shared_fields = ast_relation_shared_fields(relation_decl, &shared_count);
    } else if (effect_decl != NULL) {
        shared_fields = ast_effect_shared_fields(effect_decl, &shared_count);
    } else if (zone_decl != NULL) {
        shared_fields = ast_zone_shared_fields(zone_decl, &shared_count);
    } else if (world_decl != NULL) {
        shared_fields = ast_world_shared_fields(world_decl, &shared_count);
    }

    llvm_emit_class_constructor_shared_defaults(node, ctx, cls,
        shared_fields, shared_count, &object);
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
