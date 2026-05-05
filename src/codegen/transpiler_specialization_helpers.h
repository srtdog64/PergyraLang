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
        if (ok_len >= sizeof(combined))
            ok_len = sizeof(combined) - 1;
        memcpy(combined, ok_suffix, ok_len);
        if (ok_len + 1 < sizeof(combined)) {
            combined[ok_len++] = '_';
        }
        if (ok_len + err_len >= sizeof(combined))
            err_len = sizeof(combined) - ok_len - 1;
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

    const char *ok_ctype = pergyra_type_to_c(ok_type);
    const char *err_ctype = pergyra_type_to_c(err_type);
    if (ok_ctype == NULL) ok_ctype = ok_type;
    if (err_ctype == NULL) err_ctype = err_type;

    copy_capped_string(ctx->result_specs_suffix[ctx->result_spec_count],
                       sizeof(ctx->result_specs_suffix[0]), combined);
    copy_capped_string(ctx->result_specs_ok_ctype[ctx->result_spec_count],
                       sizeof(ctx->result_specs_ok_ctype[0]), ok_ctype);
    copy_capped_string(ctx->result_specs_err_ctype[ctx->result_spec_count],
                       sizeof(ctx->result_specs_err_ctype[0]), err_ctype);
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
    const char *ctype;

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

    ctype = pergyra_type_to_c(inner_type);
    if (ctype == NULL)
        return;

    snprintf(ctx->collection_specs[ctx->collection_spec_count].kind,
             sizeof(ctx->collection_specs[ctx->collection_spec_count].kind),
             "%s", kind);
    snprintf(ctx->collection_specs[ctx->collection_spec_count].suffix,
             sizeof(ctx->collection_specs[ctx->collection_spec_count].suffix),
             "%s", suffix);
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
    if (tuple_type->data.type.tuple_elements == NULL
        || tuple_type->data.type.tuple_element_count == 0)
        return;

    size_t n = tuple_type->data.type.tuple_element_count;

    /* Build suffix and element-name list */
    char suffix[256];
    size_t suffix_off = 0;
    char elem_names[512];
    size_t elem_off = 0;
    suffix[0] = '\0';
    elem_names[0] = '\0';
    for (size_t i = 0; i < n; i++) {
        char *elem = render_type_name(tuple_type->data.type.tuple_elements[i]);
        char sane[96];
        sanitize_c_suffix(elem, sane, sizeof(sane));
        if (i > 0) {
            suffix_off += (size_t)snprintf(suffix + suffix_off,
                sizeof(suffix) - suffix_off, "_");
            elem_off += (size_t)snprintf(elem_names + elem_off,
                sizeof(elem_names) - elem_off, " ");
        }
        suffix_off += (size_t)snprintf(suffix + suffix_off,
            sizeof(suffix) - suffix_off, "%s", sane);
        elem_off += (size_t)snprintf(elem_names + elem_off,
            sizeof(elem_names) - elem_off, "%s", elem);
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
    copy_capped_string(ctx->tuple_specs_suffix[ctx->tuple_spec_count],
        sizeof(ctx->tuple_specs_suffix[0]), suffix);
    copy_capped_string(ctx->tuple_specs_elements[ctx->tuple_spec_count],
        sizeof(ctx->tuple_specs_elements[0]), elem_names);
    ctx->tuple_specs_arity[ctx->tuple_spec_count] = (int)n;
    ctx->tuple_spec_count++;

    codebuf_write(dst, "\n/* PGY_TUPLE_%s */\n", suffix);
    codebuf_write(dst, "typedef struct {\n");
    for (size_t i = 0; i < n; i++) {
        char *elem = render_type_name(tuple_type->data.type.tuple_elements[i]);
        const char *ctype = pergyra_type_to_c(elem);
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
        if (type_node->data.event_handler_type.return_type != NULL)
            ensure_type_specializations_from_ast_to(ctx, dst,
                type_node->data.event_handler_type.return_type);
        for (size_t i = 0; i < type_node->data.event_handler_type.param_count; i++) {
            ensure_type_specializations_from_ast_to(ctx, dst,
                type_node->data.event_handler_type.param_types[i]);
        }
        return;
    }

    if (type_node->type != AST_TYPE || type_node->data.type.name == NULL)
        return;

    /* Tuple type: recurse into each element then emit struct typedef once.
     * Use ctx->out like Result so the typedef precedes forward declarations. */
    if (type_node->data.type.tuple_elements != NULL
        && type_node->data.type.tuple_element_count > 0) {
        for (size_t i = 0; i < type_node->data.type.tuple_element_count; i++)
            ensure_type_specializations_from_ast_to(ctx, dst,
                type_node->data.type.tuple_elements[i]);
        ensure_tuple_specialization_to(ctx, ctx->out, type_node);
        (void)dst;
        return;
    }

    {
        ASTNode *alias_decl = transpiler_find_type_alias_decl(
            ctx, type_node->data.type.name);
        if (alias_decl != NULL && alias_decl->data.type_alias.target_type != NULL) {
            ensure_type_specializations_from_ast_to(ctx, dst,
                alias_decl->data.type_alias.target_type);
            return;
        }
    }

    if (type_node->data.type.generic_args != NULL) {
        for (size_t i = 0; i < type_node->data.type.generic_args->count; i++) {
            GenericParam *arg = type_node->data.type.generic_args->params[i];
            if (arg != NULL && arg->constraint != NULL)
                ensure_type_specializations_from_ast_to(ctx, dst, arg->constraint);
        }
    }

    if (strcmp(type_node->data.type.name, "List") == 0
        && type_node->data.type.generic_args != NULL
        && type_node->data.type.generic_args->count > 0
        && type_node->data.type.generic_args->params[0] != NULL
        && type_node->data.type.generic_args->params[0]->constraint != NULL) {
        char *inner = render_type_name(type_node->data.type.generic_args->params[0]->constraint);
        ensure_collection_specialization_to(ctx, dst, "List", inner);
        free(inner);
        return;
    }

    if (strcmp(type_node->data.type.name, "Queue") == 0
        && type_node->data.type.generic_args != NULL
        && type_node->data.type.generic_args->count > 0
        && type_node->data.type.generic_args->params[0] != NULL
        && type_node->data.type.generic_args->params[0]->constraint != NULL) {
        char *inner = render_type_name(type_node->data.type.generic_args->params[0]->constraint);
        ensure_collection_specialization_to(ctx, dst, "Queue", inner);
        free(inner);
        return;
    }

    if (strcmp(type_node->data.type.name, "HashMap") == 0
        && type_node->data.type.generic_args != NULL
        && type_node->data.type.generic_args->count > 1
        && type_node->data.type.generic_args->params[1] != NULL
        && type_node->data.type.generic_args->params[1]->constraint != NULL) {
        char *value = render_type_name(type_node->data.type.generic_args->params[1]->constraint);
        ensure_collection_specialization_to(ctx, dst, "Map", value);
        free(value);
        return;
    }

    /* Result<T, E> with custom error type: emit PGY_RESULT_DEFINE(T_E, ...).
     * Use ctx->out rather than the caller's dst so the typedef precedes any
     * forward declarations that reference PgyResult_<T>_<E>. */
    if (strcmp(type_node->data.type.name, "Result") == 0
        && type_node->data.type.generic_args != NULL
        && type_node->data.type.generic_args->count == 2
        && type_node->data.type.generic_args->params[0] != NULL
        && type_node->data.type.generic_args->params[0]->constraint != NULL
        && type_node->data.type.generic_args->params[1] != NULL
        && type_node->data.type.generic_args->params[1]->constraint != NULL) {
        char *ok_type  = render_type_name(type_node->data.type.generic_args->params[0]->constraint);
        char *err_type = render_type_name(type_node->data.type.generic_args->params[1]->constraint);
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
        ensure_type_specializations_from_ast_to(ctx, dst, node->data.func_decl.return_type);
        for (size_t i = 0; i < node->data.func_decl.param_count; i++) {
            FuncParam *p = node->data.func_decl.params[i];
            if (p != NULL)
                ensure_type_specializations_from_ast_to(ctx, dst, p->type);
        }
        ensure_collection_specializations_from_stmt_to(ctx, dst, node->data.func_decl.body);
        break;
    case AST_BLOCK:
        for (size_t i = 0; i < node->data.block.count; i++)
            ensure_collection_specializations_from_stmt_to(ctx, dst,
                node->data.block.statements[i]);
        break;
    case AST_LET_DECL:
        ensure_type_specializations_from_ast_to(ctx, dst, node->data.let_decl.type);
        ensure_collection_specializations_from_stmt_to(ctx, dst,
            node->data.let_decl.initializer);
        break;
    case AST_LET_DESTRUCTURE:
        ensure_collection_specializations_from_stmt_to(ctx, dst,
            node->data.let_destructure.initializer);
        break;
    case AST_WITH_STMT:
        ensure_type_specializations_from_ast_to(ctx, dst, node->data.with_stmt.slot_type);
        ensure_collection_specializations_from_stmt_to(ctx, dst, node->data.with_stmt.body);
        break;
    case AST_IF_STMT:
        ensure_collection_specializations_from_stmt_to(ctx, dst, node->data.if_stmt.condition);
        ensure_collection_specializations_from_stmt_to(ctx, dst, node->data.if_stmt.then_branch);
        ensure_collection_specializations_from_stmt_to(ctx, dst, node->data.if_stmt.else_branch);
        break;
    case AST_WHILE_LOOP:
        ensure_collection_specializations_from_stmt_to(ctx, dst, node->data.while_loop.condition);
        ensure_collection_specializations_from_stmt_to(ctx, dst, node->data.while_loop.body);
        break;
    case AST_FOR_LOOP:
        ensure_collection_specializations_from_stmt_to(ctx, dst, node->data.for_loop.range_start);
        ensure_collection_specializations_from_stmt_to(ctx, dst, node->data.for_loop.range_end);
        ensure_collection_specializations_from_stmt_to(ctx, dst, node->data.for_loop.iterable);
        ensure_collection_specializations_from_stmt_to(ctx, dst, node->data.for_loop.body);
        break;
    case AST_MATCH_STMT:
        ensure_collection_specializations_from_stmt_to(ctx, dst, node->data.match_stmt.subject);
        for (size_t i = 0; i < node->data.match_stmt.case_count; i++)
            ensure_collection_specializations_from_stmt_to(ctx, dst,
                node->data.match_stmt.cases[i]);
        ensure_collection_specializations_from_stmt_to(ctx, dst,
            node->data.match_stmt.default_body);
        break;
    case AST_MATCH_CASE:
        ensure_collection_specializations_from_stmt_to(ctx, dst, node->data.match_case.guard);
        ensure_collection_specializations_from_stmt_to(ctx, dst, node->data.match_case.body);
        break;
    case AST_ASYNC_BLOCK:
        for (size_t i = 0; i < node->data.async_block.statement_count; i++)
            ensure_collection_specializations_from_stmt_to(ctx, dst,
                node->data.async_block.statements[i]);
        break;
    case AST_PARALLEL_BLOCK:
        for (size_t i = 0; i < node->data.parallel.task_count; i++)
            ensure_collection_specializations_from_stmt_to(ctx, dst,
                node->data.parallel.tasks[i]);
        break;
    default:
        break;
    }
}

static const char *
collection_runtime_suffix(const char *inner_type)
{
    static char suffix[128];

    if (inner_type == NULL)
        return "int";
    if (strcmp(inner_type, "Int") == 0)
        return "int";
    if (strcmp(inner_type, "String") == 0)
        return "string";

    sanitize_c_suffix(inner_type, suffix, sizeof(suffix));
    return suffix;
}
