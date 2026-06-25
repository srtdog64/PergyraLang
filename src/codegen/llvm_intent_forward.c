/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM backend MIR-backed intent forward declarations.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_intent_internal.h"

#include <string.h>

static const char *
llvm_forward_intent_involves_type_name(ASTNode *involves)
{
    ASTNode *subject_type = NULL;

    if (involves == NULL || involves->type != AST_INTENT_INVOLVES)
        return NULL;
    subject_type = ast_intent_involves_subject_type(involves);
    if (subject_type == NULL) {
        return NULL;
    }
    return ast_type_name(subject_type);
}

static bool
llvm_forward_declare_intent_from_mir_routine(LLVMGenCtx *ctx,
                                             const MIRRoutine *routine)
{
    const char *name;
    IntentBindingMetadataView binding_metadata = {0};
    LLVMTypeRef *param_types = NULL;
    LLVMTypeRef fn_type;
    LLVMValueRef fn;
    size_t param_count;

    if (ctx == NULL || routine == NULL)
        return false;
    if (llvm_mir_routine_kind(routine) != MIR_SCOPE_INTENT)
        return true;

    name = llvm_mir_routine_name(routine);
    if (name == NULL) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path has unnamed intent routine inventory row");
        return false;
    }
    if (llvm_lookup_function(ctx, name) != NULL)
        return true;

    param_count = llvm_collect_mir_intent_bindings(
        routine, ctx, &binding_metadata);
    for (size_t i = 0; i < param_count; i++) {
        if (!intent_binding_metadata_view_has_complete_row(
                &binding_metadata, i)) {
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path has incomplete ordered intent binding metadata for forward declaration '%s'",
                name);
            return false;
        }
        if (!intent_binding_metadata_kind_is_supported(
                intent_binding_metadata_view_kind_at(
                    &binding_metadata, i))) {
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path has invalid ordered intent binding metadata for forward declaration '%s'",
                name);
            return false;
        }
    }

    if (param_count > 0) {
        param_types = pgy_arena_calloc(&ctx->scratch,
            param_count * sizeof(LLVMTypeRef));
        if (param_types == NULL) {
            llvm_set_error(ctx,
                "LLVM intent forward parameter allocation failed for '%s'",
                name);
            return false;
        }
        for (size_t i = 0; i < param_count; i++) {
            const char *type_name =
                intent_binding_metadata_view_type_at(&binding_metadata, i);
            LLVMTypeRef pt = pergyra_type_to_llvm(ctx, type_name);
            if (ctx->has_error || pt == NULL)
                return false;
            if (intent_binding_metadata_view_row_is_kind(
                    &binding_metadata, i, "participant")
                && llvm_type_name_uses_pointer_self(ctx, type_name)) {
                pt = LLVMPointerType(pt, 0);
            }
            param_types[i] = pt;
        }
    }

    fn_type = LLVMFunctionType(ctx->type_i1, param_types,
        (unsigned)param_count, 0);
    fn = LLVMAddFunction(ctx->module, name, fn_type);
    {
        unsigned attr_kind = LLVMGetEnumAttributeKindForName(
            "no-stack-arg-probe", 18);
        if (attr_kind != 0) {
            LLVMAddAttributeAtIndex(fn, LLVMAttributeFunctionIndex,
                LLVMCreateEnumAttribute(ctx->context, attr_kind, 0));
        }
    }
    llvm_register_function(ctx, name, fn, fn_type, ctx->type_i1);
    return !ctx->has_error;
}

