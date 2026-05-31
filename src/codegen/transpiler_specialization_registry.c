/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend generated type/collection specialization registry owner.
 */

#include "transpiler_specialization_registry.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_type_mapping.h"
#include "transpiler_type_render.h"
#include "transpiler_type_require.h"

static bool
transpiler_specialization_copy_spec_name(char *dst, size_t dst_size,
                                         const char *value)
{
    size_t len;

    if (dst == NULL || dst_size == 0 || value == NULL)
        return false;

    len = strlen(value);
    if (len >= dst_size)
        return false;

    memcpy(dst, value, len + 1);
    return true;
}

static bool
transpiler_specialization_append_spec_text(char *dst, size_t dst_size,
                                           const char *value)
{
    size_t used;
    size_t len;

    if (dst == NULL || dst_size == 0 || value == NULL)
        return false;

    used = strlen(dst);
    len = strlen(value);
    if (used >= dst_size || len >= dst_size - used)
        return false;

    memcpy(dst + used, value, len + 1);
    return true;
}

static void
transpiler_specialization_spec_name_too_long(TranspilerCtx *ctx,
                                             const char *surface)
{
    transpiler_set_backend_error_with_hints(
        ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
        "generated specialization name is too long while lowering %s",
        surface != NULL ? surface : "collection specialization");
}

static void
ensure_option_specialization_to(TranspilerCtx *ctx, CodeBuf *dst,
                                const char *inner_type)
{
    if (ctx == NULL || dst == NULL || inner_type == NULL)
        return;

    if (strcmp(inner_type, "Void") == 0) {
        transpiler_set_backend_error_with_hints(
            ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "Option<Void> does not lower until the Option<Void> ABI is frozen");
        return;
    }

    if (strcmp(inner_type, "Int") == 0
        || strcmp(inner_type, "Bool") == 0
        || strcmp(inner_type, "String") == 0) {
        return;
    }

    char suffix[128];
    sanitize_c_suffix(inner_type, suffix, sizeof(suffix));

    for (int i = 0; i < ctx->option_spec_count; i++) {
        if (strcmp(ctx->option_specs_suffix[i], suffix) == 0)
            return;
    }
    if (ctx->option_spec_count >= 32) {
        transpiler_set_backend_error(
            ctx,
            "too many Option<T> specializations in one translation unit; limit is %d while lowering Option<%s>",
            32, inner_type);
        return;
    }

    char ctype_buf[128];
    if (!transpiler_copy_c_type_or_user_type_name(inner_type, ctype_buf,
            sizeof(ctype_buf))) {
        transpiler_specialization_spec_name_too_long(ctx, inner_type);
        return;
    }

    if (!transpiler_specialization_copy_spec_name(
            ctx->option_specs_suffix[ctx->option_spec_count],
            sizeof(ctx->option_specs_suffix[0]), suffix)
        || !transpiler_specialization_copy_spec_name(
            ctx->option_specs_inner_ctype[ctx->option_spec_count],
            sizeof(ctx->option_specs_inner_ctype[0]), ctype_buf)) {
        transpiler_specialization_spec_name_too_long(ctx, inner_type);
        return;
    }
    ctx->option_spec_count++;

    codebuf_write(dst,
        "\n/* PGY_OPTION_%s */\n"
        "#pragma GCC diagnostic push\n"
        "#pragma GCC diagnostic ignored \"-Wunused-function\"\n"
        "PGY_OPTION_DEFINE(%s, %s)\n"
        "#define Some_%s(...)           pgy_option_some_%s(__VA_ARGS__)\n"
        "#define None_%s()              pgy_option_none_%s()\n"
        "#define IsSome_%s(o)           ((o).tag == PgyOptionSome)\n"
        "#define IsNone_%s(o)           ((o).tag == PgyOptionNone)\n"
        "#define UnwrapOption_%s(o)     pgy_option_unwrap_%s(&(PgyOption_%s){(o).tag, (o).value})\n"
        "#pragma GCC diagnostic pop\n",
        suffix,
        suffix, ctype_buf,
        suffix, suffix,
        suffix, suffix,
        suffix,
        suffix,
        suffix, suffix, suffix);
}

