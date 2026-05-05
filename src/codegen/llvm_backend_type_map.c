/*
 * Copyright (c) 2025 Pergyra Language Project
 * LLVM backend type-name rendering and AST/Pergyra type mapping.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_backend.h"
#include "llvm_backend_type_map_internal.h"
#include "llvm_internal.h"
#include "../common/string_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <llvm-c/Core.h>

static LLVMGenCtx *g_llvm_type_render_ctx = NULL;

/* Forward declaration for type mapping (used by slot helpers) */
LLVMTypeRef pergyra_type_to_llvm(LLVMGenCtx *ctx, const char *type_name);

void
llvm_set_type_render_ctx(LLVMGenCtx *ctx)
{
    g_llvm_type_render_ctx = ctx;
}

void
llvm_clear_type_render_ctx_if(LLVMGenCtx *ctx)
{
    if (g_llvm_type_render_ctx == ctx)
        g_llvm_type_render_ctx = NULL;
}

/* =================================================================
 * Pergyra type → LLVM type mapping
 * ================================================================= */

const char *
llvm_constructed_arg_name_at(const char *type_name, int arg_index)
{
    static char arg_buf[256];
    const char *lt;
    const char *p;
    int current = 0;

    if (type_name == NULL || arg_index < 0)
        return NULL;
    lt = strchr(type_name, '<');
    if (lt == NULL)
        return NULL;

    p = lt + 1;
    while (*p != '\0' && *p != '>') {
        const char *start = p;
        int depth = 0;
        size_t len;
        while (*p != '\0') {
            if (*p == '<')
                depth++;
            else if (*p == '>') {
                if (depth == 0)
                    break;
                depth--;
            } else if (*p == ',' && depth == 0) {
                break;
            }
            p++;
        }
        if (current == arg_index) {
            while (*start == ' ')
                start++;
            while (p > start && p[-1] == ' ')
                p--;
            len = (size_t)(p - start);
            if (len >= sizeof(arg_buf))
                len = sizeof(arg_buf) - 1;
            memcpy(arg_buf, start, len);
            arg_buf[len] = '\0';
            return arg_buf;
        }
        if (*p == ',')
            p++;
        while (*p == ' ')
            p++;
        current++;
    }

    return NULL;
}

static const char *
llvm_required_constructed_arg_name_at(LLVMGenCtx *ctx, const char *type_name,
                                      int arg_index, const char *container_name)
{
    const char *arg = llvm_constructed_arg_name_at(type_name, arg_index);
    if (arg != NULL && arg[0] != '\0')
        return arg;
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
    return "Unknown";
}

char *
llvm_render_type_name(ASTNode *type_node)
{
    PgyArena arena;
    char *result;

    pgy_arena_init(&arena, 0);
    result = llvm_render_type_name_scratch(type_node, &arena);
    result = result != NULL ? pergyra_strdup(result) : pergyra_strdup("Unknown");
    pgy_arena_destroy(&arena);
    return result;
}

