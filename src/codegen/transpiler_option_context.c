/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend Option<T> contextual emission helpers.
 */

#include "transpiler_option_context.h"

#include "transpiler_context.h"
#include "transpiler_format.h"
#include "codegen_type_mapping.h"
#include "transpiler_type_require.h"

#include "../semantic/diag_codes.h"

#include <string.h>

const char *
transpiler_contextual_option_type_name(TranspilerCtx *ctx)
{
    if (ctx == NULL)
        return NULL;
    if (ctx->expected_type != NULL
        && transpiler_type_name_is_option(ctx->expected_type))
        return ctx->expected_type;
    if (ctx->current_return_type[0] != '\0'
        && transpiler_type_name_is_option(ctx->current_return_type))
        return ctx->current_return_type;
    return NULL;
}

bool
transpiler_contextual_option_inner_type_copy(TranspilerCtx *ctx,
                                             char *out,
                                             size_t out_size)
{
    const char *option_type = transpiler_contextual_option_type_name(ctx);
    char subst_buf[256];

    if (out == NULL || out_size == 0)
        return false;
    out[0] = '\0';
    if (option_type == NULL)
        return false;
    /* Inside a generic specialization window the contextual return type
     * may still read "Option<T>" — substitute active bindings so the
     * derived None_/Some_ suffix is concrete (bare `return None;` in a
     * generic body was the escapee; Some(x) flows through expr-infer). */
    option_type = transpiler_type_name_apply_generic_bindings(
        ctx, option_type, subst_buf, sizeof(subst_buf));
    return slot_inner_type_name_copy(option_type, out, out_size);
}

/* The registry (ensure_option_specialization_to) and the C type mapping
 * (PgyOption_<suffix>) both name an Option<T> specialization by the whole
 * inner type mangled with sanitize_c_suffix, so Option<Result<Int, String>>
 * is Result_Int_String. Some_/IsSome_/IsNone_/UnwrapOption_ and a return's
 * Some_/None_ take their suffix from here (None below mangles its contextual
 * inner type the same way); pasting the inner spelling itself gave
 * `IsSome_Result<Int, String>(o)`, which C reads as IsSome_Result. */
bool
transpiler_option_suffix_from_inner_type_name(const char *inner_type,
                                              char *out,
                                              size_t out_size)
{
    if (out == NULL || out_size == 0)
        return false;
    out[0] = '\0';
    if (inner_type == NULL || inner_type[0] == '\0'
        || strcmp(inner_type, "Unknown") == 0)
        return false;
    return sanitize_c_suffix(inner_type, out, out_size);
}

bool
transpiler_option_suffix_from_type_name(const char *option_type,
                                        char *out,
                                        size_t out_size)
{
    char inner[128];

    if (out == NULL || out_size == 0)
        return false;
    out[0] = '\0';
    if (!transpiler_type_name_is_option(option_type)
        || !slot_inner_type_name_copy(option_type, inner, sizeof(inner)))
        return false;
    return transpiler_option_suffix_from_inner_type_name(
        inner, out, out_size);
}

char *
transpiler_emit_none_with_context(TranspilerCtx *ctx, ASTNode *site)
{
    char inner_buf[128];
    char suffix[128];
    const char *inner = NULL;
    if (transpiler_contextual_option_inner_type_copy(ctx, inner_buf,
            sizeof(inner_buf))) {
        inner = inner_buf;
    }
    if (inner == NULL) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "None requires contextual Option<T> during C emission");
        (void)site;
        return NULL;
    }
    if (!sanitize_c_suffix(inner, suffix, sizeof(suffix))) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "None requires a C-safe concrete Option<T> suffix");
        (void)site;
        return NULL;
    }
    return strdup_fmt("None_%s()", suffix);
}
