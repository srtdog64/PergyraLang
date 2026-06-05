/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend unary expression lowering.
 */

#include "transpiler.h"

#include <stdlib.h>

#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"
#include "transpiler_context.h"
#include "transpiler_format.h"

char *
emit_unary(ASTNode *expr, TranspilerCtx *ctx)
{
    char *operand;
    const char *op;
    char *result;

    /* Postfix ? try/propagate is only valid when statement lowering can
     * synthesize the early-return branch. */
    if (ast_unary_operator(expr).type == TOKEN_QUESTION) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_BINDING_TYPE,
            "C backend: '?' is only supported in let-initializer statement context; use 'let value: T = result?;' before using the value");
        return NULL;
    }

    operand = emit_expression(ast_unary_operand(expr), ctx);
    if (operand == NULL) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "C backend: unary expression could not lower operand expression");
        return NULL;
    }
    op = (ast_unary_operator(expr).type == TOKEN_NOT) ? "!" : "-";
    result = strdup_fmt("(%s%s)", op, operand);
    free(operand);
    return result;
}
