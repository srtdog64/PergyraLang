/*
 * Copyright (c) 2025 Pergyra Language Project
 * LLVM backend type-name rendering and AST/Pergyra type mapping.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_backend.h"
#include "llvm_internal.h"
#include "../common/string_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <llvm-c/Core.h>

static LLVMGenCtx *g_llvm_type_render_ctx = NULL;

/* Forward declaration for type mapping (used by slot helpers) */
LLVMTypeRef pergyra_type_to_llvm(LLVMGenCtx *ctx, const char *type_name);
static bool llvm_can_forward_declare_type_early(LLVMGenCtx *ctx, ASTNode *type_node);
bool llvm_can_forward_declare_func_early(LLVMGenCtx *ctx, ASTNode *func);

static char *llvm_render_type_name(ASTNode *type_node);
static char *llvm_render_type_name_scratch(ASTNode *type_node, PgyArena *arena);

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

static char *
llvm_copy_first_constructed_arg_name(LLVMGenCtx *ctx, const char *type_name)
{
    if (ctx == NULL || type_name == NULL)
        return NULL;

    const char *lt = strchr(type_name, '<');
    const char *gt = strrchr(type_name, '>');
    if (lt == NULL || gt == NULL || gt <= lt + 1)
        return NULL;

    size_t len = (size_t)(gt - lt - 1);
    char *copy = pgy_arena_alloc(&ctx->persistent, len + 1);
    if (copy == NULL)
        return NULL;
    memcpy(copy, lt + 1, len);
    copy[len] = '\0';
    return copy;
}

