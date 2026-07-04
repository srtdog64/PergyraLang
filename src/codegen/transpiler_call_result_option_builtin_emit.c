/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend Result/Option builtin call lowering.
 */

#include "transpiler_call_result_option_builtin_emit.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"
#include "codegen_match_variant_policy.h"
#include "transpiler_context.h"
#include "transpiler_expr_type_infer.h"
#include "transpiler_format.h"
#include "transpiler_option_context.h"
#include "codegen_type_mapping.h"
#include "transpiler_type_result_mapping_helpers.h"

typedef enum TranspilerResultOptionOp {
    TRANS_RESULT_OPTION_OP_NONE = 0,
    TRANS_RESULT_OPTION_OP_ERR,
    TRANS_RESULT_OPTION_OP_IS_ERR,
    TRANS_RESULT_OPTION_OP_IS_NONE,
    TRANS_RESULT_OPTION_OP_IS_OK,
    TRANS_RESULT_OPTION_OP_IS_SOME,
    TRANS_RESULT_OPTION_OP_NONE_VALUE,
    TRANS_RESULT_OPTION_OP_OK,
    TRANS_RESULT_OPTION_OP_SOME,
    TRANS_RESULT_OPTION_OP_UNWRAP,
    TRANS_RESULT_OPTION_OP_UNWRAP_OPTION,
    TRANS_RESULT_OPTION_OP_UNWRAP_OR,
} TranspilerResultOptionOp;

typedef struct TranspilerResultOptionSpec {
    const char *name;
    TranspilerResultOptionOp op;
} TranspilerResultOptionSpec;

static int
transpiler_result_option_spec_compare(const void *key, const void *entry)
{
    const char *name = *(const char * const *)key;
    const TranspilerResultOptionSpec *spec =
        (const TranspilerResultOptionSpec *)entry;

    return strcmp(name, spec->name);
}

static TranspilerResultOptionOp
transpiler_result_option_lookup(const char *fn)
{
    static const TranspilerResultOptionSpec kTranspilerResultOptionSpecs[] = {
        { "IsErr", TRANS_RESULT_OPTION_OP_IS_ERR },
        { "IsNone", TRANS_RESULT_OPTION_OP_IS_NONE },
        { "IsOk", TRANS_RESULT_OPTION_OP_IS_OK },
        { "IsSome", TRANS_RESULT_OPTION_OP_IS_SOME },
        { "Unwrap", TRANS_RESULT_OPTION_OP_UNWRAP },
        { "UnwrapOption", TRANS_RESULT_OPTION_OP_UNWRAP_OPTION },
        { "UnwrapOr", TRANS_RESULT_OPTION_OP_UNWRAP_OR },
    };
    const TranspilerResultOptionSpec *match;
    PgyCodegenMatchVariantKind variant_kind;

    if (fn == NULL)
        return TRANS_RESULT_OPTION_OP_NONE;
    variant_kind = pgy_codegen_match_variant_lookup(fn);
    switch (variant_kind) {
    case PGY_MATCH_VARIANT_ERR:
        return TRANS_RESULT_OPTION_OP_ERR;
    case PGY_MATCH_VARIANT_NONE_CTOR:
        return TRANS_RESULT_OPTION_OP_NONE_VALUE;
    case PGY_MATCH_VARIANT_OK:
        return TRANS_RESULT_OPTION_OP_OK;
    case PGY_MATCH_VARIANT_SOME:
        return TRANS_RESULT_OPTION_OP_SOME;
    default:
        break;
    }

    match = (const TranspilerResultOptionSpec *)bsearch(&fn,
        kTranspilerResultOptionSpecs,
        sizeof(kTranspilerResultOptionSpecs)
            / sizeof(kTranspilerResultOptionSpecs[0]),
        sizeof(kTranspilerResultOptionSpecs[0]),
        transpiler_result_option_spec_compare);
    return match != NULL ? match->op : TRANS_RESULT_OPTION_OP_NONE;
}

static bool
transpiler_option_type_has_concrete_inner(const char *opt_type)
{
    const char *inner = NULL;
    char inner_buf[128];
    if (!transpiler_type_name_is_option(opt_type))
        return false;
    if (!slot_inner_type_name_copy(opt_type, inner_buf, sizeof(inner_buf)))
        return false;
    inner = inner_buf;
    return inner != NULL
        && inner[0] != '\0'
        && strcmp(inner, "Unknown") != 0;
}

