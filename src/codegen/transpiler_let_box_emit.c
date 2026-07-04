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
#include "codegen_type_mapping.h"
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
transpiler_box_array_inner_from_annotation(TranspilerCtx *ctx, ASTNode *ann)
{
    GenericParam *box_param;
    ASTNode *array_arg;
    GenericParam *inner_param;
    ASTNode *inner_type;

    if (ann == NULL
        || ann->type != AST_TYPE
        || ast_type_name(ann) == NULL
        || strcmp(ast_type_name(ann), "Box") != 0) {
        return NULL;
    }

    box_param = ast_generic_param_at(ast_type_generic_args(ann), 0);
    array_arg = ast_generic_param_constraint(box_param);
    if (array_arg == NULL
        || array_arg->type != AST_TYPE
        || ast_type_name(array_arg) == NULL
        || strcmp(ast_type_name(array_arg), "Array") != 0) {
        return NULL;
    }

    inner_param = ast_generic_param_at(ast_type_generic_args(array_arg), 0);
    inner_type = ast_generic_param_constraint(inner_param);
    if (inner_type != NULL)
        return render_type_name_in_ctx(ctx, inner_type);
    if (ast_generic_param_name(inner_param) != NULL)
        return pergyra_strdup(ast_generic_param_name(inner_param));
    return NULL;
}