void
llvm_register_typed_var(LLVMGenCtx *ctx, const char *var_name,
                        ASTNode *type_node)
{
    const char *type_name;

    if (ctx == NULL || var_name == NULL || type_node == NULL)
        return;

    if (type_node->type == AST_EVENT_HANDLER_TYPE) {
        llvm_register_callable_var(ctx, var_name, type_node);
        return;
    }

    if (type_node->type != AST_TYPE || type_node->data.type.name == NULL)
        return;

    type_name = type_node->data.type.name;

    if ((strcmp(type_name, "Array") == 0 || strcmp(type_name, "Slice") == 0)
        && type_node->data.type.generic_args != NULL
        && type_node->data.type.generic_args->count > 0
        && type_node->data.type.generic_args->params[0] != NULL
        && type_node->data.type.generic_args->params[0]->constraint != NULL) {
        char *elem_name = llvm_render_type_name_scratch(
            type_node->data.type.generic_args->params[0]->constraint,
            &ctx->scratch);
        LLVMTypeRef elem_type = pergyra_type_to_llvm(ctx, elem_name);
        llvm_register_array_var(ctx, var_name, elem_type, -1);
    }

    if (strcmp(type_name, "List") == 0
        && type_node->data.type.generic_args != NULL
        && type_node->data.type.generic_args->count > 0
        && type_node->data.type.generic_args->params[0] != NULL
        && type_node->data.type.generic_args->params[0]->constraint != NULL) {
        char *inner_name = llvm_render_type_name(
            type_node->data.type.generic_args->params[0]->constraint);
        llvm_register_list_var(ctx, var_name, inner_name);
        return;
    }

    if (strcmp(type_name, "Set") == 0
        && type_node->data.type.generic_args != NULL
        && type_node->data.type.generic_args->count > 0
        && type_node->data.type.generic_args->params[0] != NULL
        && type_node->data.type.generic_args->params[0]->constraint != NULL) {
        char *inner_name = llvm_render_type_name(
            type_node->data.type.generic_args->params[0]->constraint);
        llvm_register_set_var(ctx, var_name, inner_name);
        return;
    }

    if (strcmp(type_name, "Queue") == 0
        && type_node->data.type.generic_args != NULL
        && type_node->data.type.generic_args->count > 0
        && type_node->data.type.generic_args->params[0] != NULL
        && type_node->data.type.generic_args->params[0]->constraint != NULL) {
        char *inner_name = llvm_render_type_name(
            type_node->data.type.generic_args->params[0]->constraint);
        llvm_register_queue_var(ctx, var_name, inner_name);
        return;
    }

    if (strcmp(type_name, "HashMap") == 0
        && type_node->data.type.generic_args != NULL
        && type_node->data.type.generic_args->count > 1
        && type_node->data.type.generic_args->params[0] != NULL
        && type_node->data.type.generic_args->params[0]->constraint != NULL
        && type_node->data.type.generic_args->params[1] != NULL
        && type_node->data.type.generic_args->params[1]->constraint != NULL) {
        char *key_name = llvm_render_type_name(
            type_node->data.type.generic_args->params[0]->constraint);
        char *value_name = llvm_render_type_name(
            type_node->data.type.generic_args->params[1]->constraint);
        llvm_register_map_var(ctx, var_name, key_name, value_name);
        return;
    }

    if ((strcmp(type_name, "Future") == 0 || strcmp(type_name, "RemoteFuture") == 0)
        && type_node->data.type.generic_args != NULL
        && type_node->data.type.generic_args->count > 0
        && type_node->data.type.generic_args->params[0] != NULL
        && type_node->data.type.generic_args->params[0]->constraint != NULL) {
        char *inner_name = llvm_render_type_name(
            type_node->data.type.generic_args->params[0]->constraint);
        llvm_register_future_var(ctx, var_name, inner_name,
            strcmp(type_name, "RemoteFuture") == 0);
        return;
    }

    if (strcmp(type_name, "Channel") == 0
        && type_node->data.type.generic_args != NULL
        && type_node->data.type.generic_args->count > 0
        && type_node->data.type.generic_args->params[0] != NULL
        && type_node->data.type.generic_args->params[0]->constraint != NULL) {
        char *inner_name = llvm_render_type_name(
            type_node->data.type.generic_args->params[0]->constraint);
        llvm_register_channel_var(ctx, var_name, inner_name);
        return;
    }

    if (strcmp(type_name, "Rc") == 0 || strcmp(type_name, "Weak") == 0
        || strncmp(type_name, "Rc<", 3) == 0
        || strncmp(type_name, "Weak<", 5) == 0) {
        char *inner_name = NULL;
        if (type_node->data.type.generic_args != NULL
            && type_node->data.type.generic_args->count > 0
            && type_node->data.type.generic_args->params[0] != NULL
            && type_node->data.type.generic_args->params[0]->constraint != NULL) {
            inner_name = llvm_render_type_name(
                type_node->data.type.generic_args->params[0]->constraint);
        } else {
            inner_name = llvm_copy_first_constructed_arg_name(ctx, type_name);
        }
        if (inner_name == NULL)
            return;
        if (strcmp(type_name, "Rc") == 0 || strncmp(type_name, "Rc<", 3) == 0)
            llvm_register_rc_var(ctx, var_name, inner_name);
        else
            llvm_register_weak_var(ctx, var_name, inner_name);
        return;
    }

    if (llvm_lookup_class(ctx, type_name) != NULL
        || llvm_find_enum_decl(ctx, type_name) != NULL)
        llvm_register_var_class(ctx, var_name, type_name);
}

/* =================================================================
 * Pergyra type → LLVM type mapping
 * ================================================================= */

static const char *
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

static char *
llvm_render_type_name(ASTNode *type_node)
{
    PgyArena arena;
    char *result;

    pgy_arena_init(&arena, 0);
    result = llvm_render_type_name_scratch(type_node, &arena);
    result = result != NULL ? pergyra_strdup(result) : pergyra_strdup("Int");
    pgy_arena_destroy(&arena);
    return result;
}