void
ensure_result_specialization_to(TranspilerCtx *ctx, CodeBuf *dst,
                                const char *ok_type, const char *err_type)
{
    if (ctx == NULL || dst == NULL || ok_type == NULL || err_type == NULL)
        return;

    if (strcmp(ok_type, "Void") == 0) {
        transpiler_set_backend_error_with_hints(
            ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "Result<Void, %s> does not lower until the Result<Void> ABI is frozen",
            err_type);
        return;
    }

    if (strcmp(err_type, "PgyError") == 0 || strcmp(err_type, "String") == 0) {
        if (strcmp(ok_type, "Int") == 0
            || strcmp(ok_type, "Bool") == 0
            || strcmp(ok_type, "String") == 0) {
            return;
        }
    }

    char ok_suffix[128];
    char err_suffix[128];
    sanitize_c_suffix(ok_type, ok_suffix, sizeof(ok_suffix));
    sanitize_c_suffix(err_type, err_suffix, sizeof(err_suffix));

    char combined[256];
    {
        size_t ok_len = strlen(ok_suffix);
        size_t err_len = strlen(err_suffix);
        if (ok_len + 1 + err_len >= sizeof(combined)) {
            transpiler_specialization_spec_name_too_long(ctx, ok_type);
            return;
        }
        memcpy(combined, ok_suffix, ok_len);
        combined[ok_len++] = '_';
        memcpy(combined + ok_len, err_suffix, err_len);
        combined[ok_len + err_len] = '\0';
    }

    for (int i = 0; i < ctx->result_spec_count; i++) {
        if (strcmp(ctx->result_specs_suffix[i], combined) == 0)
            return;
    }

    if (ctx->result_spec_count >= 32) {
        transpiler_set_backend_error(
            ctx,
            "too many custom Result<T, E> specializations in one translation unit; limit is %d while lowering Result<%s, %s>",
            32,
            ok_type,
            err_type);
        return;
    }

    char ok_ctype_buf[128];
    char err_ctype_buf[128];
    const char *ok_ctype = ok_ctype_buf;
    const char *err_ctype = err_ctype_buf;
    if (!transpiler_copy_c_type_or_user_type_name(ok_type, ok_ctype_buf,
            sizeof(ok_ctype_buf))
        || !transpiler_copy_c_type_or_user_type_name(err_type, err_ctype_buf,
            sizeof(err_ctype_buf))) {
        transpiler_specialization_spec_name_too_long(ctx, ok_type);
        return;
    }

    if (!transpiler_specialization_copy_spec_name(
            ctx->result_specs_suffix[ctx->result_spec_count],
            sizeof(ctx->result_specs_suffix[0]), combined)
        || !transpiler_specialization_copy_spec_name(
            ctx->result_specs_ok_ctype[ctx->result_spec_count],
            sizeof(ctx->result_specs_ok_ctype[0]), ok_ctype)
        || !transpiler_specialization_copy_spec_name(
            ctx->result_specs_err_ctype[ctx->result_spec_count],
            sizeof(ctx->result_specs_err_ctype[0]), err_ctype)) {
        transpiler_specialization_spec_name_too_long(ctx, ok_type);
        return;
    }
    ctx->result_spec_count++;

    codebuf_write(dst,
        "\n/* PGY_RESULT_%s */\n"
        "#pragma GCC diagnostic push\n"
        "#pragma GCC diagnostic ignored \"-Wunused-function\"\n"
        "PGY_RESULT_DEFINE(%s, %s, %s)\n"
        "#define Ok_%s(...)        pgy_result_ok_%s(__VA_ARGS__)\n"
        "#define Err_%s(...)       pgy_result_err_%s(__VA_ARGS__)\n"
        "#define IsOk_%s(r)        ((r).tag == PgyResultOk)\n"
        "#define IsErr_%s(r)       ((r).tag == PgyResultErr)\n"
        "#define Unwrap_%s(r)      pgy_result_unwrap_%s(&(PgyResult_%s){(r).tag, {.ok=(r).ok}})\n"
        "#define UnwrapOr_%s(r, f) ((r).tag == PgyResultOk ? (r).ok : (f))\n"
        "#pragma GCC diagnostic pop\n",
        combined,
        combined, ok_ctype, err_ctype,
        combined, combined,
        combined, combined,
        combined,
        combined,
        combined, combined, combined,
        combined);
}

