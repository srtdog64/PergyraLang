#ifndef PGY_TRANSPILER_SPECIALIZATION_HELPERS_H
#define PGY_TRANSPILER_SPECIALIZATION_HELPERS_H

#include "../common/string_compat.h"
#include "../semantic/diag_codes.h"

#include "transpiler_type_mapping_helpers.h"
#include "transpiler_role_ability_helpers.h"

static char *
render_type_name_with_bindings(TranspilerCtx *ctx, ASTNode *type_node,
                               GenericBindingEntry *bindings, size_t binding_count)
{
    int saved_binding_count = ctx->generic_binding_count;
    char *result;

    for (size_t i = 0; i < binding_count && ctx->generic_binding_count < MAX_GENERIC_BINDINGS; i++)
        ctx->generic_bindings[ctx->generic_binding_count++] = bindings[i];

    result = render_type_name_in_ctx(ctx, type_node);
    ctx->generic_binding_count = saved_binding_count;
    return result;
}

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

/*
 * Record a Result<T, E> specialization and emit PGY_RESULT_DEFINE(...)
 * into `dst` if this (ok_type, err_type) pair has not been seen yet.
 * Int/Bool/String with default PgyError are pre-instantiated in the
 * runtime header, so they are skipped.
 */
static void
ensure_result_specialization_to(TranspilerCtx *ctx, CodeBuf *dst,
                                const char *ok_type, const char *err_type)
{
    if (ctx == NULL || dst == NULL || ok_type == NULL || err_type == NULL)
        return;

    /* Built-in combinations already emitted in pgy_runtime_inline_core.h. */
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
    if (!pergyra_type_to_c_copy(ok_type, ok_ctype_buf,
            sizeof(ok_ctype_buf))) {
        ok_ctype = ok_type;
    }
    if (!pergyra_type_to_c_copy(err_type, err_ctype_buf,
            sizeof(err_ctype_buf))) {
        err_ctype = err_type;
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
        "#define Ok_%s(v)          pgy_result_ok_%s(v)\n"
        "#define Err_%s(m)         pgy_result_err_%s(m)\n"
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
        && (strcmp(inner_type, "Int") == 0 || strcmp(inner_type, "String") == 0))
        return;
    if (strcmp(kind, "Map") == 0
        && (strcmp(inner_type, "Int") == 0 || strcmp(inner_type, "String") == 0))
        return;
    if (strcmp(kind, "Set") == 0
        && (strcmp(inner_type, "Int") == 0 || strcmp(inner_type, "String") == 0))
        return;  /* Int and String Set are pre-instantiated */

    sanitize_c_suffix(inner_type, suffix, sizeof(suffix));
    for (int i = 0; i < ctx->collection_spec_count; i++) {
        if (strcmp(ctx->collection_specs[i].kind, kind) == 0
            && strcmp(ctx->collection_specs[i].suffix, suffix) == 0) {
            return;
        }
    }

    if (ctx->collection_spec_count >= MAX_COLLECTION_SPECIALIZATIONS)
        return;

    if (!pergyra_type_to_c_copy(inner_type, ctype_buf, sizeof(ctype_buf)))
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
            "PGY_HASHMAP_DEFINE(%s, %s)\n",
            suffix, suffix, ctype);
    } else if (strcmp(kind, "Set") == 0) {
        codebuf_write(dst,
            "\n/* PGY_COLLECTION_SET_%s */\n"
            "PGY_SET_DEFINE(%s, %s)\n",
            suffix, suffix, ctype);
    }
}

static void
ensure_collection_specialization(TranspilerCtx *ctx, const char *kind,
                                 const char *inner_type)
{
    ensure_collection_specialization_to(ctx, ctx->decls, kind, inner_type);
}

/* Register tuple specialization and emit typedef struct once per unique
 * signature. Layout: typedef struct { T0 f0; T1 f1; ... } PgyTuple_<suffix>_t; */
