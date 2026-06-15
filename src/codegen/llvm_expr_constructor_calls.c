/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_constructor_calls.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "llvm_expr_constructor_channel_guard.h"
#include "llvm_backend_type_map_internal.h"
#include "llvm_domain_projection_count_helpers.h"
#include "llvm_inventory_decl_lookup.h"
#include "../common/string_compat.h"
#include "../compiler/mir_decl_headers.h"
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

static char *
llvm_class_constructor_field_type_name_at(LLVMGenCtx *ctx,
                                          const char *callee_name,
                                          size_t index)
{
    ASTNode *compat_decl = NULL;
    LLVMHostedFieldView field_view;
    const MIRDeclField *field_meta;
    const char *field_type_name;
    ASTNode *field_type;

    if (ctx == NULL || callee_name == NULL)
        return NULL;

    if (!llvm_active_has_mir(ctx)) {
        compat_decl = llvm_find_decl_in_active_inventory(
            ctx, AST_CLASS_DECL, callee_name);
    }
    field_view = llvm_hosted_class_field_view_from_decl(
        ctx, callee_name, compat_decl);
    if (llvm_active_has_mir(ctx) && !field_view.uses_mir_metadata) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing class-field constructor metadata for '%s'",
            callee_name);
        return NULL;
    }
    if (llvm_hosted_field_view_missing_mir_metadata(&field_view)) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing class-field constructor metadata for '%s'",
            callee_name);
        return NULL;
    }
    field_meta = llvm_hosted_field_view_metadata(&field_view, index);
    field_type_name = field_meta != NULL
        ? llvm_mir_decl_field_type_name(field_meta)
        : NULL;
    if (field_type_name != NULL)
        return pergyra_strdup(field_type_name);

    field_type = field_meta != NULL
        ? llvm_mir_decl_field_type(field_meta)
        : llvm_hosted_field_view_type(&field_view, index);
    return field_type != NULL
        ? llvm_render_type_name_in_ctx(ctx, field_type)
        : NULL;
}

