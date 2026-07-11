/* C projection of MIR-owned TextBuilder runtime-call ABI rows. */

#include "transpiler.h"
#include "transpiler_context.h"
#include "transpiler_expr_core_builtins_emit.h"
#include "transpiler_format.h"

#include "../compiler/mir_abi_layout.h"
#include "../semantic/diag_codes.h"

#include <stdlib.h>
#include <string.h>

char *
emit_builtin_text_builder(ASTNode *call, BuiltinKind kind, TranspilerCtx *ctx)
{
    const char *source_name = kind == BUILTIN_TEXT_BUILDER_NEW
        ? "TextBuilderNew"
        : kind == BUILTIN_TEXT_BUILDER_APPEND
            ? "TextBuilderAppend"
            : kind == BUILTIN_TEXT_BUILDER_FINISH
                ? "TextBuilderFinish" : "TextBuilderDrop";
    const MIRInstruction *inst = ctx != NULL
        ? ctx->active_mir_instruction : NULL;
    const MIRTextBuilderRuntimeRow *row = inst != NULL
        ? inst->text_builder_runtime_row : NULL;
    char *first;
    char *second = NULL;
    char *result;

    if (row == NULL || row->source_name == NULL
        || strcmp(row->source_name, source_name) != 0) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_MATCH_BUILTIN_SIGNATURE,
            "C backend: missing or mismatched instruction-owned TextBuilder ABI row for '%s'",
            source_name);
        return NULL;
    }
    first = emit_expression(ast_call_argument(call, 0), ctx);
    if (first == NULL)
        return NULL;
    if (ast_call_arg_count(call) == 2) {
        second = emit_expression(ast_call_argument(call, 1), ctx);
        if (second == NULL) {
            free(first);
            return NULL;
        }
    }

    if (row->c_call_shape == MIR_TEXT_BUILDER_CALL_CAPACITY_TO_BUILDER) {
        result = strdup_fmt("%s((int64_t)(%s))", row->c_inline_fn, first);
    } else if (row->c_call_shape
               == MIR_TEXT_BUILDER_CALL_BUILDER_STRING_TO_VOID) {
        result = strdup_fmt("%s(&(%s), %s)",
            row->c_inline_fn, first, second);
    } else if (row->c_call_shape
               == MIR_TEXT_BUILDER_CALL_BUILDER_ALLOCATOR_TO_STRING) {
        result = strdup_fmt("%s(&(%s), &(%s))",
            row->c_inline_fn, first, second);
    } else if (row->c_call_shape
               == MIR_TEXT_BUILDER_CALL_BUILDER_TO_VOID) {
        result = strdup_fmt("%s(&(%s))", row->c_inline_fn, first);
    } else {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_MATCH_BUILTIN_SIGNATURE,
            "C backend: unsupported MIR TextBuilder call shape for '%s'",
            source_name);
        result = NULL;
    }
    free(first);
    free(second);
    return result;
}
