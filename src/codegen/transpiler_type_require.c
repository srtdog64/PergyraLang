/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Internal C backend type requirement helpers.
 */

#include "transpiler_type_require.h"

#include <string.h>

#include "../common/string_compat.h"
#include "../semantic/diag_codes.h"
#include "codegen_channel_runtime_abi.h"
#include "transpiler_context.h"
#include "transpiler_type_mapping.h"
#include "transpiler_type_render.h"

bool
transpiler_require_ast_c_type_copy(TranspilerCtx *ctx,
                                   ASTNode *type_ast,
                                   const char *surface_desc,
                                   char *out,
                                   size_t out_size)
{
    if (out == NULL || out_size == 0)
        return false;
    out[0] = '\0';

    if (type_ast == NULL) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "cannot emit %s in C backend: missing explicit type",
            surface_desc != NULL ? surface_desc : "declaration");
        return false;
    }

    if (!pergyra_ast_type_to_c_copy_in_ctx(ctx, type_ast, out, out_size)) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "cannot lower %s to a bounded C type buffer",
            surface_desc != NULL ? surface_desc : "declaration");
        out[0] = '\0';
        return false;
    }
    if (out[0] == '\0' || strcmp(out, "Unknown") == 0) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "cannot lower %s to a concrete C type",
            surface_desc != NULL ? surface_desc : "declaration");
        out[0] = '\0';
        return false;
    }
    return true;
}

bool
transpiler_require_type_name_c_type_copy(TranspilerCtx *ctx,
                                         const char *type_name,
                                         const char *surface_desc,
                                         char *out,
                                         size_t out_size)
{
    if (out == NULL || out_size == 0)
        return false;
    out[0] = '\0';

    if (type_name == NULL || type_name[0] == '\0') {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "cannot emit %s in C backend: missing concrete type name",
            surface_desc != NULL ? surface_desc : "declaration");
        return false;
    }

    if (type_name[0] >= 'A' && type_name[0] <= 'Z' && type_name[1] == '\0') {
        if (!pergyra_str_copy(out, out_size, type_name)) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "cannot lower %s type '%s' to a concrete C type",
                surface_desc != NULL ? surface_desc : "declaration",
                type_name);
            out[0] = '\0';
            return false;
        }
        return true;
    }

    if (!pergyra_type_to_c_copy(type_name, out, out_size)
        || out[0] == '\0'
        || strcmp(out, "Unknown") == 0) {
        if (transpiler_type_name_is_channel(type_name)) {
            char inner[128];

            if (!slot_inner_type_name_copy(type_name, inner, sizeof(inner)))
                pergyra_str_copy(inner, sizeof(inner), "<unknown>");
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "C backend: %s type '%s' has no runtime Channel<%s> ABI; beta runtime supports %s",
                surface_desc != NULL ? surface_desc : "declaration",
                type_name,
                inner,
                pgy_channel_runtime_payload_supported_list());
            out[0] = '\0';
            return false;
        }
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "cannot lower %s type '%s' to a concrete C type",
            surface_desc != NULL ? surface_desc : "declaration",
            type_name);
        out[0] = '\0';
        return false;
    }

    return true;
}

bool
transpiler_copy_c_type_or_user_type_name(const char *type_name,
                                         char *out,
                                         size_t out_size)
{
    if (out == NULL || out_size == 0)
        return false;
    out[0] = '\0';

    if (type_name == NULL)
        return false;

    if (type_name[0] >= 'A' && type_name[0] <= 'Z' && type_name[1] == '\0')
        return pergyra_str_copy(out, out_size, type_name);

    if (pergyra_type_to_c_copy(type_name, out, out_size))
        return true;

    return pergyra_str_copy(out, out_size, type_name);
}