static char *
transpiler_result_option_emit_arg(TranspilerCtx *ctx,
                                  ASTNode *expr,
                                  const char *fn,
                                  const char *role)
{
    char *lowered = emit_expression(expr, ctx);
    if (lowered != NULL)
        return lowered;

    transpiler_set_backend_error_with_hints(ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
        "C backend: %s could not lower %s expression",
        fn != NULL ? fn : "<Result/Option builtin>",
        role != NULL ? role : "argument");
    return NULL;
}

char *
emit_call_result_option_builtin(ASTNode *call,
                                ASTNode *callee,
                                TranspilerCtx *ctx,
                                bool *handled)
{
    /* Result<T, E> built-in functions:
     * - Ok/Err require explicit Result context from the surrounding type.
     * - IsOk/IsErr/Unwrap/UnwrapOr may also derive suffix from their Result
     *   operand when the surrounding expression context is not specific. */
    if (handled != NULL)
        *handled = false;
    if (callee->type == AST_IDENTIFIER) {
        const char *fn = ast_identifier_name(callee);
        size_t argc = ast_call_arg_count(call);
        ASTNode *arg0 = ast_call_argument(call, 0);
        ASTNode *arg1 = ast_call_argument(call, 1);
        TranspilerResultOptionOp op = transpiler_result_option_lookup(fn);
        bool is_result_ctor = op == TRANS_RESULT_OPTION_OP_OK
            || op == TRANS_RESULT_OPTION_OP_ERR;
        bool is_result_consumer = op == TRANS_RESULT_OPTION_OP_IS_OK
            || op == TRANS_RESULT_OPTION_OP_IS_ERR
            || op == TRANS_RESULT_OPTION_OP_UNWRAP
            || op == TRANS_RESULT_OPTION_OP_UNWRAP_OR;
        char result_suffix[128] = {0};
        bool have_result_suffix = transpiler_result_suffix_from_context(
            ctx, result_suffix, sizeof(result_suffix));

        if (op == TRANS_RESULT_OPTION_OP_NONE)
            return NULL;
        if (handled != NULL)
            *handled = true;

        if (!have_result_suffix && is_result_consumer
            && argc >= 1
            && arg0 != NULL) {
            const char *arg_type = transpiler_expr_infer_type_name(
                ctx, arg0);
            have_result_suffix = transpiler_result_suffix_from_type_name(
                arg_type, result_suffix, sizeof(result_suffix));
        }

        if ((is_result_ctor || is_result_consumer) && !have_result_suffix) {
            transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "cannot derive Result<T, E> specialization for %s(); add explicit Result<T, E> type context",
                fn);
            return NULL;
        }

        if (op == TRANS_RESULT_OPTION_OP_OK && argc == 1) {
            char *arg = transpiler_result_option_emit_arg(ctx, arg0, fn,
                "payload");
            if (arg == NULL)
                return NULL;
            char *result = strdup_fmt("Ok_%s(%s)", result_suffix, arg);
            free(arg);
            return result;
        }
        if (op == TRANS_RESULT_OPTION_OP_ERR && argc == 1) {
            char *arg = transpiler_result_option_emit_arg(ctx, arg0, fn,
                "error payload");
            if (arg == NULL)
                return NULL;
            char *result = strdup_fmt("Err_%s(%s)", result_suffix, arg);
            free(arg);
            return result;
        }
        if (op == TRANS_RESULT_OPTION_OP_IS_OK && argc == 1) {
            char *arg = transpiler_result_option_emit_arg(ctx, arg0, fn,
                "operand");
            if (arg == NULL)
                return NULL;
            char *result = strdup_fmt("IsOk_%s(%s)", result_suffix, arg);
            free(arg);
            return result;
        }
        if (op == TRANS_RESULT_OPTION_OP_IS_ERR && argc == 1) {
            char *arg = transpiler_result_option_emit_arg(ctx, arg0, fn,
                "operand");
            if (arg == NULL)
                return NULL;
            char *result = strdup_fmt("IsErr_%s(%s)", result_suffix, arg);
            free(arg);
            return result;
        }
        if (op == TRANS_RESULT_OPTION_OP_UNWRAP && argc == 1) {
            char *arg = transpiler_result_option_emit_arg(ctx, arg0, fn,
                "operand");
            if (arg == NULL)
                return NULL;
            char *result = strdup_fmt("Unwrap_%s(%s)", result_suffix, arg);
            free(arg);
            return result;
        }
        if (op == TRANS_RESULT_OPTION_OP_UNWRAP_OR && argc == 2) {
            char *arg = transpiler_result_option_emit_arg(ctx, arg0, fn,
                "operand");
            char *fallback = transpiler_result_option_emit_arg(ctx, arg1,
                fn, "fallback");
            if (arg == NULL || fallback == NULL) {
                free(arg);
                free(fallback);
                return NULL;
            }
            char *result = strdup_fmt("UnwrapOr_%s(%s, %s)", result_suffix, arg, fallback);
            free(arg);
            free(fallback);
            return result;
        }
        if (op == TRANS_RESULT_OPTION_OP_SOME && argc == 1) {
            char *arg = transpiler_result_option_emit_arg(ctx, arg0, fn,
                "payload");
            if (arg == NULL)
                return NULL;
            const char *inner = transpiler_expr_infer_type_name(ctx, arg0);
            char inner_buf[128];
            if (inner == NULL || inner[0] == '\0'
                || strcmp(inner, "Unknown") == 0) {
                if (transpiler_contextual_option_inner_type_copy(ctx,
                        inner_buf, sizeof(inner_buf))) {
                    inner = inner_buf;
                }
            }
            if (inner == NULL || inner[0] == '\0'
                || strcmp(inner, "Unknown") == 0) {
                free(arg);
                transpiler_set_backend_error_with_hints(ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                    "Some requires concrete payload type during C emission");
                return NULL;
            }
            char *result = strdup_fmt("Some_%s(%s)", inner, arg);
            free(arg);
            return result;
        }
        if (op == TRANS_RESULT_OPTION_OP_NONE_VALUE && argc == 0) {
            return transpiler_emit_none_with_context(ctx, call);
        }
        if (op == TRANS_RESULT_OPTION_OP_IS_SOME && argc == 1) {
            const char *opt_type = transpiler_expr_infer_type_name(ctx, arg0);
            if (!transpiler_option_type_has_concrete_inner(opt_type)) {
                transpiler_set_backend_error_with_hints(ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                    "C backend: IsSome requires concrete Option<T>; inferred '%s'",
                    opt_type != NULL ? opt_type : "<unknown>");
                return NULL;
            }
            char *arg = transpiler_result_option_emit_arg(ctx, arg0, fn,
                "operand");
            if (arg == NULL)
                return NULL;
            char inner_buf[128];
            const char *inner = inner_buf;
            (void)slot_inner_type_name_copy(opt_type, inner_buf,
                sizeof(inner_buf));
            char *result = strdup_fmt("IsSome_%s(%s)", inner, arg);
            free(arg);
            return result;
        }
        if (op == TRANS_RESULT_OPTION_OP_IS_NONE && argc == 1) {
            const char *opt_type = transpiler_expr_infer_type_name(ctx, arg0);
            if (!transpiler_option_type_has_concrete_inner(opt_type)) {
                transpiler_set_backend_error_with_hints(ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                    "C backend: IsNone requires concrete Option<T>; inferred '%s'",
                    opt_type != NULL ? opt_type : "<unknown>");
                return NULL;
            }
            char *arg = transpiler_result_option_emit_arg(ctx, arg0, fn,
                "operand");
            if (arg == NULL)
                return NULL;
            char inner_buf[128];
            const char *inner = inner_buf;
            (void)slot_inner_type_name_copy(opt_type, inner_buf,
                sizeof(inner_buf));
            char *result = strdup_fmt("IsNone_%s(%s)", inner, arg);
            free(arg);
            return result;
        }
        if (op == TRANS_RESULT_OPTION_OP_UNWRAP_OPTION && argc == 1) {
            const char *opt_type = transpiler_expr_infer_type_name(ctx, arg0);
            if (!transpiler_option_type_has_concrete_inner(opt_type)) {
                transpiler_set_backend_error_with_hints(ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                    "C backend: UnwrapOption requires concrete Option<T>; inferred '%s'",
                    opt_type != NULL ? opt_type : "<unknown>");
                return NULL;
            }
            char *arg = transpiler_result_option_emit_arg(ctx, arg0, fn,
                "operand");
            if (arg == NULL)
                return NULL;
            char inner_buf[128];
            const char *inner = inner_buf;
            (void)slot_inner_type_name_copy(opt_type, inner_buf,
                sizeof(inner_buf));
            char *result = strdup_fmt("UnwrapOption_%s(%s)", inner, arg);
            free(arg);
            return result;
        }

        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_ALIGN_ARG_TYPE,
            "C backend: Result/Option builtin '%s' received unsupported argument shape",
            fn != NULL ? fn : "<Result/Option builtin>");
        return NULL;
    }

    return NULL;
}
