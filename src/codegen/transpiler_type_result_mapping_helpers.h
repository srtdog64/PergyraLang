#ifndef PGY_SRC_CODEGEN_TRANSPILER_TYPE_RESULT_MAPPING_HELPERS_H
#define PGY_SRC_CODEGEN_TRANSPILER_TYPE_RESULT_MAPPING_HELPERS_H

static bool
transpiler_result_suffix_from_type_name(const char *type_name,
                                        char *out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return false;
    }

    out[0] = '\0';
    if (type_name == NULL || strncmp(type_name, "Result<", 7) != 0) {
        return false;
    }

    const char *open = type_name + 7;
    const char *close = strrchr(type_name, '>');
    if (close == NULL || close <= open) {
        return false;
    }

    char inner[128];
    size_t n = (size_t)(close - open);
    if (n >= sizeof(inner))
        n = sizeof(inner) - 1;
    memcpy(inner, open, n);
    inner[n] = '\0';

    if (strchr(inner, ',') == NULL) {
        copy_capped_string(out, out_size, inner);
    } else {
        const char *sanitized = generic_args_to_c_suffix(inner);
        copy_capped_string(out, out_size, sanitized);
    }

    return out[0] != '\0';
}

static bool
transpiler_result_suffix_from_context(TranspilerCtx *ctx,
                                      char *out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return false;
    }

    out[0] = '\0';
    if (ctx == NULL) {
        return false;
    }

    if (ctx->expected_type != NULL
        && transpiler_result_suffix_from_type_name(ctx->expected_type,
                                                   out, out_size)) {
        return true;
    }

    if (ctx->current_return_type[0] != '\0'
        && transpiler_result_suffix_from_type_name(ctx->current_return_type,
                                                   out, out_size)) {
        return true;
    }

    return false;
}

static void
ensure_result_specialization_from_type_name_to(TranspilerCtx *ctx, CodeBuf *dst,
                                               const char *type_name)
{
    if (ctx == NULL || dst == NULL || type_name == NULL
        || strncmp(type_name, "Result<", 7) != 0) {
        return;
    }

    const char *open = type_name + 7;
    const char *close = strrchr(type_name, '>');
    if (close == NULL || close <= open) {
        return;
    }

    char inner[256];
    size_t n = (size_t)(close - open);
    if (n >= sizeof(inner))
        n = sizeof(inner) - 1;
    memcpy(inner, open, n);
    inner[n] = '\0';

    const char *comma = strchr(inner, ',');
    if (comma == NULL) {
        return;
    }

    char ok_type[128];
    char err_type[128];
    size_t ok_len = (size_t)(comma - inner);
    while (ok_len > 0
           && (inner[ok_len - 1] == ' ' || inner[ok_len - 1] == '\t')) {
        ok_len--;
    }
    if (ok_len >= sizeof(ok_type))
        ok_len = sizeof(ok_type) - 1;
    memcpy(ok_type, inner, ok_len);
    ok_type[ok_len] = '\0';

    const char *err_start = comma + 1;
    while (*err_start == ' ' || *err_start == '\t')
        err_start++;
    {
        size_t err_len = strlen(err_start);
        if (err_len >= sizeof(err_type))
            err_len = sizeof(err_type) - 1;
        memcpy(err_type, err_start, err_len);
        err_type[err_len] = '\0';
    }

    if (ok_type[0] == '\0' || err_type[0] == '\0') {
        return;
    }

    ensure_result_specialization_to(ctx, dst, ok_type, err_type);
}
#endif /* PGY_SRC_CODEGEN_TRANSPILER_TYPE_RESULT_MAPPING_HELPERS_H */