static void
ensure_collection_specialization_to(TranspilerCtx *ctx, CodeBuf *dst,
                                    const char *kind, const char *inner_type)
{
    char suffix[128];
    char ctype_buf[128];
    const char *ctype = ctype_buf;

    if (ctx == NULL || dst == NULL || kind == NULL || inner_type == NULL)
        return;

    if ((strcmp(kind, "List") == 0 || strcmp(kind, "Queue") == 0)
        && (strcmp(inner_type, "Int") == 0
            || strcmp(inner_type, "String") == 0)) {
        return;
    }
    if (strcmp(kind, "Map") == 0
        && (strcmp(inner_type, "Int") == 0
            || strcmp(inner_type, "String") == 0)) {
        return;
    }
    if (strcmp(kind, "Set") == 0
        && (strcmp(inner_type, "Int") == 0
            || strcmp(inner_type, "String") == 0)) {
        return;
    }

    sanitize_c_suffix(inner_type, suffix, sizeof(suffix));
    for (int i = 0; i < ctx->collection_spec_count; i++) {
        if (strcmp(ctx->collection_specs[i].kind, kind) == 0
            && strcmp(ctx->collection_specs[i].suffix, suffix) == 0) {
            return;
        }
    }

    if (ctx->collection_spec_count >= MAX_COLLECTION_SPECIALIZATIONS)
        return;

    if (!transpiler_require_type_name_c_type_copy(ctx, inner_type,
            "collection specialization element", ctype_buf,
            sizeof(ctype_buf)))
        return;

    if (!transpiler_specialization_copy_spec_name(
            ctx->collection_specs[ctx->collection_spec_count].kind,
            sizeof(ctx->collection_specs[ctx->collection_spec_count].kind),
            kind)
        || !transpiler_specialization_copy_spec_name(
            ctx->collection_specs[ctx->collection_spec_count].suffix,
            sizeof(ctx->collection_specs[ctx->collection_spec_count].suffix),
            suffix)) {
        transpiler_specialization_spec_name_too_long(ctx, inner_type);
        return;
    }
    ctx->collection_spec_count++;

    if (strcmp(kind, "List") == 0) {
        codebuf_write(dst,
            "\n/* PGY_COLLECTION_LIST_%s */\n"
            "PGY_LIST_DEFINE(%s, %s)\n",
            suffix, suffix, ctype);
    } else if (strcmp(kind, "Queue") == 0) {
        codebuf_write(dst,
            "\n/* PGY_COLLECTION_QUEUE_%s */\n"
            "PGY_QUEUE_DEFINE(%s, %s)\n",
            suffix, suffix, ctype);
    } else if (strcmp(kind, "Map") == 0) {
        codebuf_write(dst,
            "\n/* PGY_COLLECTION_MAP_%s */\n"
            "PGY_HASHMAP_DEFINE(%s, %s)\n"
            "PGY_DEFINE_MAP_KEYS_EXPORTS(%s, %s)\n",
            suffix, suffix, ctype, suffix, suffix);
    } else if (strcmp(kind, "Set") == 0) {
        codebuf_write(dst,
            "\n/* PGY_COLLECTION_SET_%s */\n"
            "PGY_SET_DEFINE(%s, %s)\n",
            suffix, suffix, ctype);
    }
}

