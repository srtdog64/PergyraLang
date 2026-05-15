/*
 * Copyright (c) 2025 Pergyra Language Project
 * LLVM backend type-name rendering and AST/Pergyra type mapping.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_backend.h"
#include "llvm_backend_type_map_internal.h"
#include "llvm_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <llvm-c/Core.h>

/* Forward declaration for type mapping (used by slot helpers) */
LLVMTypeRef pergyra_type_to_llvm(LLVMGenCtx *ctx, const char *type_name);

/* =================================================================
 * Pergyra type → LLVM type mapping
 * ================================================================= */

/* Resolve inner type for generic containers: "Result<Int>" → i32 */
static bool
llvm_required_constructed_arg_name_copy(LLVMGenCtx *ctx,
                                        const char *type_name,
                                        int arg_index,
                                        const char *container_name,
                                        char *out,
                                        size_t out_size)
{
    if (out == NULL || out_size == 0)
        return false;
    out[0] = '\0';

    if (!llvm_constructed_arg_name_copy(type_name, arg_index, out, out_size)) {
        if (ctx != NULL && !ctx->has_error) {
            llvm_set_error_with_hints(ctx,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "%s type argument %d is too long while lowering '%s'",
                container_name != NULL ? container_name : "generic type",
                arg_index + 1,
                type_name != NULL ? type_name : "<null>");
        }
        return false;
    }
    if (out[0] != '\0')
        return true;

    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_with_hints(ctx,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "%s requires explicit concrete type argument %d while lowering '%s'",
            container_name != NULL ? container_name : "generic type",
            arg_index + 1,
            type_name != NULL ? type_name : "<null>");
    }
    return false;
}

LLVMTypeRef
llvm_resolve_inner_type(LLVMGenCtx *ctx, const char *type_name)
{
    char inner_buf[256];
    LLVMTypeRef resolved;

    if (!llvm_required_constructed_arg_name_copy(ctx, type_name, 0,
            "Option<T>", inner_buf, sizeof(inner_buf))) {
        return NULL;
    }
    resolved = pergyra_type_to_llvm(ctx, inner_buf);
    if (resolved == NULL && ctx != NULL && !ctx->has_error) {
        llvm_set_error_with_hints(ctx,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "Option<T>: cannot resolve concrete inner type metadata");
    }
    return resolved;
}

static ASTNode *
llvm_generic_default_from_params(GenericParams *params, const char *type_name)
{
    if (params == NULL || type_name == NULL)
        return NULL;
    for (size_t i = 0; i < params->count; i++) {
        GenericParam *param = params->params[i];
        if (param == NULL || param->name == NULL)
            continue;
        if (strcmp(param->name, type_name) == 0) {
            if (param->default_type != NULL)
                return param->default_type;
            if (param->constraint != NULL)
                return param->constraint;
            return NULL;
        }
    }
    return NULL;
}

static ASTNode *
llvm_generic_default_from_decl(ASTNode *decl, const char *type_name)
{
    if (decl == NULL || type_name == NULL)
        return NULL;

    switch (decl->type) {
    case AST_FUNC_DECL:
        return llvm_generic_default_from_params(
            ast_func_generic_params(decl), type_name);
    case AST_CLASS_DECL:
        return llvm_generic_default_from_params(
            ast_class_generic_params(decl), type_name);
    case AST_ABILITY_DECL:
        return llvm_generic_default_from_params(
            ast_ability_generic_params(decl), type_name);
    case AST_ROLE_DECL:
        return llvm_generic_default_from_params(
            ast_role_generic_params(decl), type_name);
    case AST_PARTY_DECL:
        return llvm_generic_default_from_params(
            ast_party_generic_params(decl), type_name);
    case AST_ROSTER_DECL:
        return llvm_generic_default_from_params(
            ast_roster_generic_params(decl), type_name);
    default:
        return NULL;
    }
}

