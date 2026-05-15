#ifndef PGY_TRANSPILER_LET_BOX_EMIT_H
#define PGY_TRANSPILER_LET_BOX_EMIT_H

/* Box/BoxArray/Rc let-declaration lowering helpers.
 * Included inside transpiler.c before transpiler_let_emit.h. */

#include "../parser/ast_api.h"

static bool
transpiler_try_emit_box_array_let(TranspilerCtx *ctx,
                                  const char *name,
                                  ASTNode *init,
                                  char **ann_type_name_ptr)
{
    char *ann_type_name;
    const char *inner = NULL;
    char inner_buf[64];
    char *capacity;
    char *allocator;

    if (ctx == NULL || name == NULL || init == NULL || ann_type_name_ptr == NULL)
        return false;
    ann_type_name = *ann_type_name_ptr;
    if (init->type != AST_CALL
        || ast_call_callee(init) == NULL
        || ast_call_callee(init)->type != AST_IDENTIFIER
        || strcmp(ast_call_callee(init)->data.identifier.name, "BoxArray") != 0) {
        return false;
    }

    if (ann_type_name != NULL && strncmp(ann_type_name, "Box<Array<", 10) == 0) {
        const char *close;
        size_t len;

        inner = ann_type_name + 10;
        close = strstr(inner, ">>");
        len = close != NULL ? (size_t)(close - inner) : strlen(inner);
        if (len >= sizeof(inner_buf))
            len = sizeof(inner_buf) - 1;
        memcpy(inner_buf, inner, len);
        inner_buf[len] = '\0';
        inner = inner_buf;
    }
    if (inner == NULL) {
        transpiler_set_backend_error_with_hints(
            ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "cannot emit BoxArray declaration for '%s': missing explicit Box<Array<T>> annotation",
            name != NULL ? name : "(anonymous)");
        free(*ann_type_name_ptr);
        *ann_type_name_ptr = NULL;
        return true;
    }

    capacity = (ast_call_arg_count(init) > 0)
        ? emit_expression(ast_call_argument(init, 0), ctx)
        : pergyra_strdup("0");
    allocator = (ast_call_arg_count(init) > 1)
        ? emit_expression(ast_call_argument(init, 1), ctx)
        : pergyra_strdup("NULL");
    write_indent(ctx);
    codebuf_write(ctx->out,
        "PgyBoxArray_%s %s = pgy_box_array_new_%s(%s, %s);\n",
        inner, name, inner, capacity, allocator);
    if (*ann_type_name_ptr != NULL) {
        register_typed_var(ctx, name, *ann_type_name_ptr);
    } else {
        char *box_array_type = strdup_fmt("Box<Array<%s>>", inner);
        register_typed_var(ctx, name, box_array_type);
        free(box_array_type);
    }
    free(capacity);
    free(allocator);
    free(*ann_type_name_ptr);
    *ann_type_name_ptr = NULL;
    return true;
}

static char *
transpiler_box_inner_from_annotation(ASTNode *ann)
{
    GenericParams *generic_args = ast_type_generic_args(ann);

    if (ann == NULL
        || ann->type != AST_TYPE
        || generic_args == NULL
        || generic_args->count == 0) {
        return NULL;
    }

    GenericParam *param = generic_args->params[0];
    if (param != NULL && param->constraint != NULL)
        return render_type_name(param->constraint);
    if (param != NULL && param->name != NULL)
        return pergyra_strdup(param->name);
    return NULL;
}

static char *
transpiler_box_inner_from_init_arg(TranspilerCtx *ctx, ASTNode *init)
{
    ASTNode *arg;
    const char *inferred_arg;

    if (init == NULL || init->type != AST_CALL || ast_call_arg_count(init) == 0)
        return NULL;
    arg = ast_call_argument(init, 0);
    inferred_arg = infer_expression_type_name(ctx, arg);
    if (inferred_arg != NULL && strcmp(inferred_arg, "Unknown") != 0)
        return pergyra_strdup(inferred_arg);
    return NULL;
}

static bool
transpiler_try_emit_box_or_rc_let(TranspilerCtx *ctx,
                                  const char *name,
                                  ASTNode *init,
                                  ASTNode *ann,
                                  char **ann_type_name_ptr)
{
    const char *callee_name;
    char *box_inner_owned = NULL;
    const char *box_inner;
    char *registered_type;

    if (ctx == NULL || name == NULL || init == NULL || ann_type_name_ptr == NULL)
        return false;
    if (init->type != AST_CALL
        || ast_call_callee(init) == NULL
        || ast_call_callee(init)->type != AST_IDENTIFIER) {
        return false;
    }

    callee_name = ast_call_callee(init)->data.identifier.name;
    if (callee_name == NULL
        || ((strcmp(callee_name, "Box") != 0 || find_class_decl(ctx, callee_name) != NULL)
            && (strcmp(callee_name, "Rc") != 0 || find_class_decl(ctx, callee_name) != NULL))) {
        return false;
    }

    box_inner_owned = transpiler_box_inner_from_annotation(ann);
    if (box_inner_owned == NULL)
        box_inner_owned = transpiler_box_inner_from_init_arg(ctx, init);

    box_inner = box_inner_owned;
    if (box_inner == NULL || box_inner[0] == '\0') {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "C backend: Box/Rc binding '%s' requires explicit Box<T>/Rc<T> annotation or inferable initializer type",
            name != NULL ? name : "<binding>");
        free(box_inner_owned);
        free(*ann_type_name_ptr);
        *ann_type_name_ptr = NULL;
        return true;
    }

    write_indent(ctx);
    codebuf_write(ctx->out, "PgyBox_%s %s = pgy_box_new_%s(",
                  box_inner, name, box_inner);
    if (ast_call_arg_count(init) > 0) {
        char *arg = emit_expression(ast_call_argument(init, 0), ctx);
        codebuf_write(ctx->out, "%s", arg);
        free(arg);
    }
    codebuf_write(ctx->out, ");\n");
    registered_type = *ann_type_name_ptr != NULL
        ? pergyra_strdup(*ann_type_name_ptr)
        : strdup_fmt("Box<%s>", box_inner);
    register_typed_var(ctx, name, registered_type);
    free(registered_type);
    free(box_inner_owned);
    free(*ann_type_name_ptr);
    *ann_type_name_ptr = NULL;
    return true;
}

static bool
transpiler_try_emit_box_family_let(TranspilerCtx *ctx,
                                   const char *name,
                                   ASTNode *init,
                                   ASTNode *ann,
                                   char **ann_type_name_ptr)
{
    return transpiler_try_emit_box_array_let(ctx, name, init, ann_type_name_ptr)
        || transpiler_try_emit_box_or_rc_let(ctx, name, init, ann,
                                             ann_type_name_ptr);
}

#endif /* PGY_TRANSPILER_LET_BOX_EMIT_H */
