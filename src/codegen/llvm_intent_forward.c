/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM backend MIR-backed intent forward declarations.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_intent_internal.h"

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

void
llvm_forward_declare_intent(ASTNode *node, LLVMGenCtx *ctx)
{
    const MIRRoutine *mir_routine;
    const char *name;
    LLVMTypeRef *param_types = NULL;
    LLVMTypeRef fn_type;
    LLVMValueRef fn;
    const char **participant_aliases = NULL;
    const char **participant_types = NULL;
    const char **value_aliases = NULL;
    const char **value_types = NULL;
    size_t participant_count = 0;
    size_t mir_value_count = 0;
    size_t param_count = 0;
    bool mir_only_intent = false;
    size_t binding_count = 0;
    size_t involve_count = 0;
    size_t value_count = 0;
    size_t step_count = 0;
    ASTNode **bindings = NULL;
    ASTNode **involves = NULL;
    ASTNode **values = NULL;

    if (node == NULL || node->type != AST_INTENT_DECL || ctx == NULL)
        return;
    name = ast_intent_decl_name(node);
    if (name == NULL || llvm_lookup_function(ctx, name) != NULL)
        return;
    bindings = ast_intent_decl_bindings(node, &binding_count);
    involves = ast_intent_decl_involves(node, &involve_count);
    values = ast_intent_decl_values(node, &value_count);
    step_count = ast_intent_decl_step_count(node);
    mir_routine = llvm_find_mir_intent_routine(ctx, node);
    if (mir_routine != NULL) {
        participant_count = llvm_collect_mir_intent_participants(
            mir_routine, ctx, &participant_aliases, &participant_types);
        mir_value_count = llvm_collect_mir_intent_values(
            mir_routine, ctx, &value_aliases, &value_types);
    }
    mir_only_intent = llvm_active_has_mir(ctx) && step_count > 0;
    if (mir_only_intent && involve_count > 0) {
        if (participant_count < involve_count) {
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path missing intent participant metadata for '%s'",
                name != NULL ? name : "(anonymous)");
            return;
        }
        for (size_t i = 0; i < involve_count; i++) {
            if (participant_aliases == NULL || participant_types == NULL
                || participant_aliases[i] == NULL || participant_types[i] == NULL) {
                llvm_set_mir_inventory_missing(ctx,
                    "MIR-only LLVM path has incomplete intent participant metadata for '%s'",
                    name != NULL ? name : "(anonymous)");
                return;
            }
        }
    }
    if (mir_only_intent && value_count > 0) {
        if (mir_value_count < value_count) {
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path missing intent value metadata for '%s'",
                name != NULL ? name : "(anonymous)");
            return;
        }
        for (size_t i = 0; i < value_count; i++) {
            if (value_aliases == NULL || value_types == NULL
                || value_aliases[i] == NULL || value_types[i] == NULL) {
                llvm_set_mir_inventory_missing(ctx,
                    "MIR-only LLVM path has incomplete intent value metadata for '%s'",
                    name != NULL ? name : "(anonymous)");
                return;
            }
        }
    }
    param_count = binding_count > 0
        ? binding_count
        : (involve_count + value_count);
    if (participant_count == 0)
        participant_count = involve_count;
    if (param_count == 0)
        param_count = participant_count;

    if (param_count > 0) {
        size_t participant_index = 0;
        size_t value_index = 0;
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
            ASTNode *binding = binding_count > 0
                ? (i < binding_count ? bindings[i] : NULL)
                : (i < involve_count
                    ? involves[i]
                    : (i - involve_count < value_count
                        ? values[i - involve_count]
                        : NULL));
            if (binding != NULL && binding->type == AST_INTENT_INVOLVES) {
                const char *type_name =
                    (participant_types != NULL
                     && participant_index < participant_count)
                        ? participant_types[participant_index]
                        : llvm_forward_intent_involves_type_name(binding);
                if (type_name != NULL) {
                    pt = pergyra_type_to_llvm(ctx, type_name);
                    if (ctx->has_error || pt == NULL)
                        return;
                    if (llvm_type_name_uses_pointer_self(ctx, type_name))
                        pt = LLVMPointerType(pt, 0);
                } else if (!mir_only_intent) {
                    ASTNode *subject_type =
                        ast_intent_involves_subject_type(binding);
                    if (subject_type != NULL) {
                        pt = ast_type_to_llvm(ctx, subject_type);
                        if (ctx->has_error || pt == NULL)
                            return;
                        if (llvm_intent_involves_uses_pointer_self(ctx, binding))
                            pt = LLVMPointerType(pt, 0);
                    }
                }
                if (pt == NULL && mir_only_intent) {
                    llvm_set_mir_inventory_missing(ctx,
                        "MIR-only LLVM path missing intent participant type metadata for '%s'",
                        name != NULL ? name : "(anonymous)");
                    return;
                }
                participant_index++;
            } else if (binding != NULL && binding->type == AST_INTENT_VALUE) {
                const char *value_type_name =
                    (value_types != NULL && value_index < mir_value_count)
                        ? value_types[value_index]
                        : NULL;
                if (value_type_name != NULL) {
                    pt = pergyra_type_to_llvm(ctx, value_type_name);
                    if (ctx->has_error || pt == NULL)
                        return;
                } else {
                    ASTNode *value_type = ast_intent_value_type(binding);
                    if (value_type != NULL) {
                        pt = ast_type_to_llvm(ctx, value_type);
                        if (ctx->has_error || pt == NULL)
                            return;
                    }
                }
                if (pt == NULL && mir_only_intent) {
                    llvm_set_mir_inventory_missing(ctx,
                        "MIR-only LLVM path missing intent value type metadata for '%s'",
                        name != NULL ? name : "(anonymous)");
                    return;
                }
                value_index++;
            } else if (mir_only_intent) {
                llvm_set_mir_inventory_missing(ctx,
                    "MIR-only LLVM path missing intent parameter metadata for '%s'",
                    name != NULL ? name : "(anonymous)");
                return;
            }
            if (pt == NULL)
                pt = ctx->type_i32;
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
        ASTNode *intent_decl = NULL;
        if (!llvm_require_mir_intent_source_ast(ctx, routine, &intent_decl))
            return;
        if (intent_decl == NULL)
            continue;
        llvm_forward_declare_intent(intent_decl, ctx);
        if (ctx->has_error)
            return;
    }
}

#endif