static ASTNode *
llvm_find_generic_default_in_inventory(LLVMGenCtx *ctx, const char *type_name)
{
    ASTNode *resolved = NULL;
    ASTNode *candidate = NULL;
    ASTNodeType decl_types[] = {
        AST_FUNC_DECL,
        AST_CLASS_DECL,
        AST_ABILITY_DECL,
        AST_ROLE_DECL,
        AST_PARTY_DECL,
        AST_ROSTER_DECL
    };

    if (ctx == NULL || type_name == NULL)
        return NULL;

    resolved = llvm_generic_default_from_decl(ctx->current_host_decl, type_name);
    if (resolved != NULL)
        return resolved;

    for (size_t kind = 0; kind < sizeof(decl_types) / sizeof(decl_types[0]); kind++) {
        ASTNode **nodes = NULL;
        size_t count = 0;
        llvm_active_inventory(ctx, decl_types[kind], &nodes, &count);
        for (size_t i = 0; i < count; i++) {
            resolved = llvm_generic_default_from_decl(nodes != NULL ? nodes[i] : NULL,
                                                      type_name);
            if (resolved == NULL)
                continue;
            if (candidate == NULL) {
                candidate = resolved;
                continue;
            }
            {
                char *candidate_name =
                    llvm_render_type_name_scratch(candidate, &ctx->scratch);
                char *resolved_name =
                    llvm_render_type_name_scratch(resolved, &ctx->scratch);
                if (candidate_name == NULL || resolved_name == NULL
                    || strcmp(candidate_name, resolved_name) != 0)
                    return NULL;
            }
        }
    }

    return candidate;
}

static LLVMTypeRef
llvm_resolve_alias_type(LLVMGenCtx *ctx, const char *type_name)
{
    ASTNode *alias_decl;

    if (ctx == NULL || type_name == NULL)
        return NULL;

    alias_decl = llvm_find_decl_in_active_inventory(ctx, AST_TYPE_ALIAS, type_name);
    if (alias_decl == NULL || ast_type_alias_target_type(alias_decl) == NULL)
        return NULL;

    return ast_type_to_llvm(ctx, ast_type_alias_target_type(alias_decl));
}

static LLVMTypeRef
llvm_resolve_generic_formal_default(LLVMGenCtx *ctx, const char *type_name)
{
    ASTNode *default_type;

    if (ctx == NULL || type_name == NULL)
        return NULL;

    default_type = llvm_find_generic_default_in_inventory(ctx, type_name);
    if (default_type == NULL)
        return NULL;

    return ast_type_to_llvm(ctx, default_type);
}

