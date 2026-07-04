/*
 * Copyright (c) 2026 Pergyra Language Project
 * Shared C backend generic call binding queries.
 */

#include "transpiler_generic_binding_query.h"

#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "transpiler_expr_stdlib_collection_support.h"
#include "transpiler_generic_param_query.h"
#include "transpiler_type_render.h"

static int
transpiler_find_generic_param_index(ASTNode *decl, const char *name)
{
    GenericParams *generic_params;
    size_t generic_count;

    if (!transpiler_func_has_generic_params(decl) || name == NULL)
        return -1;

    generic_params = ast_declaration_generic_params(decl);
    generic_count = ast_generic_param_count(generic_params);
    for (size_t i = 0; i < generic_count; i++) {
        GenericParam *param = ast_generic_param_at(generic_params, i);
        if (ast_generic_param_name(param) != NULL
            && strcmp(ast_generic_param_name(param), name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

/* Split the top-level angle-bracket argument list of a rendered type name
 * ("Option<Int>", "Result<Int, String>") into depth-0 segments, verifying
 * the head name first. Returns the segment count, or -1 when the text is
 * not `head<...>` consuming the whole string. */
static int
transpiler_type_text_split_args(const char *text, const char *head,
                                const char **seg_start, size_t *seg_len,
                                size_t max_segs)
{
    size_t head_len;
    const char *p;
    const char *start;
    int depth = 0;
    int count = 0;

    if (text == NULL || head == NULL)
        return -1;
    head_len = strlen(head);
    if (strncmp(text, head, head_len) != 0 || text[head_len] != '<')
        return -1;

    p = text + head_len + 1;
    start = p;
    for (; *p != '\0'; p++) {
        if (*p == '<') {
            depth++;
        } else if (*p == '>') {
            if (depth == 0)
                break;
            depth--;
        } else if (*p == ',' && depth == 0) {
            if ((size_t)count >= max_segs)
                return -1;
            seg_start[count] = start;
            seg_len[count] = (size_t)(p - start);
            count++;
            start = p + 1;
            while (*start == ' ')
                start++;
            p = start - 1;
        }
    }
    if (*p != '>' || p[1] != '\0')
        return -1;
    if ((size_t)count >= max_segs)
        return -1;
    seg_start[count] = start;
    seg_len[count] = (size_t)(p - start);
    count++;
    return count;
}

/* G-2 structural matching: walk a declaration parameter type (AST, possibly
 * constructed over the declaration's generic parameters) in parallel with a
 * call argument's rendered type text, binding every bare generic-parameter
 * position ("Option<T>" vs "Option<Int>" binds T=Int; recursion covers
 * nesting). A conflicting rebinding fails (unification duty, docs/151 §8
 * G-2). Shape mismatches extract nothing — full type agreement stays the
 * semantic layer's job. */
static bool
transpiler_match_param_type_against_arg_text(ASTNode *decl,
                                             const ASTNode *param_type,
                                             const char *arg_text,
                                             GenericBindingEntry *bindings)
{
    const char *tname;
    GenericParams *args;
    size_t count;
    int generic_index;

    if (param_type == NULL || arg_text == NULL
        || param_type->type != AST_TYPE)
        return true;
    tname = ast_type_name(param_type);
    if (tname == NULL)
        return true;

    args = ast_type_generic_args(param_type);
    if (args == NULL || ast_generic_param_count(args) == 0) {
        generic_index = transpiler_find_generic_param_index(decl, tname);
        if (generic_index < 0)
            return true;
        if (bindings[generic_index].concrete_type[0] != '\0'
            && strcmp(bindings[generic_index].concrete_type, arg_text) != 0)
            return false;
        pergyra_str_copy(bindings[generic_index].concrete_type,
            sizeof(bindings[generic_index].concrete_type), arg_text);
        return true;
    }

    {
        const char *seg_start[MAX_GENERIC_BINDINGS];
        size_t seg_len[MAX_GENERIC_BINDINGS];
        int segs = transpiler_type_text_split_args(arg_text, tname,
            seg_start, seg_len, MAX_GENERIC_BINDINGS);

        count = ast_generic_param_count(args);
        if (segs < 0 || (size_t)segs != count)
            return true;

        for (size_t i = 0; i < count; i++) {
            char seg_buf[128];
            GenericParam *gp = ast_generic_param_at(args, i);
            ASTNode *sub = ast_generic_param_constraint(gp);
            size_t len = seg_len[i];

            while (len > 0 && seg_start[i][len - 1] == ' ')
                len--;
            if (len == 0 || len >= sizeof(seg_buf))
                return true;
            memcpy(seg_buf, seg_start[i], len);
            seg_buf[len] = '\0';

            if (sub != NULL) {
                if (!transpiler_match_param_type_against_arg_text(
                        decl, sub, seg_buf, bindings))
                    return false;
            } else if (ast_generic_param_name(gp) != NULL) {
                generic_index = transpiler_find_generic_param_index(
                    decl, ast_generic_param_name(gp));
                if (generic_index < 0)
                    continue;
                if (bindings[generic_index].concrete_type[0] != '\0'
                    && strcmp(bindings[generic_index].concrete_type,
                        seg_buf) != 0)
                    return false;
                pergyra_str_copy(bindings[generic_index].concrete_type,
                    sizeof(bindings[generic_index].concrete_type), seg_buf);
            }
        }
    }
    return true;
}

bool
transpiler_infer_generic_call_bindings(TranspilerCtx *ctx,
                                       ASTNode *decl,
                                       ASTNode *call,
                                       GenericBindingEntry *bindings,
                                       size_t *binding_count)
{
    GenericParams *generic_params;
    size_t generic_count;

    if (!transpiler_func_has_generic_params(decl)
        || call == NULL
        || call->type != AST_CALL
        || bindings == NULL
        || binding_count == NULL) {
        return false;
    }

    generic_params = ast_declaration_generic_params(decl);
    generic_count = ast_generic_param_count(generic_params);
    memset(bindings, 0, sizeof(GenericBindingEntry) * generic_count);

    for (size_t i = 0; i < generic_count; i++) {
        GenericParam *param = ast_generic_param_at(generic_params, i);
        if (ast_generic_param_name(param) != NULL) {
            pergyra_str_copy(bindings[i].name,
                sizeof(bindings[i].name), ast_generic_param_name(param));
        }
    }

    for (size_t i = 0; i < ast_func_param_count(decl)
        && i < ast_call_arg_count(call); i++) {
        FuncParam *param = ast_func_param(decl, i);
        const char *arg_type;

        if (param == NULL || param->type == NULL
            || param->type->type != AST_TYPE) {
            continue;
        }

        arg_type = transpiler_expr_infer_type_name(ctx,
            ast_call_argument(call, i));
        if (arg_type == NULL || arg_type[0] == '\0'
            || strcmp(arg_type, "Unknown") == 0)
            continue;

        /* Bare T and constructed-over-T params share one matcher; a
         * conflicting rebinding across arguments is a unification
         * failure and fails the whole inference. */
        if (!transpiler_match_param_type_against_arg_text(decl,
                param->type, arg_type, bindings))
            return false;
    }

    for (size_t i = 0; i < generic_count; i++) {
        if (bindings[i].name[0] == '\0')
            return false;
        if (bindings[i].concrete_type[0] == '\0') {
            /* Unbound by arguments: fall back to the declared default
             * type argument (<T = Int>), which the surface accepts and
             * validates but call-site binding previously ignored. */
            GenericParam *gp = ast_generic_param_at(generic_params, i);
            ASTNode *dflt = gp != NULL
                ? ast_generic_param_default_type(gp) : NULL;
            char *rendered = dflt != NULL
                ? render_type_name_in_ctx(ctx, dflt) : NULL;
            if (rendered == NULL)
                return false;
            pergyra_str_copy(bindings[i].concrete_type,
                sizeof(bindings[i].concrete_type), rendered);
            free(rendered);
        }
    }

    *binding_count = generic_count;
    return true;
}

TranspilerGenericBindingSnapshot
transpiler_generic_binding_snapshot(TranspilerCtx *ctx)
{
    TranspilerGenericBindingSnapshot snapshot;

    snapshot.binding_count = ctx != NULL ? ctx->generic_binding_count : 0;
    return snapshot;
}

void
transpiler_generic_binding_restore(
    TranspilerCtx *ctx,
    TranspilerGenericBindingSnapshot snapshot)
{
    if (ctx == NULL)
        return;
    ctx->generic_binding_count = snapshot.binding_count;
}

char *
transpiler_render_type_name_with_bindings(TranspilerCtx *ctx,
                                          ASTNode *type_node,
                                          GenericBindingEntry *bindings,
                                          size_t binding_count)
{
    TranspilerGenericBindingSnapshot snapshot;
    char *result;

    if (ctx == NULL)
        return NULL;

    snapshot = transpiler_generic_binding_snapshot(ctx);
    for (size_t i = 0;
        i < binding_count && ctx->generic_binding_count < MAX_GENERIC_BINDINGS;
        i++) {
        ctx->generic_bindings[ctx->generic_binding_count++] = bindings[i];
    }

    result = render_type_name_in_ctx(ctx, type_node);
    transpiler_generic_binding_restore(ctx, snapshot);
    return result;
}