static char *
llvm_render_type_name_scratch(ASTNode *type_node, PgyArena *arena)
{
    ASTNode *alias_decl = NULL;

    if (type_node == NULL)
        return pgy_arena_strdup(arena, "Void");
    if (type_node->type != AST_TYPE || type_node->data.type.name == NULL)
        return pgy_arena_strdup(arena, "Int");
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
            arg_name = pgy_arena_strdup(arena, "Int");

        need = strlen(result) + strlen(arg_name) + 4;
        grown = (char *)pgy_arena_alloc(arena, need);
        if (grown == NULL)
            return pgy_arena_strdup(arena, "Int");
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
            return pgy_arena_strdup(arena, "Int");
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
    /* Find the inner type name between < and > */
    const char *lt = strchr(type_name, '<');
    const char *gt = strrchr(type_name, '>');
    if (lt == NULL || gt == NULL || gt <= lt)
        return ctx->type_i32;

    char inner[128];
    size_t len = (size_t)(gt - lt - 1);
    if (len >= sizeof(inner)) len = sizeof(inner) - 1;
    memcpy(inner, lt + 1, len);
    inner[len] = '\0';

    LLVMTypeRef resolved = pgy_kind_to_llvm(ctx, pgy_classify_type(inner));
    return resolved != NULL ? resolved : ctx->type_i32;
}

LLVMTypeRef
pergyra_type_to_llvm(LLVMGenCtx *ctx, const char *type_name)
{
    if (type_name == NULL)
        return ctx->type_void;

    if (strncmp(type_name, "List<", 5) == 0) {
        const char *inner = llvm_constructed_arg_name_at(type_name, 0);
        return llvm_list_struct_type(ctx, inner != NULL ? inner : "Int");
    }
    if (strncmp(type_name, "Set<", 4) == 0) {
        const char *inner = llvm_constructed_arg_name_at(type_name, 0);
        return llvm_set_struct_type(ctx, inner != NULL ? inner : "Int");
    }
    if (strncmp(type_name, "Queue<", 6) == 0) {
        const char *inner = llvm_constructed_arg_name_at(type_name, 0);
        return llvm_queue_struct_type(ctx, inner != NULL ? inner : "Int");
    }
    if (strncmp(type_name, "HashMap<", 8) == 0) {
        const char *value = llvm_constructed_arg_name_at(type_name, 1);
        return llvm_hashmap_struct_type(ctx, value != NULL ? value : "Int");
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
            /* fall through to legacy anonymous layout if resolution fails */
        }
        LLVMTypeRef ok_ty  = pergyra_type_to_llvm(ctx,
            ok_name  != NULL ? ok_name  : "Int");
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
        const char *inner_name = strchr(type_name, '<');
        if (inner_name != NULL) {
            inner_name++;
            char buf[64]; size_t l = strcspn(inner_name, ">");
            if (l >= sizeof(buf)) l = sizeof(buf) - 1;
            memcpy(buf, inner_name, l); buf[l] = '\0';
            return llvm_slot_struct_type(ctx, buf);
        }
        return llvm_slot_struct_type(ctx, "Int");
    }
    case PGY_TK_SECURE_SLOT: {
        const char *inner_name = strchr(type_name, '<');
        if (inner_name != NULL) {
            inner_name++;
            char buf[64]; size_t l = strcspn(inner_name, ">");
            if (l >= sizeof(buf)) l = sizeof(buf) - 1;
            memcpy(buf, inner_name, l); buf[l] = '\0';
            return llvm_secure_slot_struct_type(ctx, buf);
        }
        return llvm_secure_slot_struct_type(ctx, "Int");
    }
    case PGY_TK_DEVICE_SLOT: {
        const char *inner_name = strchr(type_name, '<');
        if (inner_name != NULL) {
            inner_name++;
            char buf[64]; size_t l = strcspn(inner_name, ">");
            if (l >= sizeof(buf)) l = sizeof(buf) - 1;
            memcpy(buf, inner_name, l); buf[l] = '\0';
            return llvm_slot_struct_type(ctx, buf);
        }
        return llvm_slot_struct_type(ctx, "Int");
    }
    case PGY_TK_REMOTE_FUTURE:
        return ctx->type_task_handle;
    case PGY_TK_ARRAY: {
        const char *inner_name = strchr(type_name, '<');
        if (inner_name != NULL) {
            inner_name++;
            char buf[64]; size_t l = strcspn(inner_name, ">");
            if (l >= sizeof(buf)) l = sizeof(buf) - 1;
            memcpy(buf, inner_name, l); buf[l] = '\0';
            return llvm_array_struct_type(ctx, buf);
        }
        return llvm_array_struct_type(ctx, "Int");
    }
    case PGY_TK_SLICE: {
        const char *inner_name = strchr(type_name, '<');
        if (inner_name != NULL) {
            inner_name++;
            char buf[64]; size_t l = strcspn(inner_name, ">");
            if (l >= sizeof(buf)) l = sizeof(buf) - 1;
            memcpy(buf, inner_name, l); buf[l] = '\0';
            return llvm_slice_struct_type(ctx, buf);
        }
        return llvm_slice_struct_type(ctx, "Int");
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

    return ctx->type_i32;
}