static LLVMValueRef
llvm_emit_constructor_field_arg(ASTNode *node,
                                LLVMGenCtx *ctx,
                                const char *expected_type,
                                const char *field_name,
                                ASTNode *arg)
{
    const char *saved_expected_type_name;
    LLVMValueRef value;

    if (expected_type == NULL) {
        LLVMTypeRef inferred = llvm_stmt_infer_expr_type(ctx, arg);
        if (ctx->has_error)
            return NULL;
        if (inferred == ctx->type_void) {
            llvm_set_error_at_with_hints(ctx, arg,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_INSPECT_MIR_INVENTORY,
                "LLVM constructor field '%s' cannot consume a Void expression value",
                field_name != NULL ? field_name : "<field>");
            return NULL;
        }
        value = llvm_emit_expression(arg, ctx);
        if (value == NULL && !ctx->has_error) {
            llvm_set_error_at_with_hints(ctx, arg,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_INSPECT_MIR_INVENTORY,
                "LLVM constructor field '%s' could not lower initializer expression",
                field_name != NULL ? field_name : "<field>");
        }
        return value;
    }

    if (pgy_classify_type(expected_type) == PGY_TK_CHANNEL) {
        llvm_constructor_reject_channel_field(node, ctx, field_name);
        return NULL;
    }

    saved_expected_type_name = ctx->expected_type_name;
    ctx->expected_type_name = expected_type;
    {
        LLVMTypeRef inferred = llvm_stmt_infer_expr_type(ctx, arg);
        if (ctx->has_error) {
            ctx->expected_type_name = saved_expected_type_name;
            return NULL;
        }
        if (inferred == ctx->type_void) {
            llvm_set_error_at_with_hints(ctx, arg,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_INSPECT_MIR_INVENTORY,
                "LLVM constructor field '%s' cannot consume a Void expression value",
                field_name != NULL ? field_name : "<field>");
            ctx->expected_type_name = saved_expected_type_name;
            return NULL;
        }
    }
    value = llvm_emit_expression(arg, ctx);
    ctx->expected_type_name = saved_expected_type_name;
    if (value == NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, arg,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM constructor field '%s' could not lower initializer expression",
            field_name != NULL ? field_name : "<field>");
    }
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
    const MIRDeclHeader *enum_header = llvm_find_decl_header_in_context_of_type(
        ctx, AST_ENUM_DECL, variant->enum_name);
    if (llvm_active_has_mir(ctx) && enum_header == NULL) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing enum constructor variant metadata for '%s'",
            variant->enum_name != NULL ? variant->enum_name : "<anonymous-enum>");
        return NULL;
    }
    const MIRDeclEnumVariant *variant_meta = enum_header != NULL
        ? mir_decl_header_enum_variant(enum_header, variant_index) : NULL;
    if (enum_header != NULL && variant_meta == NULL) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path has invalid enum constructor variant metadata for '%s'",
            variant->enum_name != NULL ? variant->enum_name : "<anonymous-enum>");
        return NULL;
    }
    size_t param_count = enum_header != NULL
        ? mir_decl_enum_variant_param_count(variant_meta)
        : ast_enum_variant_param_count(enum_decl, variant_index);
    LLVMValueRef enum_val = LLVMGetUndef(enum_cls->struct_type);
    enum_val = LLVMBuildInsertValue(ctx->builder, enum_val,
        LLVMConstInt(ctx->type_i32, (unsigned long long)variant->value, 0),
        0, llvm_tmp_name(ctx));

    if (param_count > 0) {
        int field_idx = llvm_class_field_index(enum_cls, callee_name);
        if (field_idx > 0) {
            LLVMTypeRef payload_ty =
                llvm_class_field_type_at_index(enum_cls, field_idx);
            if (payload_ty == NULL) {
                return llvm_constructor_error(node, ctx,
                    "LLVM enum variant payload type metadata not found for field index");
            }
            LLVMValueRef payload = LLVMGetUndef(payload_ty);
            LLVMClassTypeEntry *payload_cls = llvm_lookup_class_by_type(ctx, payload_ty);

            for (size_t i = 0; i < param_count
                && i < ast_call_arg_count(node); i++) {
                ASTNode *arg_node = ast_call_argument(node, i);
                LLVMTypeRef arg_type = llvm_stmt_infer_expr_type(ctx,
                    arg_node);
                LLVMValueRef arg;
                if (ctx->has_error)
                    return NULL;
                if (arg_type == ctx->type_void) {
                    llvm_set_error_at_with_hints(ctx, arg_node,
                        PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                        PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                        PGY_FIX_ALIGN_ARG_TYPE,
                        "LLVM enum variant constructor '%s' cannot consume a Void expression as payload %zu",
                        callee_name != NULL ? callee_name : "<variant>",
                        i + 1);
                    return NULL;
                }
                arg = llvm_emit_expression(arg_node, ctx);
                if (arg == NULL)
                    return llvm_constructor_error(node, ctx,
                        "LLVM enum variant constructor could not lower payload argument");
                if (payload_cls != NULL
                    && i < (size_t)llvm_class_field_count(payload_cls)) {
                    LLVMTypeRef target_ty =
                        llvm_class_field_type_at(payload_cls, (int)i);
                    if (target_ty != NULL && target_ty != LLVMTypeOf(arg)) {
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

static bool
llvm_emit_class_constructor_shared_defaults(ASTNode *node, LLVMGenCtx *ctx,
                                            LLVMClassTypeEntry *cls,
                                            const LLVMHostedSharedFieldView *view,
                                            LLVMValueRef *object)
{
    if (object == NULL)
        return true;

    for (size_t i = 0; view != NULL && i < view->count; i++) {
        const char *shared_name =
            llvm_hosted_shared_field_view_name(view, i);
        ASTNode *initializer =
            llvm_hosted_shared_field_view_initializer(view, i);
        int field_idx;
        LLVMValueRef init_val;
        if (shared_name == NULL || initializer == NULL) {
            continue;
        }
        field_idx = llvm_class_field_index(cls, shared_name);
        if (field_idx < 0 || (size_t)field_idx < ast_call_arg_count(node))
            continue;
        init_val = llvm_emit_expression(initializer, ctx);
        if (init_val == NULL) {
            llvm_constructor_error(initializer, ctx,
                "LLVM class constructor could not lower shared-field initializer");
            return false;
        }
        *object = LLVMBuildInsertValue(ctx->builder, *object, init_val,
            (unsigned)field_idx, llvm_tmp_name(ctx));
    }
    return true;
}

static void
llvm_emit_class_constructor_projection_dirty(LLVMGenCtx *ctx,
                                             LLVMClassTypeEntry *cls,
                                             ASTNode *relation_decl,
                                             ASTNode *effect_decl,
                                             ASTNode *zone_decl,
                                             LLVMValueRef *object)
{
    ASTNode *decl = NULL;
    const char *decl_name = NULL;
    ASTNode **refreshes = NULL;
    size_t refresh_count = 0;

    if (object == NULL
        || (relation_decl == NULL && effect_decl == NULL && zone_decl == NULL)) {
        return;
    }

    if (relation_decl != NULL) {
        decl = relation_decl;
        refreshes = ast_relation_refreshes(relation_decl, &refresh_count);
    } else if (effect_decl != NULL) {
        decl = effect_decl;
        refreshes = ast_effect_refreshes(effect_decl, &refresh_count);
    } else if (zone_decl != NULL) {
        decl = zone_decl;
        refreshes = ast_zone_refreshes(zone_decl, &refresh_count);
    }

    decl_name = llvm_decl_node_name(decl);
    LLVMHostedDomainSlotView slot_view =
        llvm_hosted_domain_slot_view_from_decl(ctx, decl_name, decl);
    if (llvm_hosted_domain_slot_view_missing_mir_metadata(&slot_view)) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing domain-slot constructor metadata for '%s'",
            decl_name != NULL ? decl_name : "(anonymous-domain)");
        return;
    }

    for (size_t i = 0; i < slot_view.count; i++) {
        const char *slot_name =
            llvm_hosted_domain_slot_view_name(&slot_view, i);
        if (slot_name == NULL
            || !llvm_domain_slot_view_is_projection_slot(&slot_view, i,
                refreshes, refresh_count)) {
            continue;
        }
        {
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

    const char *world_name = llvm_decl_node_name(world_decl);
    LLVMHostedWorldZoneSlotView zone_view =
        llvm_hosted_world_zone_slot_view_from_decl(ctx, world_name,
            world_decl);
    if (llvm_hosted_world_zone_slot_view_missing_mir_metadata(&zone_view)) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing world zone-slot metadata for '%s'",
            world_name != NULL ? world_name : "<anonymous>");
        return;
    }
    for (size_t i = 0; i < zone_view.count; i++) {
        char dirty_field[256];
        int dirty_idx;
        const char *slot_name =
            llvm_hosted_world_zone_slot_view_name(&zone_view, i);
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

static bool
llvm_emit_one_field_claim(LLVMGenCtx *ctx, LLVMClassTypeEntry *cls,
                          LLVMValueRef *object, const char *slot_name,
                          const char *inner, bool is_secure,
                          const char *token_field)
{
    static unsigned long long s_field_token_counter = 0xC0FFEE01ULL;
    int slot_idx = llvm_class_field_index(cls, slot_name);
    LLVMTypeRef i1 = LLVMInt1TypeInContext(ctx->context);
    LLVMTypeRef slot_ty;
    LLVMValueRef slot_val;

    if (slot_idx < 0 || inner == NULL)
        return true;

    /* Inline the claim (mirrors the function-local LLVM claim) rather than
     * calling the struct-returning runtime claim, whose by-value struct ABI
     * does not match the LLVM aggregate type and corrupts the slot. */
    slot_ty = is_secure ? llvm_secure_slot_struct_type(ctx, inner)
                        : llvm_slot_struct_type(ctx, inner);
    slot_val = LLVMConstNull(slot_ty);
    slot_val = LLVMBuildInsertValue(ctx->builder, slot_val,
        LLVMConstInt(i1, 1, 0), 1, llvm_tmp_name(ctx));

    if (is_secure)
    {
        unsigned long long token_id = s_field_token_counter++;
        LLVMTypeRef token_ty = llvm_secure_token_type(ctx, inner);
        LLVMValueRef tok_val;

        slot_val = LLVMBuildInsertValue(ctx->builder, slot_val,
            LLVMConstInt(ctx->type_i64, token_id, 0), 2, llvm_tmp_name(ctx));
        *object = LLVMBuildInsertValue(ctx->builder, *object, slot_val,
            (unsigned)slot_idx, llvm_tmp_name(ctx));

        if (token_field != NULL)
        {
            int tok_idx = llvm_class_field_index(cls, token_field);
            if (tok_idx >= 0)
            {
                tok_val = LLVMConstNull(token_ty);
                tok_val = LLVMBuildInsertValue(ctx->builder, tok_val,
                    LLVMConstInt(ctx->type_i64, token_id, 0), 0,
                    llvm_tmp_name(ctx));
                tok_val = LLVMBuildInsertValue(ctx->builder, tok_val,
                    LLVMConstInt(i1, 1, 0), 1, llvm_tmp_name(ctx));
                tok_val = LLVMBuildInsertValue(ctx->builder, tok_val,
                    LLVMConstInt(i1, 1, 0), 2, llvm_tmp_name(ctx));
                *object = LLVMBuildInsertValue(ctx->builder, *object, tok_val,
                    (unsigned)tok_idx, llvm_tmp_name(ctx));
            }
        }
        return true;
    }

    *object = LLVMBuildInsertValue(ctx->builder, *object, slot_val,
        (unsigned)slot_idx, llvm_tmp_name(ctx));
    return true;
}
/* Claim each destructure slot field at construction so the built object has
 * live (occupied) slots, mirroring the C backend's __pgy_field_slot_init. */
static bool
llvm_emit_one_field_claim_meta(LLVMGenCtx *ctx, LLVMClassTypeEntry *cls,
                               LLVMValueRef *object,
                               const MIRDeclFieldClaim *claim)
{
    return llvm_emit_one_field_claim(
        ctx,
        cls,
        object,
        mir_decl_field_claim_slot_name(claim),
        mir_decl_field_claim_inner_type_name(claim),
        mir_decl_field_claim_is_secure(claim),
        mir_decl_field_claim_token_name(claim));
}

static bool
llvm_emit_field_slot_claims_from_header(LLVMGenCtx *ctx,
                                        const MIRDeclHeader *header,
                                        LLVMClassTypeEntry *cls,
                                        LLVMValueRef *object)
{
    size_t claim_count = mir_decl_header_field_claim_count(header);

    if (ctx == NULL || cls == NULL || object == NULL)
        return true;
    for (size_t i = 0; i < claim_count; i++) {
        if (!llvm_emit_one_field_claim_meta(
                ctx, cls, object, mir_decl_header_field_claim(header, i))) {
            return false;
        }
    }
    return true;
}

static bool
llvm_emit_field_slot_claims(LLVMGenCtx *ctx, ASTNode *host,
                            LLVMClassTypeEntry *cls, LLVMValueRef *object)
{
    size_t group_count;

    if (ctx == NULL || host == NULL || host->type != AST_CLASS_DECL
        || cls == NULL || object == NULL)
        return true;

    group_count = ast_class_field_destructure_count(host);
    for (size_t gi = 0; gi < group_count; gi++)
    {
        ASTNode *group = ast_class_field_destructure_at(host, gi);
        ASTNode *init;
        const char *callee;
        const char *slot_name;
        const char *inner;
        bool is_secure;

        if (group == NULL || ast_let_destructure_name_count(group) < 1)
            continue;
        init = ast_let_destructure_initializer(group);
        if (init == NULL || ast_call_callee(init) == NULL)
            continue;
        callee = ast_identifier_name(ast_call_callee(init));
        slot_name = ast_let_destructure_name(group, 0);
        inner = ast_call_generic_arg_count(init) > 0
            ? ast_generic_param_name(ast_call_generic_arg(init, 0)) : "Int";
        is_secure = callee != NULL && strcmp(callee, "ClaimSecureSlot") == 0;
        if (!is_secure
            && !(callee != NULL && strcmp(callee, "ClaimSlot") == 0))
            continue;
        if (!llvm_emit_one_field_claim(ctx, cls, object, slot_name, inner,
                is_secure,
                (is_secure && ast_let_destructure_name_count(group) >= 2)
                    ? ast_let_destructure_name(group, 1) : NULL))
            return false;
    }
    return true;
}

static LLVMValueRef
llvm_emit_class_constructor(ASTNode *node, LLVMGenCtx *ctx, const char *callee_name)
{
    LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, callee_name);
    ASTNode *host_decl = llvm_find_domain_constructor_decl(ctx, callee_name);

    /* If the binding context expects a specialized generic instantiation of
     * this class (e.g. Pair<Int>), construct that concrete entry rather than
     * the type-erased base, so field storage types match the arguments. */
    if (ctx != NULL && ctx->current_ret_type != NULL && callee_name != NULL) {
        LLVMClassTypeEntry *spec =
            llvm_lookup_class_by_struct_type(ctx, ctx->current_ret_type);
        size_t base_len = strlen(callee_name);
        if (spec != NULL && spec != cls && spec->class_name != NULL
            && strncmp(spec->class_name, callee_name, base_len) == 0
            && spec->class_name[base_len] == '<') {
            cls = spec;
        }
    }

    if (cls == NULL)
        return NULL;

    {
        const char *channel_field =
            llvm_constructor_find_host_channel_field(ctx,
                host_decl != NULL ? host_decl
                    : llvm_find_decl_in_active_inventory(
                        ctx, AST_CLASS_DECL, callee_name));
        if (channel_field != NULL) {
            llvm_constructor_reject_channel_field(node, ctx, channel_field);
            return NULL;
        }
    }

    LLVMValueRef object = LLVMConstNull(cls->struct_type);
    int field_count = llvm_class_field_count(cls);
    for (size_t i = 0; i < ast_call_arg_count(node)
        && i < (size_t)field_count; i++) {
        char *field_type_name =
            (host_decl == NULL || host_decl->type == AST_CLASS_DECL)
                ? llvm_class_constructor_field_type_name_at(ctx, callee_name, i)
                : NULL;
        const char *field_name = llvm_class_field_name_at(cls, (int)i);
        LLVMTypeRef expected_ty = llvm_class_field_type_at(cls, (int)i);
        int field_index = llvm_class_field_struct_index_at(cls, (int)i);
        if (ctx->has_error) {
            free(field_type_name);
            return NULL;
        }
        if (field_name == NULL || expected_ty == NULL || field_index < 0) {
            free(field_type_name);
            return llvm_constructor_error(node, ctx,
                "LLVM class constructor field metadata is incomplete");
        }
        LLVMValueRef arg;
        {
            LLVMTypeRef saved_ret = ctx->current_ret_type;
            ctx->current_ret_type = expected_ty;
            arg = llvm_emit_constructor_field_arg(node, ctx,
                field_type_name, field_name, ast_call_argument(node, i));
            ctx->current_ret_type = saved_ret;
            free(field_type_name);
        }
        if (arg == NULL)
            return llvm_constructor_error(node, ctx,
                "LLVM class constructor could not lower field argument");
        {
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
            (unsigned)field_index, llvm_tmp_name(ctx));
    }

    {
        if (llvm_active_has_mir(ctx)) {
            if (host_decl == NULL || host_decl->type == AST_CLASS_DECL) {
                const MIRDeclHeader *class_header =
                    llvm_find_decl_header_in_context_of_type(
                        ctx, AST_CLASS_DECL, callee_name);
                if (class_header == NULL) {
                    llvm_set_mir_inventory_missing(ctx,
                        "MIR-only LLVM path missing class field-claim metadata for '%s'",
                        callee_name != NULL ? callee_name : "(anonymous-class)");
                    return NULL;
                }
                if (!llvm_emit_field_slot_claims_from_header(
                        ctx, class_header, cls, &object)) {
                    return NULL;
                }
            }
        } else {
            ASTNode *class_ast = llvm_find_decl_in_active_inventory(
                ctx, AST_CLASS_DECL, callee_name);
            if (!llvm_emit_field_slot_claims(ctx, class_ast, cls, &object))
                return NULL;
        }
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

    if (!llvm_emit_class_constructor_shared_defaults(node, ctx, cls,
        &shared_view, &object)) {
        return NULL;
    }
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
