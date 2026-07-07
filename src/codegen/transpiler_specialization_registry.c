/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend generated type/collection specialization registry owner.
 */

#include "transpiler_specialization_registry.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"
#include "codegen_match_variant_policy.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_inventory_view.h"
#include "codegen_type_mapping.h"
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
                                           size_t *len_io,
                                           const char *value)
{
    size_t cur_len;
    size_t value_len;

    if (dst == NULL || dst_size == 0 || len_io == NULL || value == NULL)
        return false;

    cur_len = *len_io;
    if (cur_len >= dst_size)
        return false;

    value_len = strlen(value);
    if (value_len > dst_size - cur_len - 1)
        return false;

    memcpy(dst + cur_len, value, value_len);
    cur_len += value_len;
    dst[cur_len] = '\0';
    *len_io = cur_len;
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

/* A generic declaration's types reach this registry twice: once from the
 * eager declaration scan (no bindings active — inner still spelled "T")
 * and once from inside a specialization window (bindings active). Emitting
 * PGY_*_DEFINE(T, ...) from the eager pass hands gcc an unknown type, so:
 * substitute active bindings if any, then skip when the inner is still a
 * bare single-capital-letter name and no declaration inventory row owns that
 * name. Single-letter nominal types are legal fixtures (for example class P or
 * enum E), so declaration inventory must decide before the type-parameter
 * spelling heuristic fires. The bound re-scan inside the window owns the real
 * generic-param emission. */
static bool
transpiler_specialization_inner_is_unbound_param(TranspilerCtx *ctx,
                                                 const char **inner_io,
                                                 char *buf,
                                                 size_t buf_size)
{
    const char *applied;

    if (ctx == NULL || inner_io == NULL || *inner_io == NULL)
        return false;
    applied = transpiler_type_name_apply_generic_bindings(
        ctx, *inner_io, buf, buf_size);
    *inner_io = applied;
    if (applied[0] >= 'A' && applied[0] <= 'Z' && applied[1] == '\0'
        && transpiler_has_known_nominal_type(ctx, applied)) {
        return false;
    }
    return applied[0] >= 'A' && applied[0] <= 'Z' && applied[1] == '\0';
}

void
ensure_option_specialization_to(TranspilerCtx *ctx, CodeBuf *dst,
                                const char *inner_type)
{
    char inner_subst[128];
    const char *some_tag;
    const char *none_tag;
    const char *some_field;

    if (ctx == NULL || dst == NULL || inner_type == NULL)
        return;

    if (transpiler_specialization_inner_is_unbound_param(
            ctx, &inner_type, inner_subst, sizeof(inner_subst)))
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
        || strcmp(inner_type, "Float") == 0
        || strcmp(inner_type, "Double") == 0
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

    some_tag = pgy_codegen_match_variant_c_option_tag(PGY_MATCH_VARIANT_SOME);
    none_tag = pgy_codegen_match_variant_c_option_tag(PGY_MATCH_VARIANT_NONE_CTOR);
    some_field = pgy_codegen_match_variant_c_payload_field(PGY_MATCH_VARIANT_SOME);
    if (some_tag == NULL || none_tag == NULL || some_field == NULL) {
        transpiler_set_backend_error(ctx,
            "C Option<T> specialization requires complete Option variant ABI policy");
        return;
    }

    codebuf_write(dst,
        "\n/* PGY_OPTION_%s */\n"
        "#pragma GCC diagnostic push\n"
        "#pragma GCC diagnostic ignored \"-Wunused-function\"\n"
        "PGY_OPTION_DEFINE(%s, %s)\n"
        "#define Some_%s(...)           pgy_option_some_%s(__VA_ARGS__)\n"
        "#define None_%s()              pgy_option_none_%s()\n"
        "#define IsSome_%s(o)           ((o).tag == %s)\n"
        "#define IsNone_%s(o)           ((o).tag == %s)\n"
        "#define UnwrapOption_%s(o)     pgy_option_unwrap_%s(&(PgyOption_%s){(o).tag, (o).%s})\n"
        "#pragma GCC diagnostic pop\n",
        suffix,
        suffix, ctype_buf,
        suffix, suffix,
        suffix, suffix,
        suffix, some_tag,
        suffix, none_tag,
        suffix, suffix, suffix, some_field);
}

void
ensure_result_specialization_to(TranspilerCtx *ctx, CodeBuf *dst,
                                const char *ok_type, const char *err_type)
{
    char ok_subst[128];
    char err_subst[128];
    const char *ok_tag;
    const char *err_tag;
    const char *ok_field;

    if (ctx == NULL || dst == NULL || ok_type == NULL || err_type == NULL)
        return;

    if (transpiler_specialization_inner_is_unbound_param(
            ctx, &ok_type, ok_subst, sizeof(ok_subst)))
        return;
    if (transpiler_specialization_inner_is_unbound_param(
            ctx, &err_type, err_subst, sizeof(err_subst)))
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
        size_t combined_len = 0;

        combined[0] = '\0';
        if (!transpiler_specialization_append_spec_text(combined,
                sizeof(combined), &combined_len, ok_suffix)
            || !transpiler_specialization_append_spec_text(combined,
                sizeof(combined), &combined_len, "_")
            || !transpiler_specialization_append_spec_text(combined,
                sizeof(combined), &combined_len, err_suffix)) {
            transpiler_specialization_spec_name_too_long(ctx, ok_type);
            return;
        }
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

    ok_tag = pgy_codegen_match_variant_c_result_tag(PGY_MATCH_VARIANT_OK);
    err_tag = pgy_codegen_match_variant_c_result_tag(PGY_MATCH_VARIANT_ERR);
    ok_field = pgy_codegen_match_variant_c_payload_field(PGY_MATCH_VARIANT_OK);
    if (ok_tag == NULL || err_tag == NULL || ok_field == NULL) {
        transpiler_set_backend_error(ctx,
            "C Result<T,E> specialization requires complete Result variant ABI policy");
        return;
    }

    codebuf_write(dst,
        "\n/* PGY_RESULT_%s */\n"
        "#pragma GCC diagnostic push\n"
        "#pragma GCC diagnostic ignored \"-Wunused-function\"\n"
        "PGY_RESULT_DEFINE(%s, %s, %s)\n"
        "#define Ok_%s(...)        pgy_result_ok_%s(__VA_ARGS__)\n"
        "#define Err_%s(...)       pgy_result_err_%s(__VA_ARGS__)\n"
        "#define IsOk_%s(r)        ((r).tag == %s)\n"
        "#define IsErr_%s(r)       ((r).tag == %s)\n"
        "#define Unwrap_%s(r)      pgy_result_unwrap_%s(&(PgyResult_%s){(r).tag, {.%s=(r).%s}})\n"
        "#define UnwrapOr_%s(r, f) ((r).tag == %s ? (r).%s : (f))\n"
        "#pragma GCC diagnostic pop\n",
        combined,
        combined, ok_ctype, err_ctype,
        combined, combined,
        combined, combined,
        combined, ok_tag,
        combined, err_tag,
        combined, combined, combined, ok_field, ok_field,
        combined, ok_tag, ok_field);
}

void
ensure_collection_specialization_to(TranspilerCtx *ctx, CodeBuf *dst,
                                    const char *kind, const char *inner_type)
{
    char suffix[128];
    char ctype_buf[128];
    char inner_subst[128];
    const char *ctype = ctype_buf;

    if (ctx == NULL || dst == NULL || kind == NULL || inner_type == NULL)
        return;

    if (transpiler_specialization_inner_is_unbound_param(
            ctx, &inner_type, inner_subst, sizeof(inner_subst)))
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
    if (strcmp(kind, "Array") == 0
        && (strcmp(inner_type, "Int") == 0
            || strcmp(inner_type, "Long") == 0
            || strcmp(inner_type, "Float") == 0
            || strcmp(inner_type, "Double") == 0
            || strcmp(inner_type, "Bool") == 0
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

    if (ctx->collection_spec_count >= MAX_COLLECTION_SPECIALIZATIONS) {
        transpiler_set_backend_error_with_hints(
            ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "C backend collection specialization registry exceeded MAX_COLLECTION_SPECIALIZATIONS while lowering %s<%s>",
            kind,
            inner_type);
        return;
    }

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
        if (strcmp(inner_type, "Long") == 0
            || strcmp(inner_type, "Bool") == 0) {
            codebuf_write(dst,
                "PGY_SET_VALUES_DEFINE(%s, %s, %s)\n",
                suffix, ctype, suffix);
        }
    } else if (strcmp(kind, "Array") == 0) {
        codebuf_write(dst,
            "\n/* PGY_COLLECTION_ARRAY_%s */\n"
            "#ifndef PGY_ARRAY_COPY_VALUE_%s\n"
            "#define PGY_ARRAY_COPY_VALUE_%s(value) (value)\n"
            "#endif\n"
            "#pragma GCC diagnostic push\n"
            "#pragma GCC diagnostic ignored \"-Wunused-function\"\n"
            "PGY_ARRAY_DEFINE(%s, %s)\n"
            "#pragma GCC diagnostic pop\n",
            suffix,
            suffix, suffix,
            suffix, ctype);
    }
}

void
ensure_collection_specialization(TranspilerCtx *ctx, const char *kind,
                                 const char *inner_type)
{
    ensure_collection_specialization_to(ctx,
        ctx != NULL ? ctx->decls : NULL, kind, inner_type);
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
        ensure_tuple_specialization_from_ast_to(ctx, ctx->out, type_node);
        (void)dst;
        return;
    }

    {
        const char *target_type_name =
            transpiler_type_alias_target_type_name_from_headers(
                ctx, ast_type_name(type_node));
        if (target_type_name != NULL) {
            ensure_type_specializations_from_type_name_to(
                ctx, dst, target_type_name);
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

    if (strcmp(ast_type_name(type_node), "Array") == 0
        && ast_generic_param_count(generic_args) == 1
        && arg0_type != NULL) {
        char *inner_type = render_type_name_in_ctx(ctx, arg0_type);
        ensure_collection_specialization_to(ctx, ctx->out, "Array", inner_type);
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