void
ensure_collection_specialization(TranspilerCtx *ctx, const char *kind,
                                 const char *inner_type)
{
    ensure_collection_specialization_to(ctx,
        ctx != NULL ? ctx->decls : NULL, kind, inner_type);
}

static void
ensure_tuple_specialization_to(TranspilerCtx *ctx, CodeBuf *dst,
                               ASTNode *tuple_type)
{
    if (ctx == NULL || dst == NULL || tuple_type == NULL)
        return;
    if (ast_type_tuple_element_count(tuple_type) == 0)
        return;

    size_t n = ast_type_tuple_element_count(tuple_type);

    char suffix[256];
    char elem_names[512];
    suffix[0] = '\0';
    elem_names[0] = '\0';
    for (size_t i = 0; i < n; i++) {
        char *elem = render_type_name_in_ctx(
            ctx, ast_type_tuple_element(tuple_type, i));
        char sane[96];
        sanitize_c_suffix(elem, sane, sizeof(sane));
        if (i > 0) {
            if (!transpiler_specialization_append_spec_text(
                    suffix, sizeof(suffix), "_")
                || !transpiler_specialization_append_spec_text(
                    elem_names, sizeof(elem_names), " ")) {
                transpiler_specialization_spec_name_too_long(ctx, elem);
                free(elem);
                return;
            }
        }
        if (!transpiler_specialization_append_spec_text(
                suffix, sizeof(suffix), sane)
            || !transpiler_specialization_append_spec_text(
                elem_names, sizeof(elem_names), elem)) {
            transpiler_specialization_spec_name_too_long(ctx, elem);
            free(elem);
            return;
        }
        free(elem);
    }

    for (int i = 0; i < ctx->tuple_spec_count; i++) {
        if (strcmp(ctx->tuple_specs_suffix[i], suffix) == 0)
            return;
    }
    if (ctx->tuple_spec_count >= 32) {
        transpiler_set_backend_error_with_hints(
            ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "too many tuple specializations in one translation unit; limit is 32 while lowering (%s)",
            elem_names);
        return;
    }
    if (!transpiler_specialization_copy_spec_name(
            ctx->tuple_specs_suffix[ctx->tuple_spec_count],
            sizeof(ctx->tuple_specs_suffix[0]), suffix)
        || !transpiler_specialization_copy_spec_name(
            ctx->tuple_specs_elements[ctx->tuple_spec_count],
            sizeof(ctx->tuple_specs_elements[0]), elem_names)) {
        transpiler_specialization_spec_name_too_long(ctx, elem_names);
        return;
    }
    ctx->tuple_specs_arity[ctx->tuple_spec_count] = (int)n;
    ctx->tuple_spec_count++;

    codebuf_write(dst, "\n/* PGY_TUPLE_%s */\n", suffix);
    codebuf_write(dst, "typedef struct {\n");
    for (size_t i = 0; i < n; i++) {
        char *elem = render_type_name_in_ctx(
            ctx, ast_type_tuple_element(tuple_type, i));
        char ctype_buf[128];
        const char *ctype = ctype_buf;
        if (!transpiler_copy_c_type_or_user_type_name(elem, ctype_buf,
                sizeof(ctype_buf))) {
            transpiler_specialization_spec_name_too_long(ctx, elem);
            free(elem);
            return;
        }
        codebuf_write(dst, "    %s f%zu;\n",
            ctype != NULL ? ctype : elem, i);
        free(elem);
    }
    codebuf_write(dst, "} PgyTuple_%s_t;\n", suffix);
}

