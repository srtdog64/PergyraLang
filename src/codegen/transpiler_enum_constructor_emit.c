#include "transpiler_domain_constructor_emit.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../semantic/diag_codes.h"
#include "parser/ast_api.h"
#include "transpiler_context.h"
#include "transpiler_expr_type_infer.h"
#include "transpiler_format.h"

char *
transpiler_emit_enum_variant_constructor(ASTNode *call,
                                         const char *qualified_name,
                                         TranspilerCtx *ctx)
{
    size_t argc;
    char **arg_strs;
    size_t buf_len;
    char *result;

    if (call == NULL || qualified_name == NULL)
        return NULL;

    argc = ast_call_arg_count(call);
    if (argc > SIZE_MAX / sizeof(char *))
        return NULL;
    arg_strs = calloc(argc > 0 ? argc : 1, sizeof(char *));
    if (arg_strs == NULL)
        return NULL;
    for (size_t i = 0; i < argc; i++) {
        ASTNode *arg = ast_call_argument(call, i);
        const char *arg_type = transpiler_expr_infer_type_name(ctx, arg);
        if (arg_type != NULL && strcmp(arg_type, "Void") == 0) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_ALIGN_ARG_TYPE,
                "C enum variant constructor '%s' cannot consume a Void expression as payload %zu",
                qualified_name, i + 1);
            for (size_t j = 0; j < i; j++)
                free(arg_strs[j]);
            free(arg_strs);
            return pergyra_strdup("0");
        }
        arg_strs[i] = emit_expression(arg, ctx);
    }

    buf_len = strlen(qualified_name) + 3;
    for (size_t i = 0; i < argc; i++) {
        if (arg_strs[i] == NULL) {
            for (size_t j = 0; j < i; j++)
                free(arg_strs[j]);
            free(arg_strs);
            return NULL;
        }
        if (strlen(arg_strs[i]) > SIZE_MAX - buf_len - 2) {
            for (size_t j = 0; j <= i; j++)
                free(arg_strs[j]);
            for (size_t j = i + 1; j < argc; j++)
                free(arg_strs[j]);
            free(arg_strs);
            return NULL;
        }
        buf_len += strlen(arg_strs[i]) + 2;
    }

    result = malloc(buf_len);
    if (result == NULL) {
        for (size_t i = 0; i < argc; i++)
            free(arg_strs[i]);
        free(arg_strs);
        return NULL;
    }
    {
        size_t offset = 0;
        size_t qual_len = strlen(qualified_name);
        memcpy(result + offset, qualified_name, qual_len);
        offset += qual_len;
        result[offset++] = '(';
        for (size_t i = 0; i < argc; i++) {
            if (i > 0) {
                result[offset++] = ',';
                result[offset++] = ' ';
            }
            {
                size_t arg_len = strlen(arg_strs[i]);
                memcpy(result + offset, arg_strs[i], arg_len);
                offset += arg_len;
            }
            free(arg_strs[i]);
        }
        result[offset++] = ')';
        result[offset] = '\0';
    }
    free(arg_strs);
    return result;
}