static void
ensure_tuple_specialization_to(TranspilerCtx *ctx, CodeBuf *dst, ASTNode *tuple_type)
{
    if (ctx == NULL || dst == NULL || tuple_type == NULL)
        return;
    if (ast_type_tuple_element_count(tuple_type) == 0)
        return;

    size_t n = ast_type_tuple_element_count(tuple_type);

    /* Build suffix and element-name list */
    char suffix[256];
    char elem_names[512];
    suffix[0] = '\0';
    elem_names[0] = '\0';
    for (size_t i = 0; i < n; i++) {
        char *elem = render_type_name(ast_type_tuple_element(tuple_type, i));
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
        transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "too many tuple specializations in one translation unit; limit is 32 while lowering (%s)",
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
        char *elem = render_type_name(ast_type_tuple_element(tuple_type, i));
        char ctype_buf[128];
        const char *ctype = NULL;
        if (pergyra_type_to_c_copy(elem, ctype_buf, sizeof(ctype_buf)))
            ctype = ctype_buf;
        codebuf_write(dst, "    %s f%zu;\n", ctype != NULL ? ctype : elem, i);
        free(elem);
    }
    codebuf_write(dst, "} PgyTuple_%s_t;\n", suffix);
}

void
ensure_type_specializations_from_ast_to(TranspilerCtx *ctx, CodeBuf *dst, ASTNode *type_node)
{
    if (ctx == NULL || type_node == NULL)
        return;

    if (type_node->type == AST_EVENT_HANDLER_TYPE) {
        ASTNode *return_type = ast_event_handler_return_type(type_node);
        if (return_type != NULL)
            ensure_type_specializations_from_ast_to(ctx, dst,
                return_type);
        for (size_t i = 0; i < ast_event_handler_param_count(type_node); i++) {
            ensure_type_specializations_from_ast_to(ctx, dst,
                ast_event_handler_param_type(type_node, i));
        }
        return;
    }

    if (type_node->type != AST_TYPE || ast_type_name(type_node) == NULL)
        return;

    /* Tuple type: recurse into each element then emit struct typedef once.
     * Use ctx->out like Result so the typedef precedes forward declarations. */
    if (ast_type_tuple_element_count(type_node) > 0) {
        size_t element_count = ast_type_tuple_element_count(type_node);
        for (size_t i = 0; i < element_count; i++)
            ensure_type_specializations_from_ast_to(ctx, dst,
                ast_type_tuple_element(type_node, i));
        ensure_tuple_specialization_to(ctx, ctx->out, type_node);
        (void)dst;
        return;
    }

    {
        ASTNode *alias_decl = transpiler_find_type_alias_decl(
            ctx, ast_type_name(type_node));
        if (alias_decl != NULL && ast_type_alias_target_type(alias_decl) != NULL) {
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
        char *inner = render_type_name(arg0_type);
        ensure_collection_specialization_to(ctx, dst, "List", inner);
        free(inner);
        return;
    }

    if (strcmp(ast_type_name(type_node), "Queue") == 0
        && arg0_type != NULL) {
        char *inner = render_type_name(arg0_type);
        ensure_collection_specialization_to(ctx, dst, "Queue", inner);
        free(inner);
        return;
    }

    if (strcmp(ast_type_name(type_node), "HashMap") == 0
        && arg1_type != NULL) {
        char *value = render_type_name(arg1_type);
        ensure_collection_specialization_to(ctx, dst, "Map", value);
        free(value);
        return;
    }

    /* Result<T, E> with custom error type: emit PGY_RESULT_DEFINE(T_E, ...).
     * Use ctx->out rather than the caller's dst so the typedef precedes any
     * forward declarations that reference PgyResult_<T>_<E>. */
    if (strcmp(ast_type_name(type_node), "Result") == 0
        && ast_generic_param_count(generic_args) == 2
        && arg0_type != NULL
        && arg1_type != NULL) {
        char *ok_type  = render_type_name(arg0_type);
        char *err_type = render_type_name(arg1_type);
        ensure_result_specialization_to(ctx, ctx->out, ok_type, err_type);
        (void)dst;
        free(ok_type);
        free(err_type);
        return;
    }
}

void
ensure_type_specializations_from_ast(TranspilerCtx *ctx, ASTNode *type_node)
{
    ensure_type_specializations_from_ast_to(ctx, ctx->decls, type_node);
}

static void
ensure_collection_specializations_from_stmt_to(TranspilerCtx *ctx, CodeBuf *dst, ASTNode *node)
{
    if (ctx == NULL || dst == NULL || node == NULL)
        return;

    switch (node->type) {
    case AST_FUNC_DECL:
        ensure_type_specializations_from_ast_to(ctx, dst,
            ast_func_return_type(node));
        size_t param_count = ast_func_param_count(node);
        for (size_t i = 0; i < param_count; i++) {
            FuncParam *p = ast_func_param(node, i);
            if (p != NULL)
                ensure_type_specializations_from_ast_to(ctx, dst, p->type);
        }
        ensure_collection_specializations_from_stmt_to(ctx, dst,
            ast_func_body(node));
        break;
    case AST_BLOCK:
        for (size_t i = 0; i < ast_block_statement_count(node); i++)
            ensure_collection_specializations_from_stmt_to(ctx, dst,
                ast_block_statement(node, i));
        break;
    case AST_LET_DECL:
        ensure_type_specializations_from_ast_to(ctx, dst, ast_let_type(node));
        ensure_collection_specializations_from_stmt_to(ctx, dst,
            ast_let_initializer(node));
        break;
    case AST_LET_DESTRUCTURE:
        ensure_collection_specializations_from_stmt_to(ctx, dst,
            ast_let_destructure_initializer(node));
        break;
    case AST_WITH_STMT:
        ensure_type_specializations_from_ast_to(ctx, dst, ast_with_slot_type(node));
        ensure_collection_specializations_from_stmt_to(ctx, dst, ast_with_body(node));
        break;
    case AST_IF_STMT:
        ensure_collection_specializations_from_stmt_to(ctx, dst, ast_if_condition(node));
        ensure_collection_specializations_from_stmt_to(ctx, dst, ast_if_then_branch(node));
        ensure_collection_specializations_from_stmt_to(ctx, dst, ast_if_else_branch(node));
        break;
    case AST_WHILE_LOOP:
        ensure_collection_specializations_from_stmt_to(ctx, dst, ast_while_condition(node));
        ensure_collection_specializations_from_stmt_to(ctx, dst, ast_while_body(node));
        break;
    case AST_FOR_LOOP:
        ensure_collection_specializations_from_stmt_to(ctx, dst, ast_for_range_start(node));
        ensure_collection_specializations_from_stmt_to(ctx, dst, ast_for_range_end(node));
        ensure_collection_specializations_from_stmt_to(ctx, dst, ast_for_iterable(node));
        ensure_collection_specializations_from_stmt_to(ctx, dst, ast_for_body(node));
        break;
    case AST_MATCH_STMT:
        ensure_collection_specializations_from_stmt_to(ctx, dst, ast_match_subject(node));
        for (size_t i = 0; i < ast_match_case_count(node); i++)
            ensure_collection_specializations_from_stmt_to(ctx, dst,
                ast_match_case_at(node, i));
        ensure_collection_specializations_from_stmt_to(ctx, dst,
            ast_match_default_body(node));
        break;
    case AST_MATCH_CASE:
        ensure_collection_specializations_from_stmt_to(ctx, dst, ast_match_case_guard(node));
        ensure_collection_specializations_from_stmt_to(ctx, dst, ast_match_case_body(node));
        break;
    case AST_ASYNC_BLOCK:
        for (size_t i = 0; i < ast_async_block_statement_count(node); i++)
            ensure_collection_specializations_from_stmt_to(ctx, dst,
                ast_async_block_statement(node, i));
        break;
    case AST_PARALLEL_BLOCK:
        for (size_t i = 0; i < ast_parallel_task_count(node); i++)
            ensure_collection_specializations_from_stmt_to(ctx, dst,
                ast_parallel_task(node, i));
        break;
    default:
        break;
    }
}

#endif /* PGY_TRANSPILER_SPECIALIZATION_HELPERS_H */