static char *
transpiler_box_array_inner_from_type_name(const char *ann_type_name)
{
    const char *inner;
    const char *close;
    size_t len;
    char inner_buf[128];

    if (!transpiler_type_name_is_box_array(ann_type_name))
        return NULL;

    inner = ann_type_name + 10;
    close = strstr(inner, ">>");
    len = close != NULL ? (size_t)(close - inner) : strlen(inner);
    if (len >= sizeof(inner_buf))
        len = sizeof(inner_buf) - 1;
    memcpy(inner_buf, inner, len);
    inner_buf[len] = '\0';
    return pergyra_strdup(inner_buf);
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

static char *
transpiler_box_array_allocator_arg(TranspilerCtx *ctx,
                                   ASTNode *arg,
                                   const char *binding_name)
{
    const char *arg_name;
    const char *arg_type;
    char *value;
    char *result;

    if (arg == NULL)
        return pergyra_strdup("NULL");

    if (arg->type != AST_IDENTIFIER || ast_identifier_name(arg) == NULL) {
        transpiler_set_backend_error_with_hints(
            ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "C BoxArray allocator for '%s' must be a named Allocator local",
            binding_name != NULL ? binding_name : "<binding>");
        return NULL;
    }

    arg_name = ast_identifier_name(arg);
    arg_type = lookup_typed_var(ctx, arg_name);
    if (arg_type == NULL)
        arg_type = infer_expression_type_name(ctx, arg);
    if (arg_type == NULL || strcmp(arg_type, "Allocator") != 0) {
        transpiler_set_backend_error_with_hints(
            ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "C BoxArray allocator '%s' for '%s' must have type Allocator",
            arg_name != NULL ? arg_name : "<allocator>",
            binding_name != NULL ? binding_name : "<binding>");
        return NULL;
    }

    value = transpiler_box_let_emit_arg(ctx, arg, binding_name, "allocator");
    if (value == NULL)
        return NULL;
    result = strdup_fmt("&%s", value);
    free(value);
    return result;
}

bool
transpiler_try_render_box_array_let_ctor(
    TranspilerCtx *ctx,
    const char *binding_name,
    ASTNode *init,
    ASTNode *ann,
    const char *ann_type_name,
    TranspilerBoxArrayLetCtor *out)
{
    char *inner_owned = NULL;
    char *capacity = NULL;
    char *allocator = NULL;
    const char *callee_name;

    if (out != NULL) {
        out->c_type = NULL;
        out->surface_type = NULL;
        out->rhs = NULL;
    }

    if (ctx == NULL || init == NULL || out == NULL)
        return false;

    callee_name = ast_identifier_name(ast_call_callee(init));
    if (init->type != AST_CALL
        || ast_call_callee(init) == NULL
        || ast_call_callee(init)->type != AST_IDENTIFIER
        || callee_name == NULL
        || transpiler_box_let_lookup(callee_name)
            != TRANS_BOX_LET_OP_BOX_ARRAY) {
        return false;
    }

    if (ast_call_arg_count(init) > 2) {
        transpiler_set_backend_error_with_hints(
            ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "C BoxArray constructor for '%s' accepts capacity and optional Allocator only",
            binding_name != NULL ? binding_name : "<binding>");
        return true;
    }

    inner_owned = transpiler_box_array_inner_from_annotation(ctx, ann);
    if (inner_owned == NULL)
        inner_owned = transpiler_box_array_inner_from_type_name(ann_type_name);
    if (inner_owned == NULL || inner_owned[0] == '\0') {
        transpiler_set_backend_error_with_hints(
            ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "cannot emit BoxArray declaration for '%s': missing explicit Box<Array<T>> annotation",
            binding_name != NULL ? binding_name : "(anonymous)");
        free(inner_owned);
        return true;
    }

    capacity = (ast_call_arg_count(init) > 0)
        ? transpiler_box_let_emit_arg(ctx, ast_call_argument(init, 0),
              binding_name, "capacity")
        : pergyra_strdup("0");
    if (capacity == NULL) {
        free(inner_owned);
        return true;
    }
    allocator = (ast_call_arg_count(init) > 1)
        ? transpiler_box_array_allocator_arg(ctx, ast_call_argument(init, 1),
              binding_name)
        : pergyra_strdup("NULL");
    if (allocator == NULL) {
        free(inner_owned);
        free(capacity);
        return true;
    }

    out->c_type = strdup_fmt("PgyBoxArray_%s", inner_owned);
    out->surface_type = ann_type_name != NULL
        ? pergyra_strdup(ann_type_name)
        : strdup_fmt("Box<Array<%s>>", inner_owned);
    out->rhs = strdup_fmt("pgy_box_array_new_%s(%s, %s)",
                          inner_owned, capacity, allocator);
    if (out->c_type == NULL || out->surface_type == NULL || out->rhs == NULL) {
        transpiler_set_backend_error_with_hints(
            ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "C BoxArray constructor allocation failed for '%s'",
            binding_name != NULL ? binding_name : "<binding>");
        transpiler_box_array_let_ctor_destroy(out);
    }

    free(inner_owned);
    free(capacity);
    free(allocator);
    return true;
}

void
transpiler_box_array_let_ctor_destroy(TranspilerBoxArrayLetCtor *ctor)
{
    if (ctor == NULL)
        return;
    free(ctor->c_type);
    free(ctor->surface_type);
    free(ctor->rhs);
    ctor->c_type = NULL;
    ctor->surface_type = NULL;
    ctor->rhs = NULL;
}

static bool
transpiler_try_emit_box_array_let(TranspilerCtx *ctx,
                                  const char *name,
                                  ASTNode *init,
                                  ASTNode *ann,
                                  char **ann_type_name_ptr)
{
    TranspilerBoxArrayLetCtor ctor;

    if (ctx == NULL || name == NULL || init == NULL || ann_type_name_ptr == NULL)
        return false;
    if (!transpiler_try_render_box_array_let_ctor(
            ctx, name, init, ann, *ann_type_name_ptr, &ctor)) {
        return false;
    }
    if (ctor.rhs == NULL) {
        transpiler_box_array_let_ctor_destroy(&ctor);
        free(*ann_type_name_ptr);
        *ann_type_name_ptr = NULL;
        return true;
    }

    write_indent(ctx);
    codebuf_write(ctx->out, "%s %s = %s;\n", ctor.c_type, name, ctor.rhs);
    register_typed_var(ctx, name, ctor.surface_type);
    transpiler_box_array_let_ctor_destroy(&ctor);
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
    return transpiler_try_emit_box_array_let(ctx, name, init, ann,
                                             ann_type_name_ptr)
        || transpiler_try_emit_box_or_rc_let(ctx, name, init, ann,
                                             ann_type_name_ptr);
}
