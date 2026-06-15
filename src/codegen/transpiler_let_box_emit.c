/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend Box/BoxArray/Rc let-declaration lowering.
 */

#include "transpiler_let_box_emit.h"

#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_expr_type_infer.h"
#include "transpiler_format.h"
#include "transpiler_symbols.h"
#include "transpiler_type_mapping.h"
#include "transpiler_type_render.h"

typedef enum TranspilerBoxLetOp {
    TRANS_BOX_LET_OP_NONE = 0,
    TRANS_BOX_LET_OP_BOX,
    TRANS_BOX_LET_OP_BOX_ARRAY,
    TRANS_BOX_LET_OP_RC,
} TranspilerBoxLetOp;

typedef struct TranspilerBoxLetSpec {
    const char *name;
    TranspilerBoxLetOp op;
} TranspilerBoxLetSpec;

static int
transpiler_box_let_spec_compare(const void *key, const void *entry)
{
    const char *name = *(const char * const *)key;
    const TranspilerBoxLetSpec *spec = (const TranspilerBoxLetSpec *)entry;

    return strcmp(name, spec->name);
}

static TranspilerBoxLetOp
transpiler_box_let_lookup(const char *callee_name)
{
    static const TranspilerBoxLetSpec kTranspilerBoxLetSpecs[] = {
        { "Box", TRANS_BOX_LET_OP_BOX },
        { "BoxArray", TRANS_BOX_LET_OP_BOX_ARRAY },
        { "Rc", TRANS_BOX_LET_OP_RC },
    };
    const TranspilerBoxLetSpec *match;

    if (callee_name == NULL)
        return TRANS_BOX_LET_OP_NONE;

    match = (const TranspilerBoxLetSpec *)bsearch(&callee_name,
        kTranspilerBoxLetSpecs,
        sizeof(kTranspilerBoxLetSpecs) / sizeof(kTranspilerBoxLetSpecs[0]),
        sizeof(kTranspilerBoxLetSpecs[0]),
        transpiler_box_let_spec_compare);
    return match != NULL ? match->op : TRANS_BOX_LET_OP_NONE;
}

static char *
transpiler_box_let_emit_arg(TranspilerCtx *ctx,
                            ASTNode *arg,
                            const char *binding_name,
                            const char *role)
{
    char *rendered = emit_expression(arg, ctx);

    if (rendered != NULL)
        return rendered;

    transpiler_set_backend_error_with_hints(
        ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
        "C Box/Rc let binding '%s' could not lower %s expression",
        binding_name != NULL ? binding_name : "<binding>",
        role != NULL ? role : "initializer");
    return NULL;
}

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
    const char *callee_name;

    if (ctx == NULL || name == NULL || init == NULL || ann_type_name_ptr == NULL)
        return false;
    callee_name = ast_identifier_name(ast_call_callee(init));
    ann_type_name = *ann_type_name_ptr;
    if (init->type != AST_CALL
        || ast_call_callee(init) == NULL
        || ast_call_callee(init)->type != AST_IDENTIFIER
        || callee_name == NULL
        || transpiler_box_let_lookup(callee_name)
            != TRANS_BOX_LET_OP_BOX_ARRAY) {
        return false;
    }

    if (transpiler_type_name_is_box_array(ann_type_name)) {
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
        ? transpiler_box_let_emit_arg(ctx, ast_call_argument(init, 0),
              name, "capacity")
        : pergyra_strdup("0");
    if (capacity == NULL) {
        free(*ann_type_name_ptr);
        *ann_type_name_ptr = NULL;
        return true;
    }
    allocator = (ast_call_arg_count(init) > 1)
        ? transpiler_box_let_emit_arg(ctx, ast_call_argument(init, 1),
              name, "allocator")
        : pergyra_strdup("NULL");
    if (allocator == NULL) {
        free(capacity);
        free(*ann_type_name_ptr);
        *ann_type_name_ptr = NULL;
        return true;
    }
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
transpiler_box_inner_from_annotation(TranspilerCtx *ctx, ASTNode *ann)
{
    GenericParams *generic_args = ast_type_generic_args(ann);

    if (ann == NULL
        || ann->type != AST_TYPE
        || ast_generic_param_count(generic_args) == 0) {
        return NULL;
    }

    GenericParam *param = ast_generic_param_at(generic_args, 0);
    if (ast_generic_param_constraint(param) != NULL)
        return render_type_name_in_ctx(ctx, ast_generic_param_constraint(param));
    if (ast_generic_param_name(param) != NULL)
        return pergyra_strdup(ast_generic_param_name(param));
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
    char *payload_arg = NULL;
    char *registered_type;

    if (ctx == NULL || name == NULL || init == NULL || ann_type_name_ptr == NULL)
        return false;
    if (init->type != AST_CALL
        || ast_call_callee(init) == NULL
        || ast_call_callee(init)->type != AST_IDENTIFIER) {
        return false;
    }

    callee_name = ast_identifier_name(ast_call_callee(init));
    TranspilerBoxLetOp op = transpiler_box_let_lookup(callee_name);
    if ((op != TRANS_BOX_LET_OP_BOX && op != TRANS_BOX_LET_OP_RC)
        || transpiler_projection_nominal_decl_exists_local(
            ctx, callee_name)) {
        return false;
    }

    box_inner_owned = transpiler_box_inner_from_annotation(ctx, ann);
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

    if (ast_call_arg_count(init) > 0) {
        payload_arg = transpiler_box_let_emit_arg(ctx,
            ast_call_argument(init, 0), name, "payload");
        if (payload_arg == NULL) {
            free(box_inner_owned);
            free(*ann_type_name_ptr);
            *ann_type_name_ptr = NULL;
            return true;
        }
    }
    write_indent(ctx);
    codebuf_write(ctx->out, "PgyBox_%s %s = pgy_box_new_%s(%s);\n",
                  box_inner,
                  name,
                  box_inner,
                  payload_arg != NULL ? payload_arg : "");
    free(payload_arg);
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

bool
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