LLVMTypeRef
pergyra_type_to_llvm(LLVMGenCtx *ctx, const char *type_name)
{
    if (type_name == NULL)
        return ctx->type_void;

    if (strcmp(type_name, "PgyError") == 0)
        return ctx->type_i8ptr;

    if (strncmp(type_name, "List<", 5) == 0) {
        char inner_buf[256];
        if (!llvm_required_constructed_arg_name_copy(ctx, type_name, 0,
                "List<T>", inner_buf, sizeof(inner_buf))) {
            return NULL;
        }
        return llvm_list_struct_type(ctx, inner_buf);
    }
    if (strncmp(type_name, "Set<", 4) == 0) {
        char inner_buf[256];
        if (!llvm_required_constructed_arg_name_copy(ctx, type_name, 0,
                "Set<T>", inner_buf, sizeof(inner_buf))) {
            return NULL;
        }
        return llvm_set_struct_type(ctx, inner_buf);
    }
    if (strncmp(type_name, "Queue<", 6) == 0) {
        char inner_buf[256];
        if (!llvm_required_constructed_arg_name_copy(ctx, type_name, 0,
                "Queue<T>", inner_buf, sizeof(inner_buf))) {
            return NULL;
        }
        return llvm_queue_struct_type(ctx, inner_buf);
    }
    if (strncmp(type_name, "HashMap<", 8) == 0) {
        char value_buf[256];
        if (!llvm_required_constructed_arg_name_copy(ctx, type_name, 1,
                "HashMap<K, V>", value_buf, sizeof(value_buf))) {
            return NULL;
        }
        return llvm_hashmap_struct_type(ctx, value_buf);
    }

    /* Check active type substitution (monomorphization) first */
    for (int i = 0; i < ctx->type_subst_count; i++) {
        if (strcmp(type_name, ctx->type_subst[i].param_name) == 0)
            return ctx->type_subst[i].llvm_type;
    }

    PgyTypeKind kind = pgy_classify_type(type_name);

    /* Primitive types — direct mapping */
    LLVMTypeRef primitive = pgy_kind_to_llvm(ctx, kind);
    if (primitive != NULL)
        return primitive;

    {
        LLVMTypeRef alias_type = llvm_resolve_alias_type(ctx, type_name);
        if (alias_type != NULL)
            return alias_type;
    }

    {
        LLVMTypeRef generic_default =
            llvm_resolve_generic_formal_default(ctx, type_name);
        if (generic_default != NULL)
            return generic_default;
    }

    /* Generic container types */
    switch (kind) {
    case PGY_TK_RESULT: {
        char ok_name_buf[256]  = {0};
        char err_name_buf[256] = {0};
        (void)llvm_constructed_arg_name_copy(type_name, 0,
                                             ok_name_buf,
                                             sizeof(ok_name_buf));
        (void)llvm_constructed_arg_name_copy(type_name, 1,
                                             err_name_buf,
                                             sizeof(err_name_buf));
        const char *ok_name  = ok_name_buf[0]  != '\0' ? ok_name_buf  : NULL;
        const char *err_name = err_name_buf[0] != '\0' ? err_name_buf : NULL;

        /* Legacy single-arg Result<T> defaults err to PgyError (i8ptr).
         * Two-arg Result<T, E> routes through the named-struct cache so
         * Ok/Err builders and match destructuring share one layout. */
        if (ok_name != NULL && err_name != NULL) {
            LLVMResultSpecEntry *spec =
                llvm_ensure_result_type(ctx, ok_name, err_name);
            if (spec != NULL && spec->struct_ty != NULL)
                return spec->struct_ty;
            if (ctx != NULL && !ctx->has_error) {
                llvm_set_error_with_hints(ctx,
                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                    PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                    "Result<%s, %s>: cannot materialize named layout",
                    ok_name, err_name);
            }
            return NULL;
        }
        if (ok_name == NULL) {
            if (ctx != NULL && !ctx->has_error) {
                llvm_set_error_with_hints(ctx,
                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                    PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                    "Result<T>: concrete Ok type metadata is required");
            }
            return NULL;
        }
        LLVMTypeRef ok_ty  = pergyra_type_to_llvm(ctx, ok_name);
        LLVMTypeRef err_ty = pergyra_type_to_llvm(ctx,
            err_name != NULL ? err_name : "PgyError");
        if (ctx->has_error || ok_ty == NULL || err_ty == NULL)
            return NULL;
        LLVMTypeRef fields[] = {
            ctx->type_i32,
            ok_ty,
            err_ty
        };
        return LLVMStructTypeInContext(ctx->context, fields, 3, 0);
    }
    case PGY_TK_OPTION: {
        LLVMTypeRef inner = llvm_resolve_inner_type(ctx, type_name);
        if (ctx->has_error || inner == NULL)
            return NULL;
        LLVMTypeRef fields[] = { ctx->type_i32, inner };
        return LLVMStructTypeInContext(ctx->context, fields, 2, 0);
    }
    case PGY_TK_SLOT: {
        char inner_buf[256];
        if (!llvm_required_constructed_arg_name_copy(ctx, type_name, 0,
                "Slot<T>", inner_buf, sizeof(inner_buf))) {
            return NULL;
        }
        return llvm_slot_struct_type(ctx, inner_buf);
    }
    case PGY_TK_SECURE_SLOT: {
        char inner_buf[256];
        if (!llvm_required_constructed_arg_name_copy(ctx, type_name, 0,
                "SecureSlot<T>", inner_buf, sizeof(inner_buf))) {
            return NULL;
        }
        return llvm_secure_slot_struct_type(ctx, inner_buf);
    }
    case PGY_TK_DEVICE_SLOT: {
        char inner_buf[256];
        if (!llvm_required_constructed_arg_name_copy(ctx, type_name, 0,
                "DeviceSlot<T>", inner_buf, sizeof(inner_buf))) {
            return NULL;
        }
        return llvm_slot_struct_type(ctx, inner_buf);
    }
    case PGY_TK_REMOTE_FUTURE:
        return ctx->type_task_handle;
    case PGY_TK_ARRAY: {
        char inner_buf[256];
        if (!llvm_required_constructed_arg_name_copy(ctx, type_name, 0,
                "Array<T>", inner_buf, sizeof(inner_buf))) {
            return NULL;
        }
        return llvm_array_struct_type(ctx, inner_buf);
    }
    case PGY_TK_SLICE: {
        char inner_buf[256];
        if (!llvm_required_constructed_arg_name_copy(ctx, type_name, 0,
                "Slice<T>", inner_buf, sizeof(inner_buf))) {
            return NULL;
        }
        return llvm_slice_struct_type(ctx, inner_buf);
    }
    case PGY_TK_CHANNEL:
    case PGY_TK_BOX:
    case PGY_TK_RC:
    case PGY_TK_WEAK:
        return ctx->type_i8ptr;
    case PGY_TK_FUTURE:
        return ctx->type_task_handle;

    case PGY_TK_UNKNOWN:
    case PGY_TK_CLASS:
        break;
    default:
        break;
    }

    /* Check if it's a registered class type */
    LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, type_name);
    if (cls != NULL)
        return cls->struct_type;

    if (llvm_find_enum_decl(ctx, type_name) != NULL)
        return ctx->type_i32;

    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_with_hints(ctx,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "LLVM type '%s' is not registered in the LLVM type map; silent i32 fallback is not allowed",
            type_name);
    }
    return NULL;
}
#endif /* PGY_LLVM_ENABLED */
