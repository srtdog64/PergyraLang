#ifndef PGY_TRANSPILER_EXPR_ARRAY_ACCESS_EMIT_H
#define PGY_TRANSPILER_EXPR_ARRAY_ACCESS_EMIT_H

static char *
emit_array_access_expression(ASTNode *node, TranspilerCtx *ctx)
{
    ASTNode *array_node = ast_array_access_array(node);
    ASTNode *index_node = ast_array_access_index(node);
    char *array = emit_expression(array_node, ctx);
    char *index = emit_expression(index_node, ctx);
    const char *array_type =
        infer_expression_type_name(ctx, array_node);
    char *result;
    if (array_type != NULL && strncmp(array_type, "Array<", 6) == 0) {
        char inner_buf[128];
        const char *inner = NULL;
        if (slot_inner_type_name_copy(array_type, inner_buf,
                sizeof(inner_buf)))
            inner = inner_buf;
        if (inner == NULL || inner[0] == '\0'
            || strcmp(inner, "Unknown") == 0) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "C array access requires concrete Array<T> element metadata");
            free(array);
            free(index);
            return pergyra_strdup("0");
        }
        int tmp_id = ++ctx->tmp_counter;
        result = strdup_fmt(
            "({ PgyArray_%s _pgy_arr_get_%d = %s; "
            "pgy_array_get_%s(&_pgy_arr_get_%d, %s); })",
            inner, tmp_id, array, inner, tmp_id, index);
    } else if (array_type != NULL && strncmp(array_type, "Slice<", 6) == 0) {
        char inner_buf[128];
        const char *inner = NULL;
        if (slot_inner_type_name_copy(array_type, inner_buf,
                sizeof(inner_buf)))
            inner = inner_buf;
        if (inner == NULL || inner[0] == '\0'
            || strcmp(inner, "Unknown") == 0) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "C slice access requires concrete Slice<T> element metadata");
            free(array);
            free(index);
            return pergyra_strdup("0");
        }
        int tmp_id = ++ctx->tmp_counter;
        result = strdup_fmt(
            "({ PgySlice_%s _pgy_slice_get_%d = %s; "
            "pgy_slice_get_%s(&_pgy_slice_get_%d, %s); })",
            inner, tmp_id, array, inner, tmp_id, index);
    } else {
        result = strdup_fmt("%s[%s]", array, index);
    }
    free(array);
    free(index);
    return result;
}

#endif /* PGY_TRANSPILER_EXPR_ARRAY_ACCESS_EMIT_H */
