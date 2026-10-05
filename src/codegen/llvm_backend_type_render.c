/*
 * Copyright (c) 2025 Pergyra Language Project
 * LLVM backend type-name rendering and constructed type argument parsing.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_backend_type_map_internal.h"
#include "llvm_internal_api.h"
#include "llvm_inventory_decl_lookup.h"
#include "llvm_inventory_internal.h"
#include "../common/string_compat.h"
#include "../compiler/mir_decl_headers.h"

#include <string.h>

const char *
llvm_keep_rendered_persistent(LLVMGenCtx *ctx, char *rendered,
                              const char *oom_context)
{
    size_t len;
    char *copy;

    if (rendered == NULL)
        return NULL;
    if (ctx == NULL) {
        free(rendered);
        return NULL;
    }

    len = strlen(rendered);
    copy = pgy_arena_alloc(&ctx->persistent, len + 1);
    if (copy == NULL) {
        if (!ctx->has_error) {
            llvm_set_error(ctx, "%s",
                oom_context != NULL ? oom_context
                                    : "out of memory copying LLVM type text");
        }
        free(rendered);
        return NULL;
    }
    memcpy(copy, rendered, len + 1);
    free(rendered);
    return copy;
}

static bool
llvm_constructed_arg_name_write(const char *type_name, int arg_index,
                                char *out, size_t out_size)
{
    const char *lt;
    const char *p;
    int current = 0;

    if (out == NULL || out_size == 0)
        return false;
    out[0] = '\0';
    if (type_name == NULL || arg_index < 0)
        return false;
    lt = strchr(type_name, '<');
    if (lt == NULL)
        return false;

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
            if (len == 0 || len >= out_size)
                return false;
            memcpy(out, start, len);
            out[len] = '\0';
            return true;
        }
        if (*p == ',')
            p++;
        while (*p == ' ')
            p++;
        current++;
    }

    return false;
}

bool
llvm_constructed_arg_name_copy(const char *type_name, int arg_index,
                               char *out, size_t out_size)
{
    return llvm_constructed_arg_name_write(type_name, arg_index, out, out_size);
}

char *
llvm_render_alias_target_type_name_from_headers(LLVMGenCtx *ctx,
                                                const char *type_name,
                                                PgyArena *arena)
{
    const char *current = type_name;

    if (ctx == NULL || type_name == NULL || arena == NULL)
        return NULL;

    for (size_t depth = 0; depth < 32; depth++) {
        const MIRDeclHeader *alias_header =
            llvm_find_decl_header_in_context_of_type(
                ctx, AST_TYPE_ALIAS, current);
        const char *target_type_name =
            mir_decl_header_type_alias_target_type_name(alias_header);

        if (target_type_name == NULL) {
            if (llvm_active_has_mir(ctx) && alias_header != NULL) {
                llvm_set_error_with_hints(ctx,
                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                    PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                    "MIR-only LLVM type-name render missing type-alias target metadata for '%s'",
                    current);
                return NULL;
            }
            return depth == 0 ? NULL : pgy_arena_strdup(arena, current);
        }
        current = target_type_name;
        if (strchr(current, '<') != NULL || strchr(current, '(') != NULL)
            return pgy_arena_strdup(arena, current);
    }

    return pgy_arena_strdup(arena, current);
}

/* `type Gold = Int` is transparent, so a rendered name such as Array<Gold>
 * or Result<Gold, E> names Array<Int> / Result<Int, E>: one LLVM struct, not
 * a second one with the same layout. Replaces every alias token (also inside
 * an alias target) and returns the arena copy, or type_name when no alias
 * occurs. */