void
llvm_forward_declare_intent(ASTNode *node, LLVMGenCtx *ctx)
{
    const MIRRoutine *mir_routine;
    const char *name;
    LLVMTypeRef *param_types = NULL;
    LLVMTypeRef fn_type;
    LLVMValueRef fn;
    IntentBindingMetadataView binding_metadata = {0};
    size_t mir_binding_count = 0;
    size_t param_count = 0;
    bool mir_only_intent = false;
    size_t binding_count = 0;
    size_t involve_count = 0;
    size_t value_count = 0;
    ASTNode **bindings = NULL;
    ASTNode **involves = NULL;
    ASTNode **values = NULL;
    size_t intent_step_count = 0;
    bool mir_requires_routine = false;

    if (node == NULL || node->type != AST_INTENT_DECL || ctx == NULL)
        return;
    name = ast_intent_decl_name(node);
    if (name == NULL || llvm_lookup_function(ctx, name) != NULL)
        return;
    intent_step_count = ast_intent_decl_step_count(node);
    mir_requires_routine = llvm_active_has_mir(ctx) && intent_step_count > 0;
    mir_routine = llvm_find_mir_intent_routine(ctx, node);
    if (mir_requires_routine && mir_routine == NULL) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing intent routine for forward declaration '%s'",
            name != NULL ? name : "(anonymous)");
        return;
    }
    mir_only_intent = mir_routine != NULL;
    if (mir_routine != NULL) {
        mir_binding_count = llvm_collect_mir_intent_bindings(
            mir_routine, ctx, &binding_metadata);
    } else {
        bindings = ast_intent_decl_bindings(node, &binding_count);
        involves = ast_intent_decl_involves(node, &involve_count);
        values = ast_intent_decl_values(node, &value_count);
    }
    if (mir_only_intent) {
        for (size_t i = 0; i < mir_binding_count; i++) {
            if (!intent_binding_metadata_view_has_complete_row(
                    &binding_metadata, i)) {
                llvm_set_mir_inventory_missing(ctx,
                    "MIR-only LLVM path has incomplete ordered intent binding metadata for forward declaration '%s'",
                    name != NULL ? name : "(anonymous)");
                return;
            }
            if (!intent_binding_metadata_kind_is_supported(
                    intent_binding_metadata_view_kind_at(
                        &binding_metadata, i))) {
                llvm_set_mir_inventory_missing(ctx,
                    "MIR-only LLVM path has invalid ordered intent binding metadata for forward declaration '%s'",
                    name != NULL ? name : "(anonymous)");
                return;
            }
        }
    }
    param_count = mir_only_intent
        ? mir_binding_count
        : (binding_count > 0
            ? binding_count
            : (involve_count + value_count));

    if (param_count > 0) {
        param_types = pgy_arena_calloc(&ctx->scratch,
            param_count * sizeof(LLVMTypeRef));
        if (param_types == NULL) {
            llvm_set_error(ctx,
                "LLVM intent forward parameter allocation failed for '%s'",
                name != NULL ? name : "<anonymous>");
            return;
        }
        for (size_t i = 0; i < param_count; i++) {
            LLVMTypeRef pt = NULL;
            ASTNode *binding = mir_only_intent
                ? NULL
                : (binding_count > 0
                    ? (i < binding_count ? bindings[i] : NULL)
                    : (i < involve_count
                        ? involves[i]
                        : (i - involve_count < value_count
                            ? values[i - involve_count]
                            : NULL)));
            if (mir_only_intent
                && intent_binding_metadata_view_row_is_kind(
                    &binding_metadata, i, "participant")) {
                const char *type_name =
                    intent_binding_metadata_view_type_at(
                        &binding_metadata, i);
                pt = pergyra_type_to_llvm(ctx, type_name);
                if (ctx->has_error || pt == NULL)
                    return;
                if (llvm_type_name_uses_pointer_self(ctx, type_name))
                    pt = LLVMPointerType(pt, 0);
            } else if (binding != NULL && binding->type == AST_INTENT_INVOLVES) {
                const char *legacy_type_name =
                    llvm_forward_intent_involves_type_name(binding);
                if (legacy_type_name != NULL) {
                    pt = pergyra_type_to_llvm(ctx, legacy_type_name);
                    if (ctx->has_error || pt == NULL)
                        return;
                    if (llvm_intent_involves_uses_pointer_self(ctx, binding))
                        pt = LLVMPointerType(pt, 0);
                }
            } else if (mir_only_intent
                       && intent_binding_metadata_view_row_is_kind(
                           &binding_metadata, i, "value")) {
                const char *value_type_name =
                    intent_binding_metadata_view_type_at(
                        &binding_metadata, i);
                pt = pergyra_type_to_llvm(ctx, value_type_name);
                if (ctx->has_error || pt == NULL)
                    return;
            }
            /* Row 607: intent VALUE type is MIR-carrier-owned; the non-MIR AST
               value fallback is retired and fails closed via the guard below. */
            if (pt == NULL) {
                llvm_set_error_at_with_hints(ctx, node,
                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                    PGY_FIX_ANNOTATE_BINDING_TYPE,
                    "LLVM intent forward declaration for '%s' requires binding type metadata for parameter %zu",
                    name != NULL ? name : "<anonymous>",
                    i + 1);
                return;
            }
            param_types[i] = pt;
        }
    }

    fn_type = LLVMFunctionType(ctx->type_i1, param_types,
        (unsigned)param_count, 0);
    fn = LLVMAddFunction(ctx->module, name, fn_type);
    {
        unsigned attr_kind = LLVMGetEnumAttributeKindForName(
            "no-stack-arg-probe", 18);
        if (attr_kind != 0) {
            LLVMAddAttributeAtIndex(fn, LLVMAttributeFunctionIndex,
                LLVMCreateEnumAttribute(ctx->context, attr_kind, 0));
        }
    }
    llvm_register_function(ctx, name, fn, fn_type, ctx->type_i1);
}

void
llvm_forward_declare_intent_routines_from_inventory(
    LLVMGenCtx *ctx,
    const LLVMMIRRoutineInventory *inventory)
{
    if (ctx == NULL || inventory == NULL)
        return;

    for (size_t i = 0; i < inventory->count; i++) {
        const MIRRoutine *routine = llvm_routine_inventory_get(inventory, i);
        if (routine == NULL) {
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path has invalid intent routine inventory row");
            return;
        }
        if (!llvm_forward_declare_intent_from_mir_routine(ctx, routine)
            || ctx->has_error) {
            return;
        }
    }
}

#endif