char *
llvm_render_type_name_scratch(ASTNode *type_node, PgyArena *arena)
{
    ASTNode *alias_decl = NULL;

    if (type_node == NULL)
        return pgy_arena_strdup(arena, "Void");
    if (type_node->type != AST_TYPE || type_node->data.type.name == NULL)
        return NULL;
    if (type_node->data.type.generic_args == NULL
        || type_node->data.type.generic_args->count == 0) {
        ASTNode **types = NULL;
        size_t type_count = 0;
        if (g_llvm_type_render_ctx != NULL) {
            llvm_active_inventory(g_llvm_type_render_ctx, AST_TYPE_ALIAS, &types,
                                  &type_count);
        }
        if (types != NULL) {
            for (size_t i = 0; i < type_count; i++) {
                ASTNode *stmt = types[i];
                if (stmt != NULL && stmt->type == AST_TYPE_ALIAS
                    && stmt->data.type_alias.name != NULL
                    && strcmp(stmt->data.type_alias.name, type_node->data.type.name) == 0) {
                    alias_decl = stmt;
                    break;
                }
            }
        }
        if (alias_decl != NULL && alias_decl->data.type_alias.target_type != NULL)
            return llvm_render_type_name_scratch(alias_decl->data.type_alias.target_type, arena);
        return pgy_arena_strdup(arena, type_node->data.type.name);
    }

    char *result = pgy_arena_strdup(arena, type_node->data.type.name);
    for (size_t i = 0; i < type_node->data.type.generic_args->count; i++) {
        GenericParam *gp = type_node->data.type.generic_args->params[i];
        char *arg_name = NULL;
        char *grown;
        size_t need;

        if (gp == NULL)
            continue;
        if (gp->constraint != NULL)
            arg_name = llvm_render_type_name_scratch(gp->constraint, arena);
        else if (gp->name != NULL)
            arg_name = pgy_arena_strdup(arena, gp->name);
        else
            return NULL;
        if (arg_name == NULL || arg_name[0] == '\0')
            return NULL;

        need = strlen(result) + strlen(arg_name) + 4;
        grown = (char *)pgy_arena_alloc(arena, need);
        if (grown == NULL)
            return NULL;
        memcpy(grown, result, strlen(result) + 1);
        result = grown;
        {
            size_t offset = strlen(result);
            if (i == 0) {
                result[offset++] = '<';
            } else {
                result[offset++] = ',';
                result[offset++] = ' ';
            }
            {
                size_t arg_len = strlen(arg_name);
                memcpy(result + offset, arg_name, arg_len);
                offset += arg_len;
            }
            result[offset] = '\0';
        }
    }

    {
        size_t cur_len = strlen(result);
        char *grown = (char *)pgy_arena_alloc(arena, cur_len + 2);
        if (grown == NULL)
            return NULL;
        memcpy(grown, result, cur_len + 1);
        result = grown;
        result[cur_len] = '>';
        result[cur_len + 1] = '\0';
    }
    return result;
}