const char *
llvm_type_name_resolve_aliases(LLVMGenCtx *ctx, const char *type_name)
{
    char buffers[2][256];
    const char *current = type_name;
    bool any_change = false;

    if (ctx == NULL || type_name == NULL)
        return type_name;
    if (ctx->type_alias_presence == 0) {
        LLVMMIRDeclHeaderInventory inventory;
        llvm_active_decl_header_inventory(ctx, &inventory);
        ctx->type_alias_presence = 1;
        for (size_t i = 0; i < inventory.count; i++) {
            if (inventory.headers[i].ast_type == AST_TYPE_ALIAS) {
                ctx->type_alias_presence = 2;
                break;
            }
        }
    }
    if (ctx->type_alias_presence == 1)
        return type_name;

    for (size_t pass = 0; pass < 8; pass++) {
        char *out = buffers[pass % 2];
        size_t oi = 0;
        bool changed = false;
        for (size_t i = 0; current[i] != '\0';) {
            char c = current[i];
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_') {
                size_t start = i;
                char tok[128];
                const char *target = NULL;
                while ((current[i] >= 'A' && current[i] <= 'Z')
                    || (current[i] >= 'a' && current[i] <= 'z')
                    || (current[i] >= '0' && current[i] <= '9')
                    || current[i] == '_')
                    i++;
                size_t len = i - start;
                if (len < sizeof(tok)) {
                    memcpy(tok, current + start, len);
                    tok[len] = '\0';
                    target = mir_decl_header_type_alias_target_type_name(
                        llvm_find_decl_header_in_context_of_type(
                            ctx, AST_TYPE_ALIAS, tok));
                }
                const char *rep = target != NULL ? target : current + start;
                size_t rep_len = target != NULL ? strlen(target) : len;
                if (oi + rep_len >= sizeof(buffers[0]))
                    return type_name;
                memcpy(out + oi, rep, rep_len);
                oi += rep_len;
                changed = changed || target != NULL;
            } else {
                if (oi + 1 >= sizeof(buffers[0]))
                    return type_name;
                out[oi++] = c;
                i++;
            }
        }
        out[oi] = '\0';
        if (!changed)
            break;
        any_change = true;
        current = out;
    }
    if (!any_change)
        return type_name;
    return pgy_arena_strdup(&ctx->scratch, current);
}

char *
llvm_render_type_name(ASTNode *type_node)
{
    return llvm_render_type_name_in_ctx(NULL, type_node);
}

char *
llvm_render_type_name_in_ctx(LLVMGenCtx *ctx, ASTNode *type_node)
{
    PgyArena arena;
    char *result;

    pgy_arena_init_named(&arena, 0, "llvm-type-render-scratch");
    result = llvm_render_type_name_scratch_in_ctx(ctx, type_node, &arena);
    result = result != NULL ? pergyra_strdup(result) : NULL;
    pgy_arena_destroy(&arena);
    return result;
}

char *
llvm_render_type_name_scratch(ASTNode *type_node, PgyArena *arena)
{
    return llvm_render_type_name_scratch_in_ctx(NULL, type_node, arena);
}

char *
llvm_render_type_name_scratch_in_ctx(LLVMGenCtx *ctx, ASTNode *type_node,
                                     PgyArena *arena)
{
    ASTNode *alias_decl = NULL;

    if (type_node == NULL)
        return pgy_arena_strdup(arena, "Void");
    if (ast_type_name(type_node) == NULL)
        return NULL;
    GenericParams *generic_args = ast_type_generic_args(type_node);
    size_t generic_count = ast_generic_param_count(generic_args);
    if (generic_count == 0) {
        char *alias_target_type_name = NULL;
        ASTNode **types = NULL;
        size_t type_count = 0;
        if (ctx != NULL) {
            alias_target_type_name =
                llvm_render_alias_target_type_name_from_headers(
                    ctx, ast_type_name(type_node), arena);
        }
        if (alias_target_type_name != NULL)
            return alias_target_type_name;
        if (ctx != NULL && ctx->has_error)
            return NULL;
        if (ctx != NULL && llvm_active_has_mir(ctx))
            return pgy_arena_strdup(arena, ast_type_name(type_node));
        if (ctx != NULL) {
            llvm_active_inventory(ctx, AST_TYPE_ALIAS, &types, &type_count);
        }
        if (types != NULL) {
            for (size_t i = 0; i < type_count; i++) {
                ASTNode *stmt = types[i];
                if (stmt != NULL && stmt->type == AST_TYPE_ALIAS
                    && ast_type_alias_name(stmt) != NULL
                    && strcmp(ast_type_alias_name(stmt), ast_type_name(type_node)) == 0) {
                    alias_decl = stmt;
                    break;
                }
            }
        }
        if (alias_decl != NULL && ast_type_alias_target_type(alias_decl) != NULL)
            return llvm_render_type_name_scratch_in_ctx(
                ctx, ast_type_alias_target_type(alias_decl), arena);
        return pgy_arena_strdup(arena, ast_type_name(type_node));
    }

    char *result = pgy_arena_strdup(arena, ast_type_name(type_node));
    if (result == NULL)
        return NULL;
    for (size_t i = 0; i < generic_count; i++) {
        GenericParam *gp = ast_generic_param_at(generic_args, i);
        char *arg_name = NULL;
        char *grown;
        size_t need;

        if (gp == NULL)
            return NULL;
        if (ast_generic_param_constraint(gp) != NULL)
            arg_name = llvm_render_type_name_scratch_in_ctx(
                ctx, ast_generic_param_constraint(gp), arena);
        else if (ast_generic_param_name(gp) != NULL)
            arg_name = pgy_arena_strdup(arena, ast_generic_param_name(gp));
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