void
ensure_type_specializations_from_ast_to(TranspilerCtx *ctx, CodeBuf *dst,
                                        ASTNode *type_node)
{
    if (ctx == NULL || type_node == NULL)
        return;

    if (type_node->type == AST_EVENT_HANDLER_TYPE) {
        ASTNode *return_type = ast_event_handler_return_type(type_node);
        if (return_type != NULL)
            ensure_type_specializations_from_ast_to(ctx, dst, return_type);
        for (size_t i = 0; i < ast_event_handler_param_count(type_node); i++) {
            ensure_type_specializations_from_ast_to(ctx, dst,
                ast_event_handler_param_type(type_node, i));
        }
        return;
    }

    if (type_node->type != AST_TYPE || ast_type_name(type_node) == NULL)
        return;

    if (ast_type_tuple_element_count(type_node) > 0) {
        size_t element_count = ast_type_tuple_element_count(type_node);
        for (size_t i = 0; i < element_count; i++) {
            ensure_type_specializations_from_ast_to(ctx, dst,
                ast_type_tuple_element(type_node, i));
        }
        ensure_tuple_specialization_to(ctx, ctx->out, type_node);
        (void)dst;
        return;
    }

    {
        ASTNode *alias_decl = transpiler_find_type_alias_decl(
            ctx, ast_type_name(type_node));
        if (alias_decl != NULL
            && ast_type_alias_target_type(alias_decl) != NULL) {
            ensure_type_specializations_from_ast_to(ctx, dst,
                ast_type_alias_target_type(alias_decl));
            return;
        }
    }

    if (ast_type_generic_args(type_node) != NULL) {
        GenericParams *generic_args = ast_type_generic_args(type_node);
        size_t generic_count = ast_generic_param_count(generic_args);
        for (size_t i = 0; i < generic_count; i++) {
            GenericParam *arg = ast_generic_param_at(generic_args, i);
            ASTNode *constraint = ast_generic_param_constraint(arg);
            if (constraint != NULL)
                ensure_type_specializations_from_ast_to(ctx, dst, constraint);
        }
    }

    GenericParams *generic_args = ast_type_generic_args(type_node);
    ASTNode *arg0_type = ast_generic_param_constraint(
        ast_generic_param_at(generic_args, 0));
    ASTNode *arg1_type = ast_generic_param_constraint(
        ast_generic_param_at(generic_args, 1));

    if (strcmp(ast_type_name(type_node), "List") == 0
        && arg0_type != NULL) {
        char *inner = render_type_name_in_ctx(ctx, arg0_type);
        ensure_collection_specialization_to(ctx, dst, "List", inner);
        free(inner);
        return;
    }

    if (strcmp(ast_type_name(type_node), "Queue") == 0
        && arg0_type != NULL) {
        char *inner = render_type_name_in_ctx(ctx, arg0_type);
        ensure_collection_specialization_to(ctx, dst, "Queue", inner);
        free(inner);
        return;
    }

    if (strcmp(ast_type_name(type_node), "HashMap") == 0
        && arg1_type != NULL) {
        char *value = render_type_name_in_ctx(ctx, arg1_type);
        ensure_collection_specialization_to(ctx, dst, "Map", value);
        free(value);
        return;
    }

    if (strcmp(ast_type_name(type_node), "Result") == 0
        && ast_generic_param_count(generic_args) == 2
        && arg0_type != NULL
        && arg1_type != NULL) {
        char *ok_type  = render_type_name_in_ctx(ctx, arg0_type);
        char *err_type = render_type_name_in_ctx(ctx, arg1_type);
        ensure_result_specialization_to(ctx, ctx->out, ok_type, err_type);
        (void)dst;
        free(ok_type);
        free(err_type);
        return;
    }

    if (strcmp(ast_type_name(type_node), "Option") == 0
        && ast_generic_param_count(generic_args) == 1
        && arg0_type != NULL) {
        char *inner_type = render_type_name_in_ctx(ctx, arg0_type);
        ensure_option_specialization_to(ctx, ctx->out, inner_type);
        (void)dst;
        free(inner_type);
        return;
    }
}

void
ensure_type_specializations_from_ast(TranspilerCtx *ctx, ASTNode *type_node)
{
    ensure_type_specializations_from_ast_to(ctx,
        ctx != NULL ? ctx->decls : NULL, type_node);
}