LLVMTypeRef
ast_type_to_llvm(LLVMGenCtx *ctx, ASTNode *type_node)
{
    if (type_node == NULL)
        return ctx->type_void;

    if (type_node->type == AST_EVENT_HANDLER_TYPE) {
        size_t param_count = type_node->data.event_handler_type.param_count;
        LLVMTypeRef *param_types = NULL;
        LLVMTypeRef ret_type = ctx->type_void;
        LLVMTypeRef fn_type;

        if (type_node->data.event_handler_type.return_type != NULL)
            ret_type = ast_type_to_llvm(ctx,
                type_node->data.event_handler_type.return_type);

        if (param_count > 0) {
            /* Param-type buffer is consumed by LLVMFunctionType (which
             * copies contents) and never retained by the caller. */
            param_types = pgy_arena_calloc(&ctx->scratch,
                param_count * sizeof(LLVMTypeRef));
            if (param_types == NULL)
                return LLVMPointerType(LLVMFunctionType(ret_type, NULL, 0, 0), 0);
            for (size_t i = 0; i < param_count; i++) {
                param_types[i] = ast_type_to_llvm(ctx,
                    type_node->data.event_handler_type.param_types[i]);
            }
        }

        fn_type = LLVMFunctionType(ret_type, param_types, (unsigned)param_count, 0);
        /* param_types is ctx->scratch-owned. */
        return LLVMPointerType(fn_type, 0);
    }

    /* Tuple type: anonymous struct { T0, T1, ... } */
    if (type_node->type == AST_TYPE
        && type_node->data.type.tuple_elements != NULL
        && type_node->data.type.tuple_element_count > 0) {
        size_t n = type_node->data.type.tuple_element_count;
        /* Field-type buffer is consumed by LLVMStructTypeInContext (copies). */
        LLVMTypeRef *fields = pgy_arena_calloc(&ctx->scratch,
            n * sizeof(LLVMTypeRef));
        if (fields == NULL)
            return ctx->type_i32;
        for (size_t i = 0; i < n; i++)
            fields[i] = ast_type_to_llvm(ctx,
                type_node->data.type.tuple_elements[i]);
        LLVMTypeRef result = LLVMStructTypeInContext(ctx->context, fields,
            (unsigned)n, 0);
        /* fields is ctx->scratch-owned. */
        return result;
    }

    if (type_node->type == AST_TYPE && type_node->data.type.name != NULL) {
        char *full_name = llvm_render_type_name_scratch(type_node, &ctx->scratch);
        LLVMTypeRef resolved = pergyra_type_to_llvm(ctx, full_name);
        return resolved;
    }

    return ctx->type_i32;
}

#include "llvm_backend_forward_declare.h"
#endif /* PGY_LLVM_ENABLED */
