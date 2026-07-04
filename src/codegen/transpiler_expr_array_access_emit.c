#include "transpiler_expr_array_access_emit.h"

#include <stdlib.h>
#include <string.h>

#include "../semantic/diag_codes.h"
#include "parser/ast_api.h"
#include "transpiler_context.h"
#include "transpiler_expr_type_infer.h"
#include "transpiler_format.h"
#include "codegen_type_mapping.h"

static char *
transpiler_array_access_emit_operand(TranspilerCtx *ctx,
                                     ASTNode *operand,
                                     const char *role)
{
    char *lowered = emit_expression(operand, ctx);
    if (lowered != NULL)
        return lowered;

    transpiler_set_backend_error_with_hints(ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
        "C backend: array access could not lower %s expression",
        role != NULL ? role : "operand");
    return NULL;
}

char *
emit_array_access_expression(ASTNode *node, TranspilerCtx *ctx)
{
    ASTNode *array_node = ast_array_access_array(node);
    ASTNode *index_node = ast_array_access_index(node);
    char *array = transpiler_array_access_emit_operand(ctx,
        array_node, "receiver");
    if (array == NULL)
        return NULL;
    char *index = transpiler_array_access_emit_operand(ctx,
        index_node, "index");
    if (index == NULL) {
        free(array);
        return NULL;
    }
    const char *array_type = infer_expression_type_name(ctx, array_node);
    char *result;

    if (transpiler_type_name_is_array(array_type)) {
        char inner_buf[128];
        const char *inner = NULL;
        if (slot_inner_type_name_copy(array_type, inner_buf,
                sizeof(inner_buf))) {
            inner = inner_buf;
        }
        if (inner == NULL || inner[0] == '\0'
            || strcmp(inner, "Unknown") == 0) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "C array access requires concrete Array<T> element metadata");
            free(array);
            free(index);
            return NULL;
        }
        /* Recursive suffix: Array<Array<Int>> indexing yields the inner
         * PgyArray_Int via pgy_array_get_Array_Int. Identity for scalars. */
        char arr_suffix[128];
        sanitize_c_suffix(inner, arr_suffix, sizeof(arr_suffix));
        int tmp_id = ++ctx->tmp_counter;
        result = strdup_fmt(
            "({ PgyArray_%s _pgy_arr_get_%d = %s; "
            "pgy_array_get_%s(&_pgy_arr_get_%d, %s); })",
            arr_suffix, tmp_id, array, arr_suffix, tmp_id, index);
    } else if (transpiler_type_name_is_slice(array_type)) {
        char inner_buf[128];
        const char *inner = NULL;
        if (slot_inner_type_name_copy(array_type, inner_buf,
                sizeof(inner_buf))) {
            inner = inner_buf;
        }
        if (inner == NULL || inner[0] == '\0'
            || strcmp(inner, "Unknown") == 0) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "C slice access requires concrete Slice<T> element metadata");
            free(array);
            free(index);
            return NULL;
        }
        char slice_suffix[128];
        sanitize_c_suffix(inner, slice_suffix, sizeof(slice_suffix));
        int tmp_id = ++ctx->tmp_counter;
        result = strdup_fmt(
            "({ PgySlice_%s _pgy_slice_get_%d = %s; "
            "pgy_slice_get_%s(&_pgy_slice_get_%d, %s); })",
            slice_suffix, tmp_id, array, slice_suffix, tmp_id, index);
    } else {
        result = strdup_fmt("%s[%s]", array, index);
    }
    free(array);
    free(index);
    return result;
}
