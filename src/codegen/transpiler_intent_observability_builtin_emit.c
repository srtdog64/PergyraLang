/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend intent observability builtin lowering.
 */

#include "transpiler_intent_observability_builtin_emit.h"

#include <stdlib.h>

#include "../common/intent_observability_abi.h"
#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"
#include "transpiler_context.h"
#include "transpiler_format.h"

static bool
intent_observability_require_arg_count(ASTNode *call, TranspilerCtx *ctx,
                                       const PgyIntentObservabilityAbiRow *row)
{
    size_t actual = ast_call_arg_count(call);

    if (row != NULL && actual == row->arg_count)
        return true;
    transpiler_set_backend_error_with_hints(ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
        "C backend: %s requires exactly %zu argument%s",
        row != NULL ? row->source_name : "intent observability builtin",
        row != NULL ? row->arg_count : 0,
        row != NULL && row->arg_count == 1 ? "" : "s");
    return false;
}

char *
emit_builtin_intent_observability(ASTNode *call, TranspilerCtx *ctx)
{
    ASTNode *callee = ast_call_callee(call);
    const char *source_name = ast_identifier_name(callee);
    const PgyIntentObservabilityAbiRow *row =
        pgy_intent_observability_abi_row_by_source(source_name);
    char *args[2] = { NULL, NULL };
    char *result = NULL;

    if (row == NULL || row->arg_count > 2) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "C backend: missing intent observability ABI row for '%s'",
            source_name != NULL ? source_name : "<missing>");
        return NULL;
    }
    if (!intent_observability_require_arg_count(call, ctx, row))
        return NULL;

    if (ctx != NULL)
        ctx->uses_intent_observability = true;
    for (size_t i = 0; i < row->arg_count; i++) {
        args[i] = emit_expression(ast_call_argument(call, i), ctx);
        if (args[i] == NULL) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                "C backend: %s could not lower argument[%zu] expression",
                row->source_name, i);
            for (size_t j = 0; j < row->arg_count; j++)
                free(args[j]);
            return NULL;
        }
    }

    if (row->arg_count == 0)
        result = strdup_fmt("%s()", row->runtime_name);
    else if (row->arg_count == 1)
        result = strdup_fmt("%s(%s)", row->runtime_name, args[0]);
    else
        result = strdup_fmt("%s(%s, %s)", row->runtime_name, args[0], args[1]);

    for (size_t i = 0; i < row->arg_count; i++)
        free(args[i]);
    return result;
}