/* Resolve inner type for generic containers: "Result<Int>" → i32 */
LLVMTypeRef
llvm_resolve_inner_type(LLVMGenCtx *ctx, const char *type_name)
{
    const char *inner = llvm_required_constructed_arg_name_at(ctx, type_name, 0,
        "Option<T>");
    LLVMTypeRef resolved = pergyra_type_to_llvm(ctx, inner);
    if (resolved == NULL && ctx != NULL && !ctx->has_error) {
        llvm_set_error_with_hints(ctx,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "Option<T>: cannot resolve concrete inner type metadata");
    }
    return resolved != NULL ? resolved : ctx->type_i32;
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
            decl->data.func_decl.generic_params, type_name);
    case AST_CLASS_DECL:
        return llvm_generic_default_from_params(
            decl->data.class_decl.generic_params, type_name);
    case AST_ABILITY_DECL:
        return llvm_generic_default_from_params(
            decl->data.ability_decl.generic_params, type_name);
    case AST_ROLE_DECL:
        return llvm_generic_default_from_params(
            decl->data.role_decl.generic_params, type_name);
    case AST_PARTY_DECL:
        return llvm_generic_default_from_params(
            decl->data.party_decl.generic_params, type_name);
    case AST_ROSTER_DECL:
        return llvm_generic_default_from_params(
            decl->data.roster_decl.generic_params, type_name);
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
    if (alias_decl == NULL || alias_decl->data.type_alias.target_type == NULL)
        return NULL;

    return ast_type_to_llvm(ctx, alias_decl->data.type_alias.target_type);
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
        const char *inner = llvm_required_constructed_arg_name_at(ctx, type_name, 0,
            "List<T>");
        return llvm_list_struct_type(ctx, inner);
    }
    if (strncmp(type_name, "Set<", 4) == 0) {
        const char *inner = llvm_required_constructed_arg_name_at(ctx, type_name, 0,
            "Set<T>");
        return llvm_set_struct_type(ctx, inner);
    }
    if (strncmp(type_name, "Queue<", 6) == 0) {
        const char *inner = llvm_required_constructed_arg_name_at(ctx, type_name, 0,
            "Queue<T>");
        return llvm_queue_struct_type(ctx, inner);
    }
    if (strncmp(type_name, "HashMap<", 8) == 0) {
        const char *value = llvm_required_constructed_arg_name_at(ctx, type_name, 1,
            "HashMap<K, V>");
        return llvm_hashmap_struct_type(ctx, value);
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
        /* llvm_constructed_arg_name_at returns a pointer into a static
         * scratch buffer; copy each arg immediately before the next call
         * clobbers it. */
        char ok_name_buf[128]  = {0};
        char err_name_buf[128] = {0};
        const char *ok_tmp  = llvm_constructed_arg_name_at(type_name, 0);
        if (ok_tmp != NULL)
            snprintf(ok_name_buf, sizeof(ok_name_buf), "%s", ok_tmp);
        const char *err_tmp = llvm_constructed_arg_name_at(type_name, 1);
        if (err_tmp != NULL)
            snprintf(err_name_buf, sizeof(err_name_buf), "%s", err_tmp);
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
            return ctx != NULL ? ctx->type_i32 : NULL;
        }
        if (ok_name == NULL) {
            if (ctx != NULL && !ctx->has_error) {
                llvm_set_error_with_hints(ctx,
                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                    PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                    "Result<T>: concrete Ok type metadata is required");
            }
            return ctx != NULL ? ctx->type_i32 : NULL;
        }
        LLVMTypeRef ok_ty  = pergyra_type_to_llvm(ctx, ok_name);
        LLVMTypeRef err_ty = pergyra_type_to_llvm(ctx,
            err_name != NULL ? err_name : "PgyError");
        LLVMTypeRef fields[] = {
            ctx->type_i32,
            ok_ty  != NULL ? ok_ty  : ctx->type_i32,
            err_ty != NULL ? err_ty : ctx->type_i8ptr
        };
        return LLVMStructTypeInContext(ctx->context, fields, 3, 0);
    }
    case PGY_TK_OPTION: {
        LLVMTypeRef inner = llvm_resolve_inner_type(ctx, type_name);
        LLVMTypeRef fields[] = { ctx->type_i32, inner };
        return LLVMStructTypeInContext(ctx->context, fields, 2, 0);
    }
    case PGY_TK_SLOT: {
        const char *inner = llvm_required_constructed_arg_name_at(ctx, type_name, 0,
            "Slot<T>");
        return llvm_slot_struct_type(ctx, inner);
    }
    case PGY_TK_SECURE_SLOT: {
        const char *inner = llvm_required_constructed_arg_name_at(ctx, type_name, 0,
            "SecureSlot<T>");
        return llvm_secure_slot_struct_type(ctx, inner);
    }
    case PGY_TK_DEVICE_SLOT: {
        const char *inner = llvm_required_constructed_arg_name_at(ctx, type_name, 0,
            "DeviceSlot<T>");
        return llvm_slot_struct_type(ctx, inner);
    }
    case PGY_TK_REMOTE_FUTURE:
        return ctx->type_task_handle;
    case PGY_TK_ARRAY: {
        const char *inner = llvm_required_constructed_arg_name_at(ctx, type_name, 0,
            "Array<T>");
        return llvm_array_struct_type(ctx, inner);
    }
    case PGY_TK_SLICE: {
        const char *inner = llvm_required_constructed_arg_name_at(ctx, type_name, 0,
            "Slice<T>");
        return llvm_slice_struct_type(ctx, inner);
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
    return ctx->type_i32;
}
#endif /* PGY_LLVM_ENABLED */
