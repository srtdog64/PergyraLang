/*
 * Copyright (c) 2025 Pergyra Language Project
 * LLVM backend type-name rendering and constructed type argument parsing.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_backend_type_map_internal.h"
#include "llvm_inventory_decl_lookup.h"
#include "../common/string_compat.h"

#include <string.h>

static LLVMGenCtx *g_llvm_type_render_ctx = NULL;

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
                return NULL;
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

char *
llvm_render_type_name(ASTNode *type_node)
{
    PgyArena arena;
    char *result;

    pgy_arena_init(&arena, 0);
    result = llvm_render_type_name_scratch(type_node, &arena);
    result = result != NULL ? pergyra_strdup(result) : NULL;
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
    if (result == NULL)
        return NULL;
    for (size_t i = 0; i < type_node->data.type.generic_args->count; i++) {
        GenericParam *gp = type_node->data.type.generic_args->params[i];
        char *arg_name = NULL;
        char *grown;
        size_t need;

        if (gp == NULL)
            return NULL;
        if (gp->constraint != NULL)
            arg_name = llvm_render_type_name_scratch(gp->constraint, arena);
        else if (gp->name != NULL)
            arg_name = pgy_arena_strdup(arena, gp->name);
        else
            return NULL;
        if (arg_name == NULL || arg_name[0] == '\0')
            return NULL;

        {
            size_t result_len = strlen(result);
            size_t arg_len = strlen(arg_name);
            if (arg_len > ((size_t)-1) - result_len - 4)
                return NULL;
            need = result_len + arg_len + 4;
        }
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
        if (cur_len > ((size_t)-1) - 2)
            return NULL;
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

#endif /* PGY_LLVM_ENABLED */
